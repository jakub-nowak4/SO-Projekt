#include "egzamin.h"

int main()
{
    // Sprawdzenie limitu POJEMNOSC_BUDYNKU
    if (POJEMNOSC_BUDYNKU >= MAX_POJEMNOSC_BUDYNKU)
    {
        fprintf(stderr, "BLAD: POJEMNOSC_BUDYNKU (%d) musi byc < %d!\n", POJEMNOSC_BUDYNKU, MAX_POJEMNOSC_BUDYNKU);
        fprintf(stderr, "Zmniejsz wartosc POJEMNOSC_BUDYNKU w egzamin.h\n");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    setpgid(0, 0);

    mkdir(LOGI_DIR, 0777);
    const char *files[] = {LOGI_MAIN, LOGI_DZIEKAN, LOGI_KANDYDACI, LOGI_KOMISJA_A, LOGI_KOMISJA_B, LOGI_LISTA_RANKINGOWA, LOGI_LISTA_ODRZUCONYCH};
    for (int i = 0; i < 7; i++)
    {
        int fd = open(files[i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd != -1)
            close(fd);
    }

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
    pamiec_shm->index_rankingowa = 0;
    pamiec_shm->egzamin_trwa = false;
    pamiec_shm->kandydatow_procesow = LICZBA_KANDYDATOW;
    pamiec_shm->liczba_osob_w_A = 0;
    pamiec_shm->liczba_osob_w_B = 0;
    pamiec_shm->nastepny_do_komisja_A = 0;
    semafor_v(SEMAFOR_MUTEX);

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    key_t klucz_msq_B = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_B);
    key_t klucz_msq_dziekan_komisja = utworz_klucz(MSQ_DZIEKAN_KOMISJA);

    int msqid_budynek = utworz_msq(klucz_msq_budynek);
    int msqid_A = utworz_msq(klucz_msq_A);
    int msqid_B = utworz_msq(klucz_msq_B);
    int msqid_dziekan_komisja = utworz_msq(klucz_msq_dziekan_komisja);

    char msg_buffer[512];

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Rozpoczynam symulacje EGZAMIN WSTĘPNY NA KIERUNEK INFORMATYKA\n");
    // wypisz_wiadomosc(msg_buffer);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] DANE STARTOWE:\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] LICZBA MIEJSC: %d\n", M);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] LICZBA KANDYDATÓW: %d\n", LICZBA_KANDYDATOW);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // Dziekan
    pid_t pid_dziekan = fork();
    switch (pid_dziekan)
    {
    case -1:
        perror("fork() | Nie udalo sie utworzyc procesu dziekan");
        exit(EXIT_FAILURE);

    case 0:
        setpgid(0, getppid());
        execl("./dziekan", "dziekan", NULL);
        perror("execl() | Nie udalo sie urchomic programu dziekan.");
        exit(EXIT_FAILURE);
    }

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
                setpgid(0, getppid());
                // usleep(10000);
                execl("./komisja_a", "komisja_a", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_a.");
            }
            else
            {
                setpgid(0, getppid());
                // usleep(10000);
                execl("./komisja_b", "komisja_b", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_a.");
            }
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < LICZBA_KANDYDATOW; i++)
    {
        // Kandydaci przychodza w roznych odstepach czasu
        // usleep(rand() % 30000 + 5000); // 5-35ms

        switch (fork())
        {
        case -1:
            perror("fork() | Nie udalo sie utworzyc kandydata");
            exit(EXIT_FAILURE);

        case 0:
            setpgid(0, getppid());
            execl("./kandydat", "kandydat", NULL);
            perror("execl() | Nie udalo sie urchomic programu kandydat");
            exit(EXIT_FAILURE);
        }
    }

    semafor_p(SEMAFOR_DZIEKAN_GOTOWY);
    semafor_p(SEMAFOR_KOMISJA_A_GOTOWA);
    semafor_p(SEMAFOR_KOMISJA_B_GOTOWA);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Oczekiwanie na godzinę T (start egzaminu)...\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // sleep(GODZINA_ROZPOCZECIA_EGZAMINU);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Wysyłam sygnał SIGUSR1 do Dziekana (PID:%d) - rozpoczynam egzamin\n", pid_dziekan);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    if (kill(pid_dziekan, SIGUSR1) == -1)
    {
        perror("kill() | Nie udalo sie wyslac SIGUSR1 do dziekana");
        exit(EXIT_FAILURE);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Utworzono wszystkich %d kandydatów\n", LICZBA_KANDYDATOW);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    int status;
    pid_t pid_procesu;

    while ((pid_procesu = wait(&status)) > 0)
    {
        if (WIFEXITED(status))
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "PROCES PID: %d zakonczył działanie z kodem: %d\n", pid_procesu, WEXITSTATUS(status));
            loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
        }
        else if (WIFSIGNALED(status))
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "PROCES PID: %d został przerwany sygnałem: %d\n", pid_procesu, WTERMSIG(status));
            loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "Zakończono oczekiwanie na procesy.\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    usun_semafory();
    odlacz_shm(pamiec_shm);
    usun_shm();
    usun_msq(msqid_budynek);
    usun_msq(msqid_A);
    usun_msq(msqid_B);
    usun_msq(msqid_dziekan_komisja);

    return 0;
}