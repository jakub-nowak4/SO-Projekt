#include "egzamin.h"

int main()
{
    printf("Utworzono PROCES Komisja B |  PID: %d\n", getpid());

    // symulacja pracy
    sleep(200);

    printf("Komisja B kończy prace\n");
    return 0;
}