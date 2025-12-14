#include "egzamin.h"

int main()
{

    printf("Utworzono PROCES Kandydat | PID: %d\n", getpid());

    // symulacja pracy
    sleep(30);

    printf("Kandydat kończy prace\n");
    return 0;
}