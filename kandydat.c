#include "egzamin.h"

PamiecDzielona *pamiec_shm;
volatile sig_atomic_t ewakuacja_aktywna = false;
volatile sig_atomic_t w_budynku = false;
int global_numer_na_liscie = -1;

void kandydat_zakoncz(void);
void handler_sigterm(int sigNum);

int main()
{
    // SIGTERM
    struct sigaction sa;
    sa.sa_handler = handler_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // bez SA_RESTART aby przerwac msqrcb
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction(SIGTERM) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGINT) | Nie udalo sie zignorowac SIGINT.");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL) ^ getpid());

    char msg_buffer[200];
    pid_t moj_pid = getpid();

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    if (pamiec_shm->ewakuacja)
    {
        ewakuacja_aktywna = true;
        kandydat_zakoncz();
    }

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    int msqid_budynek = utworz_msq(klucz_msq_budynek);

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Ustawiam sie w kolejce przed budynkiem.\n", moj_pid);
    loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

    // usleep(CZAS_OCZEKIWANIA_W_KOLEJCE_MIN + rand() % (CZAS_OCZEKIWANIA_W_KOLEJCE_MAX - CZAS_OCZEKIWANIA_W_KOLEJCE_MIN));

    if (semafor_p(SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM) == -1)
    {
        kandydat_zakoncz();
    }

    w_budynku = true;

    if (ewakuacja_aktywna)
    {
        kandydat_zakoncz();
    }

    bool moja_matura = losuj_czy_zdal_matura();
    bool czy_powtarzam = moja_matura ? losuj_czy_powtarza_egzamin() : false;
    float moj_wynik_a = czy_powtarzam ? (float)(rand() % 71 + 30) : -1.0f;

    MSG_ZGLOSZENIE zgloszenie;
    zgloszenie.mtype = KANDYDAT_PRZESYLA_MATURE;
    zgloszenie.pid = moj_pid;
    zgloszenie.matura = moja_matura;
    zgloszenie.czy_powtarza_egzamin = czy_powtarzam;
    zgloszenie.wynik_a = moj_wynik_a;

    zgloszenie.wynik_a = moj_wynik_a;

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Wysylam zgloszenie (wpisuje sie na liste) i czekam na decyzje od Dziekana.\n", moj_pid);
    loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

    if (msq_send(msqid_budynek, &zgloszenie, sizeof(zgloszenie)) == -1)
    {
        kandydat_zakoncz();
    }

    MSG_DECYZJA decyzja;
    ssize_t res = msq_receive(msqid_budynek, &decyzja, sizeof(decyzja), MTYPE_BUDYNEK_DECYZJA + moj_pid);

    if (res == -1 || ewakuacja_aktywna)
    {
        kandydat_zakoncz();
    }

    if (decyzja.numer_na_liscie == -1)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Otrzymalem negatywna decyzje od Dziekana (niezdana matura) - koncze udzial w egzaminie.\n", moj_pid);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
        kandydat_zakoncz();
    }

    global_numer_na_liscie = decyzja.numer_na_liscie;

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Otrzymalem pozytywna decyzje od Dziekana - ustawiam sie w kolejce do Komisji A.\n", moj_pid, decyzja.numer_na_liscie);
    loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    int msqid_A = utworz_msq(klucz_msq_A);

    while (true)
    {
        if (ewakuacja_aktywna)
        {
            kandydat_zakoncz();
        }

        bool moja_kolej = false;
        MSG_KANDYDAT_WCHODZI_DO_A zgloszenia_A;

        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            kandydat_zakoncz();
        }

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
            if (msq_send(msqid_A, &zgloszenia_A, sizeof(zgloszenia_A)) == -1)
            {
                kandydat_zakoncz();
            }

            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Czekam przed drzwiami Komisji A...\n", moj_pid, decyzja.numer_na_liscie);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

            MSG_KANDYDAT_WCHODZI_DO_A_POTWIERDZENIE potwierdzenie;
            res = msq_receive(msqid_A, &potwierdzenie, sizeof(potwierdzenie), MTYPE_A_POTWIERDZENIE + moj_pid);

            if (res == -1 || ewakuacja_aktywna)
            {
                kandydat_zakoncz();
            }

            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Wszedlem do sali A.\n", moj_pid, decyzja.numer_na_liscie);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
            break;
        }
    }

    if (ewakuacja_aktywna)
    {
        kandydat_zakoncz();
    }

    if (semafor_p(SEMAFOR_MUTEX) == -1)
    {
        kandydat_zakoncz();
    }
    bool egzamin_nadal_trwa = pamiec_shm->egzamin_trwa;
    semafor_v(SEMAFOR_MUTEX);

    if (!egzamin_nadal_trwa)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Egzamin nie trwa - opuszczam budynek.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
        kandydat_zakoncz();
    }

    bool czy_musze_zdawac = false;
    bool czy_ide_do_B = false;

    if (czy_powtarzam)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Mam zdana czesc teoretyczna egzaminu. Czekam na weryfikacje od Nadzorcy Komisji A.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        MSG_KANDYDAT_POWTARZA weryfikacja;
        weryfikacja.mtype = NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO;
        weryfikacja.pid = moj_pid;
        weryfikacja.wynik_a = moj_wynik_a;

        if (msq_send(msqid_A, &weryfikacja, sizeof(weryfikacja)) == -1)
        {
            kandydat_zakoncz();
        }

        MSG_KANDYDAT_POWTARZA_ODPOWIEDZ_NADZORCY weryfikacja_odpowiedz;
        res = msq_receive(msqid_A, &weryfikacja_odpowiedz, sizeof(weryfikacja_odpowiedz), MTYPE_A_WERYFIKACJA_ODP + moj_pid);

        if (res == -1 || ewakuacja_aktywna)
        {
            kandydat_zakoncz();
        }

        if (weryfikacja_odpowiedz.zgoda)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Nadzorca Komisji A uznaje moj wynik z egzaminu. Ustawiam sie w kolejce do Komisji B.\n", moj_pid, decyzja.numer_na_liscie);
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
        MSG_KANDYDAT_GOTOWY gotowy;
        gotowy.mtype = KANDYDAT_GOTOWY_A;
        gotowy.pid = moj_pid;
        if (msq_send(msqid_A, &gotowy, sizeof(gotowy)) == -1)
        {
            kandydat_zakoncz();
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Jestem gotowy do odbierania pytan od Komisji A.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Czekam na otrzymanie wszystkich pytan od czlonkow Komisji A.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
        {
            MSG_PYTANIE pytanie;
            res = msq_receive(msqid_A, &pytanie, sizeof(pytanie), MTYPE_A_PYTANIE + moj_pid);
            if (res == -1 || ewakuacja_aktywna)
            {
                kandydat_zakoncz();
            }
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Otrzymalem wszystkie pytania od Komisji A. Zaczynam opracowywac odpowiedzi.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        // usleep(CZAS_PRZYGOTOWANIA_ODPOWIEDZI_MIN + rand() % (CZAS_PRZYGOTOWANIA_ODPOWIEDZI_MAX - CZAS_PRZYGOTOWANIA_ODPOWIEDZI_MIN));

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Opracowalem pytania od Komisji A. Czekam az bede mogl odpowiadac.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        if (semafor_p(SEMAFOR_ODPOWIEDZ_A) == -1)
        {
            kandydat_zakoncz();
        }

        for (int i = 1; i <= LICZBA_CZLONKOW_A; i++)
        {
            MSG_ODPOWIEDZ odpowiedz;
            odpowiedz.mtype = i;
            odpowiedz.pid = moj_pid;
            if (msq_send(msqid_A, &odpowiedz, sizeof(odpowiedz)) == -1)
            {
                semafor_v(SEMAFOR_ODPOWIEDZ_A);
                kandydat_zakoncz();
            }
            // usleep(CZAS_UDZIELANIA_ODPOWIEDZI_MIN + rand() % (CZAS_UDZIELANIA_ODPOWIEDZI_MAX - CZAS_UDZIELANIA_ODPOWIEDZI_MIN));
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Udzielilem odpowiedzi na wszystkie pytania Komisji A. Czekam na wyniki za czesc teoretyczna egzaminu.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
        {
            MSG_WYNIK wynik;
            res = msq_receive(msqid_A, &wynik, sizeof(wynik), MTYPE_A_WYNIK + moj_pid);
            if (res == -1 || ewakuacja_aktywna)
            {
                semafor_v(SEMAFOR_ODPOWIEDZ_A);
                kandydat_zakoncz();
            }
        }

        MSG_WYNIK_KONCOWY wynik_koncowy;
        res = msq_receive(msqid_A, &wynik_koncowy, sizeof(wynik_koncowy), MTYPE_A_WYNIK_KONCOWY + moj_pid);

        semafor_v(SEMAFOR_ODPOWIEDZ_A);

        if (res == -1 || ewakuacja_aktywna)
        {
            kandydat_zakoncz();
        }

        czy_ide_do_B = wynik_koncowy.czy_zdal;
        if (wynik_koncowy.czy_zdal)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Otrzymalem wynik koncowy za czesc teoretyczna egzaminu: %.2f. Ustawiam sie w kolejce do Komisji B.\n", moj_pid, decyzja.numer_na_liscie, wynik_koncowy.wynik_koncowy);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
        }
        else
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Otrzymalem wynik koncowy za czesc teoretyczna egzaminu: %.2f. Moj wynik jest za niski aby przystapic do kolejnego etapu egzaminu.\n", moj_pid, decyzja.numer_na_liscie, wynik_koncowy.wynik_koncowy);
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

        if (msq_send(msqid_B, &zgloszenie_B, sizeof(zgloszenie_B)) == -1)
        {
            kandydat_zakoncz();
        }

        MSG_KANDYDAT_WCHODZI_DO_B_POTWIERDZENIE potwierdzenie_wejscia;
        res = msq_receive(msqid_B, &potwierdzenie_wejscia, sizeof(potwierdzenie_wejscia), MTYPE_B_POTWIERDZENIE + moj_pid);

        if (res == -1 || ewakuacja_aktywna)
        {
            kandydat_zakoncz();
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Wchodze na czesc praktyczna egzaminu do Komisji B.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        MSG_KANDYDAT_GOTOWY gotowy_B;
        gotowy_B.mtype = KANDYDAT_GOTOWY_B;
        gotowy_B.pid = moj_pid;
        if (msq_send(msqid_B, &gotowy_B, sizeof(gotowy_B)) == -1)
        {
            kandydat_zakoncz();
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Jestem gotowy do odbierania pytan od Komisji B.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Czekam na otrzymanie wszystkich pytan od czlonkow Komisji B.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            MSG_PYTANIE pytanie_B;
            res = msq_receive(msqid_B, &pytanie_B, sizeof(pytanie_B), MTYPE_B_PYTANIE + moj_pid);
            if (res == -1 || ewakuacja_aktywna)
            {
                kandydat_zakoncz();
            }
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Otrzymalem wszystkie pytania od Komisji B. Zaczynam opracowywac odpowiedzi.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        // usleep(CZAS_PRZYGOTOWANIA_ODPOWIEDZI_MIN + rand() % (CZAS_PRZYGOTOWANIA_ODPOWIEDZI_MAX - CZAS_PRZYGOTOWANIA_ODPOWIEDZI_MIN));

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Opracowalem pytania od Komisji B. Czekam az bede mogl odpowiadac.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        if (semafor_p(SEMAFOR_ODPOWIEDZ_B) == -1)
        {
            kandydat_zakoncz();
        }

        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            MSG_ODPOWIEDZ odpowiedz_B;
            odpowiedz_B.pid = moj_pid;
            odpowiedz_B.mtype = i + 1;
            if (msq_send(msqid_B, &odpowiedz_B, sizeof(odpowiedz_B)) == -1)
            {
                semafor_v(SEMAFOR_ODPOWIEDZ_B);
                kandydat_zakoncz();
            }
            // usleep(CZAS_UDZIELANIA_ODPOWIEDZI_MIN + rand() % (CZAS_UDZIELANIA_ODPOWIEDZI_MAX - CZAS_UDZIELANIA_ODPOWIEDZI_MIN));
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Udzielilem odpowiedzi na wszystkie pytania Komisji B. Czekam na wyniki za czesc praktyczna egzaminu.\n", moj_pid, decyzja.numer_na_liscie);
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

        for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
        {
            MSG_WYNIK wynik_B;
            res = msq_receive(msqid_B, &wynik_B, sizeof(wynik_B), MTYPE_B_WYNIK + moj_pid);
            if (res == -1 || ewakuacja_aktywna)
            {
                semafor_v(SEMAFOR_ODPOWIEDZ_B);
                kandydat_zakoncz();
            }
        }

        MSG_WYNIK_KONCOWY wynik_koncowy_B;
        res = msq_receive(msqid_B, &wynik_koncowy_B, sizeof(wynik_koncowy_B), MTYPE_B_WYNIK_KONCOWY + moj_pid);

        semafor_v(SEMAFOR_ODPOWIEDZ_B);

        if (res == -1 || ewakuacja_aktywna)
        {
            kandydat_zakoncz();
        }

        if (wynik_koncowy_B.czy_zdal)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Otrzymalem wynik koncowy za czesc praktyczna egzaminu: %.2f. Ide do domu i czekam na liste rankingowa.\n", moj_pid, decyzja.numer_na_liscie, wynik_koncowy_B.wynik_koncowy);
            loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
        }
        else
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Otrzymalem wynik koncowy za czesc praktyczna egzaminu: %.2f. Moj wynik jest za niski - koncze egzamin.\n", moj_pid, decyzja.numer_na_liscie, wynik_koncowy_B.wynik_koncowy);
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
        const char *msg = "[Kandydat] Blad: pamiec_shm == NULL\n";
        (void)write(STDERR_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
    }

    if (ewakuacja_aktywna)
    {
        char msg_buffer[128];
        if (global_numer_na_liscie != -1)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Ewakuacja - opuszczam budynek.\n", getpid(), global_numer_na_liscie);
        }
        else
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Ewakuacja - opuszczam budynek.\n", getpid());
        }
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
    }
    else
    {
        char msg_buffer[128];
        if (global_numer_na_liscie != -1)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Koncze egzamin - wypisuje sie.\n", getpid(), global_numer_na_liscie);
        }
        else
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Koncze egzamin - wypisuje sie.\n", getpid());
        }
        loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);
    }

    int pozostalo = -1;
    if (semafor_p(SEMAFOR_MUTEX) == 0)
    {
        pamiec_shm->kandydatow_procesow--;
        pozostalo = pamiec_shm->kandydatow_procesow;

        if (pozostalo == 0)
        {
            pamiec_shm->egzamin_trwa = false;
        }
        semafor_v(SEMAFOR_MUTEX);
    }

    char msg_buffer[256];
    if (global_numer_na_liscie != -1)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Nr:%d | Koncze prace. Pozostalo procesow: %d\n", getpid(), global_numer_na_liscie, pozostalo);
    }
    else
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Koncze prace. Pozostalo procesow: %d\n", getpid(), pozostalo);
    }
    loguj(SEMAFOR_LOGI_KANDYDACI, LOGI_KANDYDACI, msg_buffer);

    semafor_v_bez_undo(SEMAFOR_KONIEC_KANDYDATOW);

    odlacz_shm(pamiec_shm);
    if (w_budynku)
    {
        semafor_v(SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM);
    }

    exit(EXIT_SUCCESS);
}

void handler_sigterm(int sigNum)
{
    (void)sigNum;
    ewakuacja_aktywna = true;
    if (pamiec_shm != NULL)
    {
        pamiec_shm->ewakuacja = true;
    }
}
