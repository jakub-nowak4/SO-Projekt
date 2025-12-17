#include "egzamin.h"

void start_egzamin(int sig);
volatile sig_atomic_t egzamin_start = false; // kompilator zawsze czyta z pamięci

int main()
{
    if (signal(SIGUSR1, start_egzamin) == SIG_ERR)
    {
        perror("signal() | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    char msg_buffer[200];
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    snprintf(msg_buffer, sizeof(msg_buffer), "Utworzono PROCES Dziekan | PID: %d\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // Czekaj na start egzaminu o chwili T
    while (egzamin_start == false)
    {
        pause();
    }

    sprintf(msg_buffer, "PID: %d | Dziekan rozpoczyna egzamin\n", getpid());
    wypisz_wiadomosc(msg_buffer);
    semafor_v(SEMAFOR_EGZAMIN_START);

    return 0;
}

void start_egzamin(int)
{
    egzamin_start = true;
}