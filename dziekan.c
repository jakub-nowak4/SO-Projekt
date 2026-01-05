#include "egzamin.h"

void start_egzamin(int sigNum);
volatile sig_atomic_t egzamin_start = false; // kompilator zawsze czyta z pamięci

int main()
{
    if (signal(SIGUSR1, start_egzamin) == SIG_ERR)
    {
        perror("signal() | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    char msg_buffer[512];
    PamiecDzielona *pamiec_shm;

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    int msqid_budynek = utworz_msq(klucz_msq_budynek);

    key_t klucz_msq_dziekan_komisja = utworz_klucz(MSQ_DZIEKAN_KOMISJA);
    int msqid_dziekan_komisja = utworz_msq(klucz_msq_dziekan_komisja);

    snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID: %d | Rozpoczynam prace.\n", getpid());
    // wypisz_wiadomosc(msg_buffer);
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    // Czekaj na start egzaminu o chwili T
    while (egzamin_start == false)
    {
        pause();
    }

    semafor_p(SEMAFOR_MUTEX);
    pamiec_shm->egzamin_trwa = true;
    sprintf(msg_buffer, "[DZIEKAN] PID: %d | Rozpoczynam egzamin.\n", getpid());
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
    semafor_v(SEMAFOR_MUTEX);

    // Czekaj na wiadomosci z FIFO
    while (true)
    {
        ssize_t res;

        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa)
        {
            break;
        }

        MSG_ZGLOSZENIE zgloszenie;
        res = msq_receive_no_wait(msqid_budynek, &zgloszenie, sizeof(zgloszenie), KANDYDAT_PRZESYLA_MATURE);

        // Dziekan sprawdza wynik matury kandydata
        if (res != -1)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Odebralem informacje o wyniku matury od Kandydata PID:%d\n", getpid(), zgloszenie.pid);
            loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

            MSG_DECYZJA decyzja;
            decyzja.mtype = zgloszenie.pid;

            if (zgloszenie.czy_zdal_mature)
            {

                decyzja.dopuszczony_do_egzamin = true;

                semafor_p(SEMAFOR_MUTEX);

                int index = pamiec_shm->index_kandydaci;
                pamiec_shm->LISTA_KANDYDACI[index].pid = zgloszenie.pid;
                pamiec_shm->LISTA_KANDYDACI[index].numer_na_liscie = index;
                pamiec_shm->LISTA_KANDYDACI[index].status = WERYFIKACJA_MATURY;
                pamiec_shm->LISTA_KANDYDACI[index].czy_zdal_mature = zgloszenie.czy_zdal_mature;
                pamiec_shm->LISTA_KANDYDACI[index].czy_powtarza_egzamin = zgloszenie.czy_powtarza_egzamin;
                pamiec_shm->LISTA_KANDYDACI[index].wynik_a = zgloszenie.wynik_a;
                pamiec_shm->LISTA_KANDYDACI[index].wynik_b = -1;
                pamiec_shm->LISTA_KANDYDACI[index].wynik_koncowy = -1;

                decyzja.numer_na_liscie = index;
                pamiec_shm->index_kandydaci++;

                semafor_v(SEMAFOR_MUTEX);

                snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Po weryfikacji matury dopuszczam Kandydata PID:%d do dalszej czesci egzaminu\n", getpid(), zgloszenie.pid);
                loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
            }
            else
            {
                decyzja.dopuszczony_do_egzamin = false;
                decyzja.numer_na_liscie = -1;

                semafor_p(SEMAFOR_MUTEX);
                int index = pamiec_shm->index_odrzuceni;
                pamiec_shm->LISTA_ODRZUCONYCH[index].pid = zgloszenie.pid;
                pamiec_shm->LISTA_ODRZUCONYCH[index].czy_zdal_mature = false;
                pamiec_shm->LISTA_ODRZUCONYCH[index].czy_powtarza_egzamin = false;
                pamiec_shm->LISTA_ODRZUCONYCH[index].wynik_a = -1;
                pamiec_shm->LISTA_ODRZUCONYCH[index].wynik_b = -1;
                pamiec_shm->LISTA_ODRZUCONYCH[index].wynik_koncowy = -1;
                pamiec_shm->index_odrzuceni++;
                semafor_v(SEMAFOR_MUTEX);

                snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Odrzucam kandydata PID:%d (brak matury)\n", getpid(), zgloszenie.pid);
                loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);
            }

            msq_send(msqid_budynek, &decyzja, sizeof(decyzja));
        }

        MSG_WYNIK_KONCOWY_DZIEKAN wynik_koncowy_egzamin;
        res = msq_receive_no_wait(msqid_dziekan_komisja, &wynik_koncowy_egzamin, sizeof(wynik_koncowy_egzamin), NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA);

        if (res != -1)
        {
            pid_t kandydat_pid = wynik_koncowy_egzamin.pid;
            float wynik = wynik_koncowy_egzamin.wynik_koncowy;

            int index = znajdz_kandydata(kandydat_pid, pamiec_shm);

            if (index == -1)
            {
                printf("dziekan.c | Nie znaleziono kandydta o podanym pid_t.\n");
                exit(EXIT_FAILURE);
            }

            semafor_p(SEMAFOR_MUTEX);

            if (wynik_koncowy_egzamin.komisja == 'A')
            {
                pamiec_shm->LISTA_KANDYDACI[index].wynik_a = wynik;
            }
            else
            {
                pamiec_shm->LISTA_KANDYDACI[index].wynik_b = wynik;
            }

            semafor_v(SEMAFOR_MUTEX);
        }

        usleep(1000);
    }

    // Pozostale wiadomosci
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

        int index = znajdz_kandydata(kandydat_pid, pamiec_shm);

        if (index != -1)
        {
            semafor_p(SEMAFOR_MUTEX);

            if (wynik_koncowy_egzamin.komisja == 'A')
            {
                pamiec_shm->LISTA_KANDYDACI[index].wynik_a = wynik;
            }
            else
            {
                pamiec_shm->LISTA_KANDYDACI[index].wynik_b = wynik;
            }

            semafor_v(SEMAFOR_MUTEX);
            odebrane++;
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[DZIEKAN] Odebrano %d pozostalych wynikow.\n", odebrane);
    loguj(SEMAFOR_LOGI_DZIEKAN, LOGI_DZIEKAN, msg_buffer);

    // Dziekan tworzy liste rankingowa
    semafor_p(SEMAFOR_MUTEX);

    // Dziekan liczy wynik koncowy i segreguje kandydatów
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

    // Sortowanie listy rankingowej
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

    odlacz_shm(pamiec_shm);

    return 0;
}

void start_egzamin(int sigNum)
{
    (void)sigNum;
    egzamin_start = true;
}
