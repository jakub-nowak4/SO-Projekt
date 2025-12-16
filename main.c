#include "egzamin.h"

int main()
{
    printf("[main] Rozpoczynam symulacje EGZAMIN WSTĘPNY NA KIERUNEK INFORMATYKA\n");
    printf("[main] DANE STARTOWE:\n");
    printf("[main] LICZBA MIEJSC: %d\n", M);
    printf("[main] LICZBA KANDYDATÓW: %d\n", LICZBA_KANDYDATOW);

    //init
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);

    // Dziekan
    switch (fork())
    {
    case -1:
        perror("fork() | Nie udalo sie utworzyc procesu dziekan");
        exit(EXIT_FAILURE);

    case 0:
        execl("./dziekan", "dziekan", NULL);
        perror("execl() | Nie udalo sie urchomic programu dziekan.");
        exit(EXIT_FAILURE);
    }

    // Komisj A oraz B

    for (int i = 0; i < 2; i++)
    {
        switch (fork())
        {
        case -1:
            if (i == 0)
            {
                perror("fork() | Nie udalo sie utworzyc procesu komisja A");
            }
            else
            {
                perror("fork() | Nie udalo sie utworzyc procesu komisja B");
            }
            exit(EXIT_FAILURE);

        case 0:
            if (i == 0)
            {
                execl("./komisja_a", "komisja_a", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_a.");
            }
            else
            {
                execl("./komisja_b", "komisja_b", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_a.");
            }
            exit(EXIT_FAILURE);
        }
    }

    // Kandydaci
    for (int i = 0; i < LICZBA_KANDYDATOW; i++)
    {
        switch (fork())
        {
        case -1:
            perror("fork() | Nie udalo sie utworzyc kandydata");
            exit(EXIT_FAILURE);
            break;

        case 0:
            execl("./kandydat", "kandydat", NULL);
            perror("execl() | Nie udalo sie urchomic programu kandydat");
            exit(EXIT_FAILURE);
        }
    }

    printf("[main] Poprawnie utworzono wszytskie potrzebne procesy do działania symulacji\n");

    int status;
    pid_t pid_procesu;

    while ((pid_procesu = wait(&status)) > 0)
    {
        if (WIFEXITED(status))
        {
            printf("PROCES O PID: %d zakonczył działanie z kodem: %d\n", pid_procesu, WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            printf("PROCES O PID: %d został przerwany sygnałem: %d\n", pid_procesu, WTERMSIG(status));
        }
    }

    printf("Zakończono oczekiwanie na procesy.\n");

    return 0;
}
