#include "egzamin.h"

void start_egzamin(int);
volatile sig_atomic_t egzamin_start = false; // kompilator zawsze czyta z pamięci

int main()
{
    if (signal(SIGUSR1, start_egzamin) == SIG_ERR)
    {
        perror("signal() | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    char msg_buffer[200];
    PamiecDzielona *pamiec_shm;

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    int msqid_budynek = utworz_msq(klucz_msq_budynek);

    snprintf(msg_buffer, sizeof(msg_buffer), "Utworzono PROCES Dziekan | PID: %d\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // Czekaj na start egzaminu o chwili T
    while (egzamin_start == false)
    {
        pause();
    }

    semafor_p(SEMAFOR_MUTEX);
    pamiec_shm->egzamin_trwa = true;
    sprintf(msg_buffer, "[DZIEKAN] PID: %d | Rozpoczynam egzamin\n", getpid());
    wypisz_wiadomosc(msg_buffer);
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
            snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Odebralem informacje o wyniku matury od Kandydata PID:%d\n", getpid(), zgloszenie.kandydat.pid);
            wypisz_wiadomosc(msg_buffer);

            MSG_DECYZJA decyzja;

            if (zgloszenie.kandydat.czy_zdal_mature)
            {
                snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Po weryfikacji matury dopuszczam Kandydata PID:%d do dalszej czesci egzaminu\n", getpid(), zgloszenie.kandydat.pid);
                wypisz_wiadomosc(msg_buffer);

                decyzja.mtype = zgloszenie.kandydat.pid;
                decyzja.dopuszczony_do_egzamin = true;

                semafor_p(SEMAFOR_MUTEX);

                int index = pamiec_shm->index_kandydaci;
                pamiec_shm->LISTA_KANDYDACI[index] = zgloszenie.kandydat;
                decyzja.numer_na_liscie = index;
                pamiec_shm->index_kandydaci++;

                semafor_v(SEMAFOR_MUTEX);
            }
            else
            {
                snprintf(msg_buffer, sizeof(msg_buffer), "[Dziekan] PID:%d | Po weryfikacji matury nie dopuszczam Kandydata PID:%d do dalszej czesci egzaminu.\n", getpid(), zgloszenie.kandydat.pid);
                wypisz_wiadomosc(msg_buffer);

                decyzja.mtype = zgloszenie.kandydat.pid;
                decyzja.dopuszczony_do_egzamin = false;
                decyzja.numer_na_liscie = -1;

                semafor_p(SEMAFOR_MUTEX);

                int index = pamiec_shm->index_odrzuceni;
                pamiec_shm->LISTA_ODRZUCONYCH[index] = zgloszenie.kandydat;
                pamiec_shm->index_odrzuceni++;

                semafor_v(SEMAFOR_MUTEX);
            }

            msq_send(msqid_budynek, &decyzja, sizeof(decyzja));
            MSG_POTWIERDZENIE potwierdzenie;
            msq_receive(msqid_budynek, &potwierdzenie, sizeof(potwierdzenie), zgloszenie.kandydat.pid);
        }

        usleep(1000);
    }

    odlacz_shm(pamiec_shm);

    return 0;
}

void start_egzamin(int)
{
    egzamin_start = true;
}