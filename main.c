#include "egzamin.h"

volatile sig_atomic_t ewakuacja_aktywna = false;
volatile sig_atomic_t liczba_zakonczonych_procesow = 0;
int liczba_utworzonych_procesow = 0;
pid_t pid_dziekan = -1;
PamiecDzielona *pamiec_shm_global = NULL;

void handler_sigint(int sigNum);
void handler_sigchld(int sigNum);

int main()
{
    // Sprawdzenie limitu POJEMNOSC_BUDYNKU
    if (POJEMNOSC_BUDYNKU >= MAX_POJEMNOSC_BUDYNKU)
    {
        fprintf(stderr, "BLAD: POJEMNOSC_BUDYNKU (%d) musi byc < %d!\n", POJEMNOSC_BUDYNKU, MAX_POJEMNOSC_BUDYNKU);
        fprintf(stderr, "Zmniejsz wartosc POJEMNOSC_BUDYNKU w egzamin.h\n");
        exit(EXIT_FAILURE);
    }

    // Ewakuacja moze zostac przeprowadzona tylko po stracie egzaminu - do tego czasu blokujemy SIGINT
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
    {
        perror("sigprocmask(SIG_BLOCK)");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, handler_sigint) == SIG_ERR)
    {
        perror("signal(SIGINT) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGTERM) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_chld;
    sa_chld.sa_handler = handler_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1)
    {
        perror("sigaction(SIGCHLD) | Nie udalo sie dodac signal handler.");
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

    key_t klucz_sem = utworz_klucz(66);
    key_t klucz_shm = utworz_klucz(77);

    utworz_semafory(klucz_sem);
    utworz_shm(klucz_shm);

    PamiecDzielona *pamiec_shm;
    dolacz_shm(&pamiec_shm);
    pamiec_shm_global = pamiec_shm;

    if (semafor_p(SEMAFOR_MUTEX) == 0)
    {
        pamiec_shm->index_kandydaci = 0;
        pamiec_shm->index_odrzuceni = 0;
        pamiec_shm->index_rankingowa = 0;
        pamiec_shm->egzamin_trwa = false;
        pamiec_shm->ewakuacja = false;
        pamiec_shm->kandydatow_procesow = LICZBA_KANDYDATOW;
        pamiec_shm->liczba_osob_w_A = 0;
        pamiec_shm->liczba_osob_w_B = 0;
        pamiec_shm->nastepny_do_komisja_A = 0;
        semafor_v(SEMAFOR_MUTEX);
    }

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
        setpgid(0, getppid());
        execl("./dziekan", "dziekan", NULL);
        perror("execl() | Nie udalo sie urchomic programu dziekan.");
        exit(EXIT_FAILURE);

    default:
        liczba_utworzonych_procesow++;
        break;
    }

    // Tworzenie komisji A i B
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
                execl("./komisja_a", "komisja_a", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_a.");
            }
            else
            {
                setpgid(0, getppid());
                execl("./komisja_b", "komisja_b", NULL);
                perror("execl() | Nie udalo sie urchomic programu komisja_b.");
            }
            exit(EXIT_FAILURE);

        default:
            liczba_utworzonych_procesow++;
            break;
        }
    }

    // Oczekiwanie na gotowosc procesow
    if (semafor_p(SEMAFOR_DZIEKAN_GOTOWY) == -1)
    {
        perror("semafor_p(SEMAFOR_DZIEKAN_GOTOWY)");
        exit(EXIT_FAILURE);
    }
    if (semafor_p(SEMAFOR_KOMISJA_A_GOTOWA) == -1)
    {
        perror("semafor_p(SEMAFOR_KOMISJA_A_GOTOWA)");
        exit(EXIT_FAILURE);
    }
    if (semafor_p(SEMAFOR_KOMISJA_B_GOTOWA) == -1)
    {
        perror("semafor_p(SEMAFOR_KOMISJA_B_GOTOWA)");
        exit(EXIT_FAILURE);
    }

    // Tworzenie wszystkich kandydatów
    for (int i = 0; i < LICZBA_KANDYDATOW; i++)
    {
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

        default:
            liczba_utworzonych_procesow++;
            break;
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Utworzono wszystkich %d kandydatów\n", LICZBA_KANDYDATOW);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Oczekiwanie na godzinę T (start egzaminu)...\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // sleep(GODZINA_ROZPOCZECIA_EGZAMINU);

    // SIGUSR1 - dziekan rozpoczyna egzamin
    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Wysyłam sygnał SIGUSR1 do Dziekana (PID:%d) - rozpoczynam egzamin\n", pid_dziekan);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
    if (kill(pid_dziekan, SIGUSR1) == -1)
    {
        perror("kill() | Nie udalo sie wyslac SIGUSR1 do dziekana");
        exit(EXIT_FAILURE);
    }

    // Odblokowac SIGINT !!!
    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Godzina T minela - odblokowuje SIGINT (Ewakuacja mozliwa)\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
    {
        perror("sigprocmask(SIG_UNBLOCK)");
        exit(EXIT_FAILURE);
    }

    int status;
    pid_t pid_procesu;

    while (true)
    {
        pid_procesu = wait(&status);
        if (pid_procesu == -1)
        {
            if (errno == EINTR)
                continue;
            if (errno == ECHILD)
                break;
            perror("wait() | Blad oczekiwania na procesy");
            break;
        }

        liczba_zakonczonych_procesow++;
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "Zakończono oczekiwanie na procesy.\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // Podsumowanie procesów
    snprintf(msg_buffer, sizeof(msg_buffer), "[main] PODSUMOWANIE: Utworzono %d procesów, zakończyło się %d procesów\n", 
             liczba_utworzonych_procesow, (int)liczba_zakonczonych_procesow);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);
    printf("\n========================================\n");
    printf("PODSUMOWANIE PROCESÓW:\n");
    printf("  Utworzono:    %d procesów\n", liczba_utworzonych_procesow);
    printf("  Zakończono:   %d procesów\n", (int)liczba_zakonczonych_procesow);
    printf("========================================\n\n");

    usun_semafory();
    odlacz_shm(pamiec_shm);
    usun_shm();
    usun_msq(msqid_budynek);
    usun_msq(msqid_A);
    usun_msq(msqid_B);
    usun_msq(msqid_dziekan_komisja);

    return 0;
}

void handler_sigint(int sigNum)
{
    (void)sigNum;
    if (!ewakuacja_aktywna)
    {
        ewakuacja_aktywna = true;
        if (pamiec_shm_global != NULL)
        {
            pamiec_shm_global->ewakuacja = true;
        }
        const char *msg = "CTRL+C: Ewakuacja aktywna\n";
        (void)write(STDOUT_FILENO, msg, strlen(msg));
        // SIGUSR2 - jesli dziekan instanieje ropczyna procedure ewakuacji
        if (pid_dziekan > 0)
        {
            kill(pid_dziekan, SIGUSR2);
        }
    }
}

void handler_sigchld(int sigNum)
{
    (void)sigNum;
    int saved_errno = errno;
    int status;

    while (waitpid(-1, &status, WNOHANG) > 0)
    {
        liczba_zakonczonych_procesow++;
    }

    errno = saved_errno;
}
