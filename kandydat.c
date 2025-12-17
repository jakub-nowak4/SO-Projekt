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

    // Wyslij inforamcje poprzez FIFO i czekaj na odpowiedz
    sleep(20);

    odlacz_shm(pamiec_shm);

    return 0;
}