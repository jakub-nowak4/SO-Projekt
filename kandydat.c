#include "egzamin.h"

PamiecDzielona *pamiec_shm;

void kandydat_zakoncz(void);

int main()
{
    srand(time(NULL) ^ getpid());

    char msg_buffer[200];
    pid_t moj_pid = getpid();

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    int msqid_budynek = utworz_msq(klucz_msq_budynek);

    semafor_p(SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM);

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Ustawia sie w kolejce przed budynkiem.\n", moj_pid);
    loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

    bool moja_matura = losuj_czy_zdal_matura();
    bool czy_powtarzam = moja_matura ? losuj_czy_powtarza_egzamin() : false;
    float moj_wynik_a = czy_powtarzam ? (float)(rand() % 71 + 30) : -1.0f;

    MSG_ZGLOSZENIE zgloszenie;
    zgloszenie.mtype = KANDYDAT_PRZESYLA_MATURE;
    zgloszenie.pid = moj_pid;
    zgloszenie.matura = moja_matura;
    zgloszenie.czy_powtarza_egzamin = czy_powtarzam;
    zgloszenie.wynik_a = moj_wynik_a;

    msq_send(msqid_budynek, &zgloszenie, sizeof(zgloszenie));

    MSG_DECYZJA decyzja;
    msq_receive(msqid_budynek, &decyzja, sizeof(decyzja), MTYPE_BUDYNEK_DECYZJA + moj_pid);

    if (decyzja.numer_na_liscie == -1)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem negatywna decyzje od dziekana i koncze udzial w egzaminie.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
        kandydat_zakoncz();
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem decyzje od dziekana ustawiam sie w kolejce do komisji A.\n", moj_pid);
    loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    int msqid_A = utworz_msq(klucz_msq_A);

    while (true)
    {
        bool moja_kolej = false;
        MSG_KANDYDAT_WCHODZI_DO_A zgloszenia_A;

        semafor_p(SEMAFOR_MUTEX);

        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int aktualny = pamiec_shm->nastepny_do_komisja_A;

        if (!egzamin_trwa)
        {
            semafor_v(SEMAFOR_MUTEX);
            break;
        }

        if (aktualny == decyzja.numer_na_liscie)
        {
            moja_kolej = true;
            zgloszenia_A.mtype = KANDYDAT_WCHODZI_DO_A;
            zgloszenia_A.numer_na_liscie = decyzja.numer_na_liscie;
            zgloszenia_A.pid = moj_pid;
            pamiec_shm->nastepny_do_komisja_A++;
        }

        semafor_v(SEMAFOR_MUTEX);

        if (moja_kolej)
        {
            msq_send(msqid_A, &zgloszenia_A, sizeof(zgloszenia_A));

            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Czekam przed drzwiami Komisji A...\n", moj_pid);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

            MSG_KANDYDAT_WCHODZI_DO_A_POTWIERDZENIE potwierdzenie;
            msq_receive(msqid_A, &potwierdzenie, sizeof(potwierdzenie), MTYPE_A_POTWIERDZENIE + moj_pid);

            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Wszedlem do sali A\n", moj_pid);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
            break;
        }

        // usleep(10000);
    }

    bool czy_musze_zdawac = false;
    bool czy_ide_do_B = false;

    if (czy_powtarzam)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Mam zdana czesc teorytyczna egzaminu. Czekam na weryfikacje od nadzorcy Komisji A.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        MSG_KANDYDAT_POWTARZA weryfikacja;
        weryfikacja.mtype = NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO;
        weryfikacja.pid = moj_pid;
        weryfikacja.wynik_a = moj_wynik_a;

        msq_send(msqid_A, &weryfikacja, sizeof(weryfikacja));

        MSG_KANDYDAT_POWTARZA_ODPOWIEDZ_NADZORCY weryfikacja_odpowiedz;
        msq_receive(msqid_A, &weryfikacja_odpowiedz, sizeof(weryfikacja_odpowiedz), MTYPE_A_WERYFIKACJA_ODP + moj_pid);

        if (weryfikacja_odpowiedz.zgoda)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Nadzorca Komisji A uznaje moj wynik z egazminu. Ustawiam sie w kolejce do Komisji B.\n", moj_pid);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
            czy_ide_do_B = true;
        }
    }
    else
    {
        czy_musze_zdawac = true;
    }

    if (czy_musze_zdawac)
    {
        // Kandydat wysla do nadzorcy komunikat ze jest gotowy na zadawanie pytan co skutkuje ustawieniem flagi gotowosci w komisji
        MSG_KANDYDAT_GOTOWY gotowy;
        gotowy.mtype = KANDYDAT_GOTOWY_A;
        gotowy.pid = moj_pid;
        msq_send(msqid_A, &gotowy, sizeof(gotowy));

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Czekam na otrzymanie wszytskich pytan od czlonkow Komisji A.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
        {
            MSG_PYTANIE pytanie;
            msq_receive(msqid_A, &pytanie, sizeof(pytanie), MTYPE_A_PYTANIE + moj_pid);
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wszystkie pytania od Komisji A. Zaczynam opracowywac odpowiedzi.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        // usleep(rand() % 1000000);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Opracowalem pytania od komisji A. Czekam az bede mogl odpowiadac.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        semafor_p(SEMAFOR_ODPOWIEDZ_A);

        for (int i = 1; i <= LICZBA_CZLONKOW_A; i++)
        {
            MSG_ODPOWIEDZ odpowiedz;
            odpowiedz.mtype = i;
            odpowiedz.pid = moj_pid;
            msq_send(msqid_A, &odpowiedz, sizeof(odpowiedz));
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Udzieliem odpowiedzi na wszytskie pytania Komisji A. Czekam na wyniki za czesc teorytyczna egzaminu.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
        {
            MSG_WYNIK wynik;
            msq_receive(msqid_A, &wynik, sizeof(wynik), MTYPE_A_WYNIK + moj_pid);
        }

        MSG_WYNIK_KONCOWY wynik_koncowy;
        msq_receive(msqid_A, &wynik_koncowy, sizeof(wynik_koncowy), MTYPE_A_WYNIK_KONCOWY + moj_pid);

        semafor_v(SEMAFOR_ODPOWIEDZ_A);

        czy_ide_do_B = wynik_koncowy.czy_zdal;
        if (wynik_koncowy.czy_zdal)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wynik koncowy za czesc teorytyczna egzaminu: %.2f. Ustawiam sie w kolejsce do Komisji B.\n", moj_pid, wynik_koncowy.wynik_koncowy);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
        }
        else
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wynik koncowy za czesc teorytyczna egzaminu: %.2f. Moj wynik jest za niski aby przystapic do kolejnego etapu egzaminu.\n", moj_pid, wynik_koncowy.wynik_koncowy);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
            kandydat_zakoncz();
        }
    }

    if (czy_ide_do_B)
    {
        key_t klucz_msq_B = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_B);
        int msqid_B = utworz_msq(klucz_msq_B);

        MSG_KANDYDAT_WCHODZI_DO_B zgloszenie_B;
        zgloszenie_B.mtype = KANDYDAT_WCHODZI_DO_B;
        zgloszenie_B.numer_na_liscie = decyzja.numer_na_liscie;
        zgloszenie_B.pid = moj_pid;

        msq_send(msqid_B, &zgloszenie_B, sizeof(zgloszenie_B));

        MSG_KANDYDAT_WCHODZI_DO_B_POTWIERDZENIE potwierdzenie_wejscia;
        msq_receive(msqid_B, &potwierdzenie_wejscia, sizeof(potwierdzenie_wejscia), MTYPE_B_POTWIERDZENIE + moj_pid);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Wchodze na czesc praktyczna egzaminu do Komisji B.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        // Kandydat wysla do nadzorcy komunikat ze jest gotowy na zadawanie pytan co skutkuje ustawieniem flagi gotowosci w komisji - jak w A
        MSG_KANDYDAT_GOTOWY gotowy_B;
        gotowy_B.mtype = KANDYDAT_GOTOWY_B;
        gotowy_B.pid = moj_pid;
        msq_send(msqid_B, &gotowy_B, sizeof(gotowy_B));

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Czekam na otrzymanie wszytskich pytan od czlonkow Komisji B.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            MSG_PYTANIE pytanie_B;
            msq_receive(msqid_B, &pytanie_B, sizeof(pytanie_B), MTYPE_B_PYTANIE + moj_pid);
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wszystkie pytania od Komisji B. Zaczynam opracowywac odpowiedzi.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        // usleep(rand() % 500000);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Opracowalem pytania od komisji B. Czekam az bede mogl odpowiadac.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        semafor_p(SEMAFOR_ODPOWIEDZ_B);

        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            MSG_ODPOWIEDZ odpowiedz_B;
            odpowiedz_B.pid = moj_pid;
            odpowiedz_B.mtype = i + 1;
            msq_send(msqid_B, &odpowiedz_B, sizeof(odpowiedz_B));
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Udzieliem odpowiedzi na wszytskie pytania Komisji B. Czekam na wyniki za czesc praktyczna egzaminu.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            MSG_WYNIK wynik_B;
            msq_receive(msqid_B, &wynik_B, sizeof(wynik_B), MTYPE_B_WYNIK + moj_pid);
        }

        MSG_WYNIK_KONCOWY wynik_koncowy_B;
        msq_receive(msqid_B, &wynik_koncowy_B, sizeof(wynik_koncowy_B), MTYPE_B_WYNIK_KONCOWY + moj_pid);

        semafor_v(SEMAFOR_ODPOWIEDZ_B);

        if (wynik_koncowy_B.czy_zdal)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wynik koncowy za czesc praktyczna egzaminu: %.2f. Ide do domu i czekam na liste rankingowa.\n", moj_pid, wynik_koncowy_B.wynik_koncowy);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
        }
        else
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wynik koncowy za czesc praktyczna egzaminu: %.2f. Moj wynik jest za niski - koncze egzamin.\n", moj_pid, wynik_koncowy_B.wynik_koncowy);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
        }
    }

    kandydat_zakoncz();

    return 0;
}

void kandydat_zakoncz(void)
{
    if (pamiec_shm == NULL)
    {
        const char *msg = "[KANDYDAT] Blad: pamiec_shm == NULL\n";
        write(STDERR_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
    }

    semafor_p(SEMAFOR_MUTEX);
    pamiec_shm->kandydatow_procesow--;
    if (pamiec_shm->kandydatow_procesow == 0)
    {
        pamiec_shm->egzamin_trwa = false;
    }
    semafor_v(SEMAFOR_MUTEX);

    odlacz_shm(pamiec_shm);
    semafor_v(SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM);

    exit(EXIT_SUCCESS);
}