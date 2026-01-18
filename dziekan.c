#include "egzamin.h"

void start_egzamin(int sigNum);
void handler_sigusr2(int sigNum);
void handler_sigterm(int sigNum);

volatile sig_atomic_t egzamin_start = false;
volatile sig_atomic_t egzamin_aktywny = true;
volatile sig_atomic_t ewakuacja_aktywna = false;
PamiecDzielona *pamiec_shm = NULL;

int main()
{
    // SIGUSR1
    struct sigaction sa_usr1;
    sa_usr1.sa_handler = start_egzamin;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1)
    {
        perror("sigaction(SIGUSR1) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    // SIGUSR2
    struct sigaction sa_usr2;
    sa_usr2.sa_handler = handler_sigusr2;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = 0;
    if (sigaction(SIGUSR2, &sa_usr2, NULL) == -1)
    {
        perror("sigaction(SIGUSR2) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    // SIGTERM
    struct sigaction sa_term;
    sa_term.sa_handler = handler_sigterm;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    if (sigaction(SIGTERM, &sa_term, NULL) == -1)
    {
        perror("sigaction(SIGTERM) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    // SIGINT
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGINT) | Nie udalo sie zignorowac SIGINT.");
        exit(EXIT_FAILURE);
    }

    char msg_buffer[512];

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    semafor_v(SEMAFOR_DZIEKAN_GOTOWY);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    // Sprawdz czy ewakuacja nie zostala juz ogloszona
    if (pamiec_shm->ewakuacja)
    {
        ewakuacja_aktywna = true;
        egzamin_aktywny = false;
    }

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    int msqid_budynek = utworz_msq(klucz_msq_budynek);

    key_t klucz_msq_dziekan_komisja = utworz_klucz(MSQ_DZIEKAN_KOMISJA);
    int msqid_dziekan_komisja = utworz_msq(klucz_msq_dziekan_komisja);

    snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID: %d | Rozpoczynam prace.\n", getpid());
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    // Czeka na start egzaminu
    while (!egzamin_start)
    {
        pause();
    }

    // Start egzaminu
    semafor_p_bez_ewakuacji(SEMAFOR_MUTEX);
    pamiec_shm->egzamin_trwa = true;
    sprintf(msg_buffer, "[DZIEKAN] PID: %d | Rozpoczynam egzamin.\n", getpid());
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
    semafor_v(SEMAFOR_MUTEX);

    while (egzamin_aktywny && !ewakuacja_aktywna)
    {
        ssize_t res;

        semafor_p_bez_ewakuacji(SEMAFOR_MUTEX);
        int procesow = pamiec_shm->kandydatow_procesow;
        semafor_v(SEMAFOR_MUTEX);

        if (procesow == 0)
        {
            egzamin_aktywny = false;
            break;
        }

        MSG_ZGLOSZENIE zgloszenie;
        res = msq_receive_no_wait(msqid_budynek, &zgloszenie, sizeof(zgloszenie), KANDYDAT_PRZESYLA_MATURE);

        if (res != -1)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Odebralem wyniki matury od Kandydata PID:%d i przetwarzam je...\n", getpid(), zgloszenie.pid);
            loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

            MSG_DECYZJA decyzja;
            decyzja.mtype = MTYPE_BUDYNEK_DECYZJA + zgloszenie.pid;

            if (zgloszenie.matura)
            {
                decyzja.dopuszczony_do_egzamin = true;

                semafor_p_bez_ewakuacji(SEMAFOR_MUTEX);

                int index = pamiec_shm->index_kandydaci;

                Kandydat nowy_kandydat;
                nowy_kandydat.pid = zgloszenie.pid;
                nowy_kandydat.numer_na_liscie = index;
                nowy_kandydat.czy_zdal_mature = zgloszenie.matura;
                nowy_kandydat.czy_powtarza_egzamin = zgloszenie.czy_powtarza_egzamin;
                nowy_kandydat.wynik_a = zgloszenie.czy_powtarza_egzamin ? zgloszenie.wynik_a : -1.0f;
                nowy_kandydat.wynik_b = -1.0f;
                nowy_kandydat.wynik_koncowy = -1.0f;

                pamiec_shm->LISTA_KANDYDACI[index] = nowy_kandydat;
                decyzja.numer_na_liscie = index;
                pamiec_shm->index_kandydaci++;

                semafor_v(SEMAFOR_MUTEX);

                snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Po weryfikacji matury dopuszczam Kandydata PID:%d Nr:%d do dalszej czesci egzaminu.\n", getpid(), zgloszenie.pid, index);
                loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
            }
            else
            {
                decyzja.dopuszczony_do_egzamin = false;
                decyzja.numer_na_liscie = -1;

                semafor_p_bez_ewakuacji(SEMAFOR_MUTEX);

                int index = pamiec_shm->index_odrzuceni;

                Kandydat odrzucony;
                odrzucony.pid = zgloszenie.pid;
                odrzucony.numer_na_liscie = -1;
                odrzucony.czy_zdal_mature = false;
                odrzucony.czy_powtarza_egzamin = false;
                odrzucony.wynik_a = -1.0f;
                odrzucony.wynik_b = -1.0f;
                odrzucony.wynik_koncowy = -1.0f;

                pamiec_shm->LISTA_ODRZUCONYCH[index] = odrzucony;
                pamiec_shm->index_odrzuceni++;

                semafor_v(SEMAFOR_MUTEX);

                snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Odrzucam kandydata PID:%d (brak matury)\n", getpid(), zgloszenie.pid);
                loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
            }

            if (msq_send(msqid_budynek, &decyzja, sizeof(decyzja)) == -1)
            {
                continue;
            }
        }

        MSG_WYNIK_KONCOWY_DZIEKAN wynik_koncowy_egzamin;
        res = msq_receive_no_wait(msqid_dziekan_komisja, &wynik_koncowy_egzamin, sizeof(wynik_koncowy_egzamin), NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA);

        if (res != -1)
        {
            pid_t kandydat_pid = wynik_koncowy_egzamin.pid;
            float wynik = wynik_koncowy_egzamin.wynik_koncowy;
            char komisja = wynik_koncowy_egzamin.komisja;

            bool sukces = zaktualizuj_wynik_kandydata(kandydat_pid, komisja, wynik, pamiec_shm);

            if (!sukces)
            {
                char error_msg[256];
                int len = snprintf(error_msg, sizeof(error_msg), "dziekan.c | Nie znaleziono kandydata o podanym pid_t: %d\n", kandydat_pid);
                write(STDERR_FILENO, error_msg, len);
            }
        }
    }

    if (ewakuacja_aktywna && egzamin_start)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Ewakuacja w trakcie egzaminu - przerywam egzamin i generuje ranking\n");
        loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Odbieram pozostale wyniki z kolejki...\n");
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    int odebrane = 0;
    while (true)
    {
        MSG_WYNIK_KONCOWY_DZIEKAN wynik_koncowy_egzamin;
        int res = msq_receive_no_wait(msqid_dziekan_komisja, &wynik_koncowy_egzamin, sizeof(wynik_koncowy_egzamin), NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA);

        if (res == -1)
        {
            break;
        }

        pid_t kandydat_pid = wynik_koncowy_egzamin.pid;
        float wynik = wynik_koncowy_egzamin.wynik_koncowy;
        char komisja = wynik_koncowy_egzamin.komisja;

        bool sukces = zaktualizuj_wynik_kandydata(kandydat_pid, komisja, wynik, pamiec_shm);

        if (sukces)
        {
            odebrane++;
        }
        else
        {
            char error_msg[256];
            int len = snprintf(error_msg, sizeof(error_msg), "dziekan.c | Nie znaleziono kandydata o podanym pid_t: %d (odbiór pozostałych)\n", kandydat_pid);
            write(STDERR_FILENO, error_msg, len);
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Odebrano %d pozostalych wynikow.\n", odebrane);
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    // Kandydaci dostaja SIGTERM nie czekaj na nich
    if (!ewakuacja_aktywna)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Oczekiwanie na zakonczenie procesow kandydatow...\n");
        loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

        int odebrane = 0;
        while (odebrane < LICZBA_KANDYDATOW)
        {
            if (ewakuacja_aktywna)
                break;

            if (semafor_p(SEMAFOR_KONIEC_KANDYDATOW) == -1)
            {
                break;
            }
            odebrane++;

            if (odebrane % 1000 == 0 || odebrane == LICZBA_KANDYDATOW)
            {
                semafor_p_bez_ewakuacji(SEMAFOR_MUTEX);
                int pozostalo = pamiec_shm->kandydatow_procesow;
                semafor_v(SEMAFOR_MUTEX);

                snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Zakonczylo sie %d/%d kandydatow (pozostalo w SHM: %d)\n", odebrane, LICZBA_KANDYDATOW, pozostalo);
                loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
            }
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Przerwano oczekiwanie na kandydatow. Zakonczylo sie: %d/%d.\n", odebrane, LICZBA_KANDYDATOW);
        loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
    }
    else
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Ewakuacja - pomijam oczekiwanie na kandydatow (zostali zabici przez SIGTERM)\n");
        loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
    }

    semafor_p_bez_ewakuacji(SEMAFOR_MUTEX);
    pamiec_shm->egzamin_trwa = false;

    int n = 0;
    for (int i = 0; i < pamiec_shm->index_kandydaci; i++)
    {
        if (pamiec_shm->LISTA_KANDYDACI[i].wynik_a >= 30 &&
            pamiec_shm->LISTA_KANDYDACI[i].wynik_b >= 30)
        {
            pamiec_shm->LISTA_KANDYDACI[i].wynik_koncowy =
                (pamiec_shm->LISTA_KANDYDACI[i].wynik_a + pamiec_shm->LISTA_KANDYDACI[i].wynik_b) / 2;

            pamiec_shm->LISTA_RANKINGOWA[n] = pamiec_shm->LISTA_KANDYDACI[i];
            n++;
        }
        else
        {
            pamiec_shm->LISTA_KANDYDACI[i].wynik_koncowy = -1;

            int idx_odrz = pamiec_shm->index_odrzuceni;
            pamiec_shm->LISTA_ODRZUCONYCH[idx_odrz] = pamiec_shm->LISTA_KANDYDACI[i];
            pamiec_shm->index_odrzuceni++;
        }
    }
    pamiec_shm->index_rankingowa = n;

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n - i - 1; j++)
        {
            if (pamiec_shm->LISTA_RANKINGOWA[j].wynik_koncowy <
                pamiec_shm->LISTA_RANKINGOWA[j + 1].wynik_koncowy)
            {
                Kandydat temp = pamiec_shm->LISTA_RANKINGOWA[j];
                pamiec_shm->LISTA_RANKINGOWA[j] = pamiec_shm->LISTA_RANKINGOWA[j + 1];
                pamiec_shm->LISTA_RANKINGOWA[j + 1] = temp;
            }
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Na liscie rankingowej: %d kandydatow, odrzuconych: %d\n", n, pamiec_shm->index_odrzuceni);
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    wypisz_liste_rankingowa(pamiec_shm);
    semafor_v(SEMAFOR_MUTEX);

    // Czekaj na kom A i kom B
    snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Czekam na zakonczenie Komisji A i B...\n");
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    semafor_p_bez_ewakuacji(SEMAFOR_KOMISJA_A_KONIEC);
    semafor_p_bez_ewakuacji(SEMAFOR_KOMISJA_B_KONIEC);

    snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Komisje zakonczone. Dziekan konczy prace.\n");
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] PID:%d | Opuszczam budynek.\n", getpid());
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    odlacz_shm(pamiec_shm);

    return 0;
}

void start_egzamin(int sigNum)
{
    (void)sigNum;
    egzamin_start = true;
}

void handler_sigusr2(int sigNum)
{
    (void)sigNum;
    if (!ewakuacja_aktywna)
    {
        ewakuacja_aktywna = true;
        egzamin_aktywny = false;
        if (pamiec_shm != NULL)
        {
            pamiec_shm->ewakuacja = true;
        }

        kill(0, SIGTERM);
    }
}

void handler_sigterm(int sigNum)
{
    (void)sigNum;
    if (!ewakuacja_aktywna)
    {
        ewakuacja_aktywna = true;
        egzamin_aktywny = false;
        if (pamiec_shm != NULL)
        {
            pamiec_shm->ewakuacja = true;
        }
    }
}
