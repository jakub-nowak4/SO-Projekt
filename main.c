#include "egzamin.h"

pid_t grupa_procesow = 0;
pid_t pid_dziekan = 0;

void handler_sigint(int sig)
{
    (void)sig;
    if (pid_dziekan > 0)
    {
        kill(pid_dziekan, SIGUSR2);
    }
}

int main()
{
    srand(time(NULL));

    signal(SIGUSR2, SIG_IGN);

    mkdir(LOGI_DIR, 0777);
    const char *files[] = {LOGI_MAIN, LOGI_DZIEKAN, LOGI_KANDYDACI, LOGI_KOMISJA_A, LOGI_KOMISJA_B, LOGI_LISTA_RANKINGOWA, LOGI_LISTA_ODRZUCONYCH};
    for (int i = 0; i < 7; i++)
    {
        int fd = open(files[i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd != -1)
            close(fd);
    }

    grupa_procesow = getpid();
    if (setpgid(0, 0) == -1)
    {
        perror("setpgid() | Nie udalo sie utworzyc grupy procesow");
        exit(EXIT_FAILURE);
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
    pamiec_shm->ewakuacja = false;
    pamiec_shm->pozostalo_kandydatow = LICZBA_KANDYDATOW;
    pamiec_shm->liczba_osob_w_A = 0;
    pamiec_shm->liczba_osob_w_B = 0;
    pamiec_shm->nastepny_do_komisja_A = 0;
    pamiec_shm->odpowiadajacy_A = 0;
    pamiec_shm->odpowiadajacy_B = 0;
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
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] DANE STARTOWE:\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] LICZBA MIEJSC: %d\n", M);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] LICZBA KANDYDATÓW: %d\n", LICZBA_KANDYDATOW);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // Dziekan
    pid_dziekan = fork();
    switch (pid_dziekan)
    {
    case -1:
        perror("fork() | Nie udalo sie utworzyc procesu dziekan");
        exit(EXIT_FAILURE);

    case 0:
        setpgid(0, grupa_procesow);
        execl("./dziekan", "dziekan", NULL);
        perror("execl() | Nie udalo sie urchomic programu dziekan.");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handler_sigint);

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
            setpgid(0, grupa_procesow);
            if (i == 0)
            {
                usleep(10000);
                execl("./komisja_a", "komisja_a", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_a.");
            }
            else
            {
                usleep(10000);
                execl("./komisja_b", "komisja_b", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_b.");
            }
            exit(EXIT_FAILURE);
        }
    }

    sleep(1);

    int procent_wczesnych = rand() % 31 + 20;
    int liczba_wczesnych = (LICZBA_KANDYDATOW * procent_wczesnych) / 100;
    int utworzonych_kandydatow = 0;

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] %d kandydatów (%d%%) przyszło PRZED rozpoczęciem egzaminu\n", liczba_wczesnych, procent_wczesnych);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // Kandydaci ktorzy sa wczesniej
    for (int i = 0; i < liczba_wczesnych; i++)
    {
        semafor_p(SEMAFOR_MUTEX);
        bool byla_ewakuacja = pamiec_shm->ewakuacja;
        semafor_v(SEMAFOR_MUTEX);

        if (byla_ewakuacja)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[main] EWAKUACJA - przerywam tworzenie wczesnych kandydatow (utworzono %d z %d)\n", i, liczba_wczesnych);
            loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
            break;
        }

        usleep(rand() % 30000 + 5000); // 5-35ms

        switch (fork())
        {
        case -1:
            perror("fork() | Nie udalo sie utworzyc kandydata");
            exit(EXIT_FAILURE);

        case 0:
            setpgid(0, grupa_procesow);
            execl("./kandydat", "kandydat", NULL);
            perror("execl() | Nie udalo sie urchomic programu kandydat");
            exit(EXIT_FAILURE);
        }
        utworzonych_kandydatow++;
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Oczekiwanie na godzinę T (start egzaminu)...\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    sleep(GODZINA_ROZPOCZECIA_EGZAMINU);

    // Sprawdz ewakuacje przed startem egzaminu
    semafor_p(SEMAFOR_MUTEX);
    bool byla_ewakuacja = pamiec_shm->ewakuacja;
    semafor_v(SEMAFOR_MUTEX);

    if (!byla_ewakuacja)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[main] Wysyłam sygnał SIGUSR1 do Dziekana (PID:%d) - rozpoczynam egzamin\n", pid_dziekan);
        loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

        if (kill(pid_dziekan, SIGUSR1) == -1)
        {
            perror("kill() | Nie udalo sie wyslac SIGUSR1 do dziekana");
            exit(EXIT_FAILURE);
        }

        // Kandydaci spoznienieni
        int liczba_spoznionych = LICZBA_KANDYDATOW - liczba_wczesnych;

        snprintf(msg_buffer, sizeof(msg_buffer), "[main] %d kandydatów przychodzi W TRAKCIE egzaminu\n", liczba_spoznionych);
        loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

        for (int i = 0; i < liczba_spoznionych; i++)
        {
            semafor_p(SEMAFOR_MUTEX);
            byla_ewakuacja = pamiec_shm->ewakuacja;
            semafor_v(SEMAFOR_MUTEX);

            if (byla_ewakuacja)
            {
                snprintf(msg_buffer, sizeof(msg_buffer), "[main] EWAKUACJA - przerywam tworzenie kandydatow (utworzono %d z %d spoznionych)\n", i, liczba_spoznionych);
                loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
                break;
            }

            usleep(rand() % 80000 + 10000); // 10-90ms

            switch (fork())
            {
            case -1:
                perror("fork() | Nie udalo sie utworzyc kandydata");
                exit(EXIT_FAILURE);

            case 0:
                setpgid(0, grupa_procesow);
                execl("./kandydat", "kandydat", NULL);
                perror("execl() | Nie udalo sie urchomic programu kandydat");
                exit(EXIT_FAILURE);
            }
            utworzonych_kandydatow++;
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[main] Utworzono %d kandydatów\n", utworzonych_kandydatow);
        loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
    }
    else
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[main] EWAKUACJA przed startem egzaminu - nie wysylam SIGUSR1\n");
        loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
    }

    int status;
    pid_t pid_procesu;
    int zakonczonych = 0;

    while ((pid_procesu = wait(&status)) > 0)
    {
        zakonczonych++;
        if (WIFEXITED(status))
        {
            if (zakonczonych % 100 == 0 || pid_procesu == pid_dziekan)
            {
                snprintf(msg_buffer, sizeof(msg_buffer), "PROCES PID: %d zakonczyl dzialanie z kodem: %d (zakonczonych: %d)\n", pid_procesu, WEXITSTATUS(status), zakonczonych);
                loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
            }
        }
        else if (WIFSIGNALED(status))
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "PROCES PID: %d zostal przerwany sygnalem: %d\n", pid_procesu, WTERMSIG(status));
            loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "Zakończono oczekiwanie na procesy (zakonczonych: %d).\n", zakonczonych);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // Sprawdz czy byla ewakuacja
    semafor_p(SEMAFOR_MUTEX);
    byla_ewakuacja = pamiec_shm->ewakuacja;
    semafor_v(SEMAFOR_MUTEX);

    if (byla_ewakuacja)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[main] Ewakuacja zakonczona - usuwam zasoby IPC\n");
        loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
    }

    usun_semafory();
    odlacz_shm(pamiec_shm);
    usun_shm();
    usun_msq(msqid_budynek);
    usun_msq(msqid_A);
    usun_msq(msqid_B);
    usun_msq(msqid_dziekan_komisja);

    return 0;
}