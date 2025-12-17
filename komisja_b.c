#include "egzamin.h"

int main()
{
    char msg_buffer[1024];
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    snprintf(msg_buffer, sizeof(msg_buffer), "Utworzono PROCES Komisja B |  PID: %d\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // symulacja pracy
    sleep(200);

    snprintf(msg_buffer, sizeof(msg_buffer), "Komisja B kończy prace\n");
    wypisz_wiadomosc(msg_buffer);
    return 0;
}