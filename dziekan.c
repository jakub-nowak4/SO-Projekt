#include "egzamin.h"

int main()
{

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);


    printf("Utworzono PROCES Dziekan | PID: %d\n", getpid());

    //Egzamin start
    sleep(120);
    semafor_v(SEMAFOR_BUDYNEK);
    printf("PID: %d | Dziekan rozpoczyna egzamin\n",getpid());


    // symulacja pracy

    printf("Dziekan kończy prace\n");
    return 0;
}