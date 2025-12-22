#include "egzamin.h"

int main()
{
    srand(time(NULL));
    // init
    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);

    PamiecDzielona *pamiec_shm;
    dolacz_shm(&pamiec_shm);

    // Init shm
    semafor_p(SEMAFOR_MUTEX);
    pamiec_shm->index_kandydaci = 0;
    pamiec_shm->index_odrzuceni = 0;
    pamiec_shm->egzamin_trwa = false;
    pamiec_shm->pozostalo_kandydatow = LICZBA_KANDYDATOW;
    pamiec_shm->liczba_osob_w_A = 0;
    pamiec_shm->liczba_osob_w_B = 0;
    pamiec_shm->nastepny_do_komisja_A = 0;
    semafor_v(SEMAFOR_MUTEX);

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    key_t klucz_msq_B = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_B);

    int msqid_budynek = utworz_msq(klucz_msq_budynek);
    int msqid_A = utworz_msq(klucz_msq_A);
    int msqid_B = utworz_msq(klucz_msq_B);

    char msg_buffer[200];

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

    semafor_p(SEMAFOR_MUTEX);
    int dopuszczonych = pamiec_shm->index_kandydaci;

    for (int i = 0; i < dopuszczonych; i++)
    {
        Kandydat k = pamiec_shm->LISTA_KANDYDACI[i];
        snprintf(msg_buffer, sizeof(msg_buffer), "Index:%d PID:%d | Matura:%d Czy powtarza:%d\n", i, k.pid, k.czy_zdal_mature, k.czy_powtarza_egzamin);
        wypisz_wiadomosc(msg_buffer);
    }

    // Odrzuceni
    int odrzuconych = pamiec_shm->index_odrzuceni;

    for (int i = 0; i < odrzuconych; i++)
    {
        Kandydat k = pamiec_shm->LISTA_ODRZUCONYCH[i];
        snprintf(msg_buffer, sizeof(msg_buffer), "Index:%d PID:%d | Matura:%d Czy powtarza:%d\n", i, k.pid, k.czy_zdal_mature, k.czy_powtarza_egzamin);
        wypisz_wiadomosc(msg_buffer);
    }

    semafor_v(SEMAFOR_MUTEX);

    usun_semafory();
    odlacz_shm(pamiec_shm);
    usun_shm();
    usun_msq(msqid_budynek);
    usun_msq(msqid_A);
    usun_msq(msqid_B);

    return 0;
}
