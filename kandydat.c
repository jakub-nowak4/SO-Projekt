#include "egzamin.h"

int main()
{
    char msg_buffer[1024];
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d Utworzono proces kandydat\n[Kandydat] PID:%d | Ustawia sie w koljece przed budynkiem\n", getpid(), getpid());
    wypisz_wiadomosc(msg_buffer);

    while (true)
    {
        if (semafor_wartosc(SEMAFOR_BUDYNEK) == 1)
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