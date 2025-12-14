#include "egzamin.h"

int main()
{

    printf("Utworzono PROCES Komisja A |  PID: %d\n", getpid());

    // symulacja pracy
    sleep(200);

    printf("Komisja A kończy prace\n");

    return 0;
}