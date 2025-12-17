#include "egzamin.h"

int main()
{
    char msg_buffer[200];
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Ustawia sie w kolejce przed budynkiem\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // Oczekiwanie na start egzaminu
    while (semafor_wartosc(SEMAFOR_EGZAMIN_START) == 1)
    {
        if ()
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Dziekan rozpoczal egzamin\n", getpid());
            wypisz_wiadomosc(msg_buffer);
            break;
        }
    }
    // symulacja pracy
    sleep(30);

    return 0;
}