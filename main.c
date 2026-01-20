#include "egzamin.h"
#include <pthread.h>

volatile sig_atomic_t ewakuacja_aktywna = false;
int liczba_zakonczonych_procesow = 0;
int liczba_utworzonych_procesow = 0;
pid_t pid_dziekan = -1;
PamiecDzielona *pamiec_shm_global = NULL;

pthread_t watek_zbierajacy;
pthread_mutex_t mutex_procesow;
pthread_cond_t cond_procesow;
volatile bool watek_dziala = true;

void handler_sigint(int sigNum);
void *zbieraj_procesy(void *arg);

int main()
{
    if (POJEMNOSC_BUDYNKU >= MAX_POJEMNOSC_BUDYNKU)
    {
        fprintf(stderr, "BLAD: POJEMNOSC_BUDYNKU (%d) musi byc < %d!\n", POJEMNOSC_BUDYNKU, MAX_POJEMNOSC_BUDYNKU);
        exit(EXIT_FAILURE);
    }

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
        perror("signal(SIGINT)");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGTERM)");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&mutex_procesow, NULL) != 0)
    {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&cond_procesow, NULL) != 0)
    {
        perror("pthread_cond_init");
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

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Rozpoczynam symulacje EGZAMIN WSTEPNY NA KIERUNEK INFORMATYKA\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] LICZBA MIEJSC: %d\n", M);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] LICZBA KANDYDATOW: %d\n", LICZBA_KANDYDATOW);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    if (pthread_create(&watek_zbierajacy, NULL, zbieraj_procesy, NULL) != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Uruchomiono watek zbierajacy procesy\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // Dziekan
    pid_dziekan = fork();
    switch (pid_dziekan)
    {
    case -1:
        perror("fork() dziekan");
        exit(EXIT_FAILURE);
    case 0:
        setpgid(0, getppid());
        execl("./dziekan", "dziekan", NULL);
        perror("execl dziekan");
        exit(EXIT_FAILURE);
    default:
        pthread_mutex_lock(&mutex_procesow);
        liczba_utworzonych_procesow++;
        pthread_mutex_unlock(&mutex_procesow);
        break;
    }

    // Komisje A i B
    for (int i = 0; i < 2; i++)
    {
        switch (fork())
        {
        case -1:
            perror("fork() komisja");
            exit(EXIT_FAILURE);
        case 0:
            setpgid(0, getppid());
            if (i == 0)
                execl("./komisja_a", "komisja_a", NULL);
            else
                execl("./komisja_b", "komisja_b", NULL);
            perror("execl komisja");
            exit(EXIT_FAILURE);
        default:
            pthread_mutex_lock(&mutex_procesow);
            liczba_utworzonych_procesow++;
            pthread_mutex_unlock(&mutex_procesow);
            break;
        }
    }

    semafor_p(SEMAFOR_DZIEKAN_GOTOWY);
    semafor_p(SEMAFOR_KOMISJA_A_GOTOWA);
    semafor_p(SEMAFOR_KOMISJA_B_GOTOWA);

    // Kandydaci
    for (int i = 0; i < LICZBA_KANDYDATOW; i++)
    {
        switch (fork())
        {
        case -1:
            perror("fork() kandydat");
            exit(EXIT_FAILURE);
        case 0:
            setpgid(0, getppid());
            execl("./kandydat", "kandydat", NULL);
            perror("execl kandydat");
            exit(EXIT_FAILURE);
        default:
            pthread_mutex_lock(&mutex_procesow);
            liczba_utworzonych_procesow++;
            pthread_mutex_unlock(&mutex_procesow);
            break;
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Utworzono %d kandydatow\n", LICZBA_KANDYDATOW);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    // Start egzaminu
    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Wysylam SIGUSR1 do Dziekana (PID:%d)\n", pid_dziekan);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    if (kill(pid_dziekan, SIGUSR1) == -1)
    {
        perror("kill SIGUSR1");
        exit(EXIT_FAILURE);
    }

    // Odblokuj SIGINT
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
    {
        perror("sigprocmask(SIG_UNBLOCK)");
        exit(EXIT_FAILURE);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] Oczekiwanie na zakonczenie procesow...\n");
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    pthread_mutex_lock(&mutex_procesow);
    while (liczba_zakonczonych_procesow < liczba_utworzonych_procesow)
    {
        pthread_cond_wait(&cond_procesow, &mutex_procesow);
    }
    pthread_mutex_unlock(&mutex_procesow);

    watek_dziala = false;
    pthread_join(watek_zbierajacy, NULL);

    snprintf(msg_buffer, sizeof(msg_buffer), "[main] PODSUMOWANIE: Utworzono %d, zakonczono %d procesow\n", liczba_utworzonych_procesow, liczba_zakonczonych_procesow);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    pthread_mutex_destroy(&mutex_procesow);
    pthread_cond_destroy(&cond_procesow);
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
            pamiec_shm_global->ewakuacja = true;

        const char *msg = "CTRL+C: Ewakuacja aktywna\n";
        (void)write(STDOUT_FILENO, msg, strlen(msg));

        if (pid_dziekan > 0)
            kill(pid_dziekan, SIGUSR2);
    }
}

void *zbieraj_procesy(void *arg)
{
    (void)arg;
    char msg_buffer[512];

    while (watek_dziala)
    {
        int status;
        int zebrane_w_iteracji = 0;

        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        {
            zebrane_w_iteracji++;
        }

        if (zebrane_w_iteracji > 0)
        {
            pthread_mutex_lock(&mutex_procesow);
            liczba_zakonczonych_procesow += zebrane_w_iteracji;
            int zakonczone = liczba_zakonczonych_procesow;
            int utworzone = liczba_utworzonych_procesow;

            if (zakonczone >= utworzone || zakonczone % 100 == 0)
            {
                pthread_cond_signal(&cond_procesow);
            }
            pthread_mutex_unlock(&mutex_procesow);

            if (zakonczone >= utworzone && utworzone > 0)
            {
                break;
            }

            continue;
        }

        if (pid == -1)
        {
            if (errno == ECHILD)
            {
                pthread_mutex_lock(&mutex_procesow);
                int zakonczone = liczba_zakonczonych_procesow;
                int utworzone = liczba_utworzonych_procesow;
                pthread_mutex_unlock(&mutex_procesow);

                if (zakonczone >= utworzone && utworzone > 0)
                {
                    break;
                }
            }
        }
    }

    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
    {
        pthread_mutex_lock(&mutex_procesow);
        liczba_zakonczonych_procesow++;
        pthread_mutex_unlock(&mutex_procesow);
    }

    pthread_mutex_lock(&mutex_procesow);
    pthread_cond_signal(&cond_procesow);
    pthread_mutex_unlock(&mutex_procesow);

    snprintf(msg_buffer, sizeof(msg_buffer), "[Watek] Zakonczono. Zebrano: %d procesow\n", liczba_zakonczonych_procesow);
    loguj(SEMAFOR_LOGI_MAIN, LOGI_MAIN, msg_buffer);

    return NULL;
}
