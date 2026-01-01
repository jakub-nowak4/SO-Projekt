#include "egzamin.h"

int main()
{

    srand(time(NULL) ^ getpid());

    char msg_buffer[200];
    PamiecDzielona *pamiec_shm;

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    int msqid_budynek = utworz_msq(klucz_msq_budynek);

    Kandydat kandydat;
    init_kandydat(getpid(), &kandydat);

    // Kandydat ustawia sie przed budynkiem

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Ustawia sie w kolejce przed budynkiem.\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    MSG_ZGLOSZENIE zgloszenie;
    zgloszenie.mtype = KANDYDAT_PRZESYLA_MATURE;
    zgloszenie.kandydat = kandydat;
    msq_send(msqid_budynek, &zgloszenie, sizeof(zgloszenie));

    // Kandydat czeka na decyzje dziekana
    MSG_DECYZJA decyzja;
    msq_receive(msqid_budynek, &decyzja, sizeof(decyzja), getpid());

    MSG_POTWIERDZENIE potwierdzenie;

    if (decyzja.numer_na_liscie == -1)
    {
        potwierdzenie.mtype = getpid();
        potwierdzenie.pid = getpid();
        msq_send(msqid_budynek, &potwierdzenie, sizeof(potwierdzenie));

        semafor_p(SEMAFOR_MUTEX);
        pamiec_shm->pozostalo_kandydatow--;
        if (pamiec_shm->pozostalo_kandydatow == 0)
        {
            pamiec_shm->egzamin_trwa = false;
        }

        semafor_v(SEMAFOR_MUTEX);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem decyzje od dziekana i koncze udzial w egzaminie.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        odlacz_shm(pamiec_shm);
        exit(EXIT_SUCCESS);
    }
    else
    {
        potwierdzenie.mtype = getpid();
        potwierdzenie.pid = getpid();

        msq_send(msqid_budynek, &potwierdzenie, sizeof(potwierdzenie));
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem decyzje od dziekana ustawiam sie w kolejce do komisji A.\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // KOMUNIKACJA Z KOMISJA A

    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    int msqid_A = utworz_msq(klucz_msq_A);

    while (true)
    {
        bool moge_wejsc = false;

        semafor_p(SEMAFOR_MUTEX);

        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int aktualny = pamiec_shm->nastepny_do_komisja_A;
        int osoby = pamiec_shm->liczba_osob_w_A;

        if (!egzamin_trwa)
        {
            semafor_v(SEMAFOR_MUTEX);
            break;
        }

        if (aktualny == decyzja.numer_na_liscie && osoby < 3)
        {
            pamiec_shm->nastepny_do_komisja_A++;
            pamiec_shm->liczba_osob_w_A++;
            moge_wejsc = true;
        }

        semafor_v(SEMAFOR_MUTEX);

        if (moge_wejsc)
        {
            // Wysyla komunikat do nadzorcy komisji A

            MSG_KANDYDAT_WCHODZI_DO_A zgloszenia_A;
            zgloszenia_A.mtype = KANDYDAT_WCHODZI_DO_A;
            zgloszenia_A.numer_na_liscie = decyzja.numer_na_liscie;
            zgloszenia_A.pid = getpid();
            msq_send(msqid_A, &zgloszenia_A, sizeof(zgloszenia_A));

            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Wchodze na czesc teorytyczna egzaminu do Komisji A.\n", getpid());
            wypisz_wiadomosc(msg_buffer);

            break;
        }

        usleep(1000);
    }

    bool czy_musze_zdawac = false;
    bool czy_ide_do_B = false;

    if (kandydat.czy_powtarza_egzamin)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Mam zdana czesc teorytyczna egzaminu. Czekam na weryfikacje od nadzorcy Komisji A.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        MSG_KANDYDAT_POWTARZA weryfikacja;
        weryfikacja.mtype = NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO;
        weryfikacja.pid = getpid();
        weryfikacja.wynika_a = kandydat.wynik_a;

        msq_send(msqid_A, &weryfikacja, sizeof(weryfikacja));

        MSG_KANDYDAT_POWTARZA_ODPOWIEDZ_NADZORCY weryfikacja_odpowiedz;
        msq_receive(msqid_A, &weryfikacja_odpowiedz, sizeof(weryfikacja_odpowiedz), getpid());

        if (weryfikacja_odpowiedz.zgoda)
        {

            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Nadzorca Komisji A uznaje moj wynik z egazminu. Ustawiam sie w kolejce do Komisji B.\n", getpid());
            wypisz_wiadomosc(msg_buffer);
            czy_ide_do_B = true;
        }
    }
    else
    {
        czy_musze_zdawac = true;
    }

    if (czy_musze_zdawac)
    {
        // Czekam na pytania od komisji A
        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Czekam na otrzymanie wszytskich pytan od czlonkow Komisji A.\n", getpid());
        wypisz_wiadomosc(msg_buffer);
        for (int i = 0; i < 5; i++)
        {
            MSG_PYTANIE pytanie;
            msq_receive(msqid_A, &pytanie, sizeof(pytanie), getpid());
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wszystkie pytania od Komisji A. Zaczynam opracowywac odpowiedzi.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        usleep(rand() % 3000);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Opracowalem pytania od komisji A. Czekam az bede mogl odpowiadac.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        semafor_p(SEMAFOR_ODPOWIEDZ_A);
        for (int i = 1; i <= 5; i++)
        {
            MSG_ODPOWIEDZ odpowiedz;
            odpowiedz.mtype = i;
            odpowiedz.pid = getpid();

            msq_send(msqid_A, &odpowiedz, sizeof(odpowiedz));
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Udzieliem odpowiedzi na wszytskie pytania Komisji A. Czekam na wyniki za czesc teorytyczna egzaminu.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        for (int i = 0; i < 5; i++)
        {
            MSG_WYNIK wynik;
            msq_receive(msqid_A, &wynik, sizeof(wynik), getpid());
        }

        MSG_WYNIK_KONCOWY wynik_koncowy;
        msq_receive(msqid_A, &wynik_koncowy, sizeof(wynik_koncowy), getpid());
        semafor_v(SEMAFOR_ODPOWIEDZ_A);

        czy_ide_do_B = wynik_koncowy.czy_zdal;
        if (wynik_koncowy.czy_zdal)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wynik koncowy za czesc teorytyczna egzaminu: %.2f. Ustawiam sie w kolejsce do Komisji B.\n", getpid(), wynik_koncowy.wynik_koncowy);
            wypisz_wiadomosc(msg_buffer);
        }
        else
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wynik koncowy za czesc teorytyczna egzaminu: %.2f. Moj wynik jest za niski aby przystapic do kolejnego etapu egzaminu.\n", getpid(), wynik_koncowy.wynik_koncowy);
            wypisz_wiadomosc(msg_buffer);

            semafor_p(SEMAFOR_MUTEX);
            pamiec_shm->pozostalo_kandydatow--;
            if (pamiec_shm->pozostalo_kandydatow == 0)
            {
                pamiec_shm->egzamin_trwa = false;
            }
            semafor_v(SEMAFOR_MUTEX);

            odlacz_shm(pamiec_shm);

            exit(EXIT_SUCCESS);
        }
    }

    // Kandydat ustawia sie w kolejce do komisji B

    if (czy_ide_do_B)
    {

        key_t klucz_msq_B = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_B);
        int msqid_B = utworz_msq(klucz_msq_B);

        MSG_KANDYDAT_WCHODZI_DO_B zgloszenie_B;
        zgloszenie_B.mtype = KANDYDAT_WCHODZI_DO_B;
        zgloszenie_B.numer_na_liscie = decyzja.numer_na_liscie;
        zgloszenie_B.pid = getpid();

        msq_send(msqid_B, &zgloszenie_B, sizeof(zgloszenie_B));

        MSG_KANDYDAT_WCHODZI_DO_B_POTWIERDZENIE potwierdzenie_wejscia;
        msq_receive(msqid_B, &potwierdzenie_wejscia, sizeof(potwierdzenie_wejscia), getpid());

        semafor_p(SEMAFOR_MUTEX);
        pamiec_shm->liczba_osob_w_B++;
        semafor_v(SEMAFOR_MUTEX);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Wchodze na czesc praktyczna egzaminu do Komisji B.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Czekam na otrzymanie wszytskich pytan od czlonkow Komisji B.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        MSG_PYTANIE pytanie_B;
        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            msq_receive(msqid_B, &pytanie_B, sizeof(pytanie_B), getpid());
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wszystkie pytania od Komisji B. Zaczynam opracowywac odpowiedzi.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        usleep(rand() % 3000);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Opracowalem pytania od komisji B. Czekam az bede mogl odpowiadac.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        semafor_p(SEMAFOR_ODPOWIEDZ_B);
        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            MSG_ODPOWIEDZ odpowiedz_B;
            odpowiedz_B.pid = getpid();
            odpowiedz_B.mtype = i + 1;
            msq_send(msqid_B, &odpowiedz_B, sizeof(odpowiedz_B));
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Udzieliem odpowiedzi na wszytskie pytania Komisji B. Czekam na wyniki za czesc teorytyczna egzaminu.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            MSG_WYNIK wynik_B;
            msq_receive(msqid_B, &wynik_B, sizeof(wynik_B), getpid());
        }

        MSG_WYNIK_KONCOWY wynik_koncowy_B;
        msq_receive(msqid_B, &wynik_koncowy_B, sizeof(wynik_koncowy_B), getpid());

        semafor_v(SEMAFOR_ODPOWIEDZ_B);

        if (wynik_koncowy_B.czy_zdal)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wynik koncowy za czesc praktyczna egzaminu: %.2f. Czekam az Dziekan oglosi liste rankingowa.\n", getpid(), wynik_koncowy_B.wynik_koncowy);
            wypisz_wiadomosc(msg_buffer);
        }
        else
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wynik koncowy za czesc praktyczna egzaminu: %.2f. Moj wynik jest za niski - koncze egzamin.\n", getpid(), wynik_koncowy_B.wynik_koncowy);
            wypisz_wiadomosc(msg_buffer);

            semafor_p(SEMAFOR_MUTEX);
            pamiec_shm->pozostalo_kandydatow--;
            if (pamiec_shm->pozostalo_kandydatow == 0)
            {
                pamiec_shm->egzamin_trwa = false;
            }
            semafor_v(SEMAFOR_MUTEX);

            odlacz_shm(pamiec_shm);

            exit(EXIT_SUCCESS);
        }
    }

    semafor_p(SEMAFOR_MUTEX);
    pamiec_shm->pozostalo_kandydatow--;
    if (pamiec_shm->pozostalo_kandydatow == 0)
    {
        pamiec_shm->egzamin_trwa = false;
    }
    semafor_v(SEMAFOR_MUTEX);

    odlacz_shm(pamiec_shm);

    return 0;
}