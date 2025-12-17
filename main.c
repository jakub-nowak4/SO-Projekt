#include "egzamin.h"

int main()
{
    srand(time(NULL));
    // init
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    char msg_buffer[1024];

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Rozpoczynam symulacje EGZAMIN WSTĘPNY NA KIERUNEK INFORMATYKA\n");
    wypisz_wiadomosc(msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] DANE STARTOWE:\n");
    wypisz_wiadomosc(msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] LICZBA MIEJSC: %d\n", M);
    wypisz_wiadomosc(msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] LICZBA KANDYDATÓW: %d\n", LICZBA_KANDYDATOW);
    wypisz_wiadomosc(msg_buffer);

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

    sleep(1);

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
                usleep(100);
                execl("./komisja_a", "komisja_a", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_a.");
            }
            else
            {
                usleep(100);
                execl("./komisja_b", "komisja_b", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_a.");
            }
            exit(EXIT_FAILURE);
        }
    }

    sleep(1);

    // Kandydaci
    for (int i = 0; i < LICZBA_KANDYDATOW; i++)
    {
        usleep(rand() % (500000 - 100000 + 1) + 100000);
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

    sleep(1);
    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Poprawnie utworzono wszytskie potrzebne procesy do działania symulacji\n");
    wypisz_wiadomosc(msg_buffer);

    int status;
    pid_t pid_procesu;

    while ((pid_procesu = wait(&status)) > 0)
    {
        if (WIFEXITED(status))
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "PROCES PID: %d zakonczył działanie z kodem: %d\n", pid_procesu, WEXITSTATUS(status));
            wypisz_wiadomosc(msg_buffer);
        }
        else if (WIFSIGNALED(status))
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "PROCES PID: %d został przerwany sygnałem: %d\n", pid_procesu, WTERMSIG(status));
            wypisz_wiadomosc(msg_buffer);
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "Zakończono oczekiwanie na procesy.\n");
    wypisz_wiadomosc(msg_buffer);

    usun_semafory();
    return 0;
}
