#include "egzamin.h"

int main()
{
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);


    printf("Utworzono PROCES Kandydat | PID: %d\n", getpid());
    sleep(15);

    printf("[Kandydat] PID:%d | Ustawia sie w koljece przed budynkiem\n",getpid());

    while(true)
    {
        if(semafor_wartosc(SEMAFOR_BUDYNEK) == 1)
        {
            printf("[KANDYDAT] PID:%d | Dziekan rozpoczal egzamin\n",getpid());
            break;
        }
    }
    // symulacja pracy
    sleep(30);

    printf("Kandydat kończy prace\n");
    return 0;
}