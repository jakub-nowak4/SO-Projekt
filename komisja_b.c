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

    snprintf(msg_buffer, sizeof(msg_buffer), "Utworzono PROCES Komisja B |  PID: %d\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // symulacja pracy
    sleep(200);

    snprintf(msg_buffer, sizeof(msg_buffer), "Komisja B kończy prace\n");
    wypisz_wiadomosc(msg_buffer);

    odlacz_shm(pamiec_shm);
    return 0;
}