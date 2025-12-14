#include "egzamin.h"

int main()
{

    printf("Utworzono PROCES Dziekan | PID: %d\n", getpid());

    // symulacja pracy
    sleep(200);

    printf("Dziekan kończy prace\n");
    return 0;
}