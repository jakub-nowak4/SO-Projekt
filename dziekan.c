#include "egzamin.h"

int main()
{
    char msg_buffer[1024];
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    sprintf(msg_buffer, "Utworzono PROCES Dziekan | PID: %d\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // Egzamin start
    sleep(120);
    semafor_v(SEMAFOR_BUDYNEK);
    sprintf(msg_buffer, "PID: %d | Dziekan rozpoczyna egzamin\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    return 0;
}