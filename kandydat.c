#include "egzamin.h"

int main()
{
    char msg_buffer[200];
    PamiecDzielona *pamiec_shm;

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Ustawia sie w kolejce przed budynkiem\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // Oczekiwanie na start egzaminu
    while (true)
    {
        if (semafor_wartosc(SEMAFOR_EGZAMIN_START) == 1)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Dziekan rozpoczal egzamin\n", getpid());
            wypisz_wiadomosc(msg_buffer);
            break;
        }
    }

    // symulacja pracy
    sleep(30);

    odlacz_shm(pamiec_shm);

    return 0;
}