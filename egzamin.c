#include "egzamin.h"

int semafor_id = -1;
int shmid = -1;
extern volatile sig_atomic_t ewakuacja_aktywna;
static PamiecDzielona *shm_ptr = NULL;

static inline bool czy_ewakuacja(void)
{
    if (ewakuacja_aktywna)
        return true;
    if (shm_ptr != NULL && shm_ptr->ewakuacja)
        return true;
    return false;
}

void pobierz_czas(struct tm *wynik)
{
    if (wynik == NULL)
    {
        printf("Nie udalo sie pobrac wartosci czasu\n");
        exit(EXIT_FAILURE);
    }

    time_t now = time(NULL);
    *wynik = *localtime(&now);
}

void pobierz_czas_precyzyjny(struct tm *wynik, int *ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    *ms = ts.tv_nsec / 1000000;
    localtime_r(&ts.tv_sec, wynik);
}

void loguj(int sem_index, char *file_path, char *msg)
{
    struct tm czas;
    int ms;
    char buffer[512];
    char color_buffer[600];
    int len;

    if (semafor_p_bez_ewakuacji(sem_index) == -1)
    {
        return;
    }

    pobierz_czas_precyzyjny(&czas, &ms);

    len = snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d:%02d | %s", czas.tm_hour, czas.tm_min, czas.tm_sec, ms / 10, msg);

    int fd = open(file_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd != -1)
    {
        (void)write(fd, buffer, len);
        close(fd);
    }
    semafor_v(sem_index);

    bool wazny_komunikat = (strcmp(file_path, LOGI_DZIEKAN) == 0 ||
                            strcmp(file_path, LOGI_MAIN) == 0 ||
                            strcmp(file_path, LOGI_LISTA_RANKINGOWA) == 0 ||
                            strcmp(file_path, LOGI_LISTA_ODRZUCONYCH) == 0 ||
                            strcmp(file_path, LOGI_KOMISJA_A) == 0 ||
                            strcmp(file_path, LOGI_KOMISJA_B) == 0);

    const char *color = "\033[0m";

    if (strcmp(file_path, LOGI_MAIN) == 0)
    {
        color = "\033[1;37m";
    }
    else if (strcmp(file_path, LOGI_DZIEKAN) == 0)
    {
        color = "\033[1;35m";
    }
    else if (strcmp(file_path, LOGI_KANDYDACI) == 0)
    {
        color = "\033[1;36m";
    }
    else if (strcmp(file_path, LOGI_KOMISJA_A) == 0)
    {
        color = "\033[1;33m";
    }
    else if (strcmp(file_path, LOGI_KOMISJA_B) == 0)
    {
        color = "\033[1;32m";
    }
    else if (strcmp(file_path, LOGI_LISTA_RANKINGOWA) == 0)
    {
        color = "\033[1;34m";
    }
    else if (strcmp(file_path, LOGI_LISTA_ODRZUCONYCH) == 0)
    {
        color = "\033[1;31m";
    }

    int color_len = snprintf(color_buffer, sizeof(color_buffer), "%s%s\033[0m", color, buffer);

    if (wazny_komunikat)
    {
        if (semafor_p_bez_ewakuacji(SEMAFOR_STD_OUT) == -1)
            return;
    }
    else
    {
        if (semafor_p(SEMAFOR_STD_OUT) == -1)
            return;
    }

    (void)write(STDOUT_FILENO, color_buffer, color_len);
    semafor_v(SEMAFOR_STD_OUT);
}

void wypisz_wiadomosc(char *msg)
{
    struct tm czas;
    char buffor[250];

    if (msg == NULL)
    {
        printf("wypisz_wiadomosc() | Podano nieprawidlowa wiadomosc\n");
        exit(EXIT_FAILURE);
    }

    if (semafor_p(SEMAFOR_STD_OUT) == -1)
    {
        fprintf(stderr, "[Egzamin] Blad semafora przy logowaniu\n");
        exit(EXIT_FAILURE);
    }
    pobierz_czas(&czas);
    int len = snprintf(buffor, sizeof(buffor), "%02d:%02d:%02d | %s", czas.tm_hour, czas.tm_min, czas.tm_sec, msg);

    if (write(STDOUT_FILENO, buffor, len) == -1)
    {
        semafor_v(SEMAFOR_STD_OUT);
        perror("write() | Nie udalo sie wypisac wiadomosci na stdout");
        exit(EXIT_FAILURE);
    }

    semafor_v(SEMAFOR_STD_OUT);
}

bool losuj_czy_zdal_matura()
{
    int los = rand() % 100;
    return (los < 2) ? false : true;
}

bool losuj_czy_powtarza_egzamin()
{
    int los = rand() % 100;
    return (los < 2) ? true : false;
}

key_t utworz_klucz(int arg)
{
    key_t klucz = ftok(".", arg);
    if (klucz == -1)
    {
        printf("ftok() | Nie udalo sie utworzyc klucza\n");
        exit(EXIT_FAILURE);
    }

    return klucz;
}

void utworz_semafory(key_t klucz_sem)
{
    semafor_id = semget(klucz_sem, 18, IPC_CREAT | IPC_EXCL | 0600);
    if (semafor_id == -1)
    {
        if (errno == EEXIST)
        {
            semafor_id = semget(klucz_sem, 18, 0600);
            if (semafor_id == -1)
            {
                perror("semget() | Nie udalo sie przylaczyc do zbioru semaforow");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            perror("semget() | Nie udalo sie utworzyc zbioru semaforow");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        if (semctl(semafor_id, SEMAFOR_STD_OUT, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_STD_OUT");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_MUTEX, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMFOR_MUTEX");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_ODPOWIEDZ_A, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMFOR_ODPOWIEDZ_A");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_ODPOWIEDZ_B, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMFOR_ODPOWIEDZ_B");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_LOGI_MAIN, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMFOR_LOGI_MAIN");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_LOGI_DZIEKAN, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_LOGI_DZIEKAN");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_LOGI_KANDYDACI, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_LOGI_KANDYDACI");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_LOGI_KOMISJA_A, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_LOGI_KOMISJA_A");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_LOGI_KOMISJA_B, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_LOGI_KOMISJA_B");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_LOGI_LISTA_RANKINGOWA, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_LOGI_LISTA_RANKINGOWA");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_LOGI_LISTA_ODRZUCONYCH, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_LOGI_LISTA_ODRZUCONYCH");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_DZIEKAN_GOTOWY, SETVAL, 0) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_DZIEKAN_GOTOWY");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_KOMISJA_A_GOTOWA, SETVAL, 0) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_KOMISJA_A_GOTOWA");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_KOMISJA_B_GOTOWA, SETVAL, 0) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_KOMISJA_B_GOTOWA");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM, SETVAL, POJEMNOSC_BUDYNKU) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_KONIEC_KANDYDATOW, SETVAL, 0) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_KONIEC_KANDYDATOW");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_KOMISJA_A_KONIEC, SETVAL, 0) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_KOMISJA_A_KONIEC");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_KOMISJA_B_KONIEC, SETVAL, 0) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_KOMISJA_B_KONIEC");
            exit(EXIT_FAILURE);
        }
    }
}

void usun_semafory(void)
{
    if (semafor_id == -1)
    {
        return;
    }
    if (semctl(semafor_id, 0, IPC_RMID) == -1)
    {
        if (errno != EINVAL && errno != EIDRM)
        {
            perror("semctl() | Nie udalo sie usunac zbioru semaforow");
        }
    }
}

int semafor_p(int semNum)
{
    if (czy_ewakuacja())
    {
        return -1;
    }

    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = -1;
    buffer.sem_flg = SEM_UNDO;

    while (semop(semafor_id, &buffer, 1) == -1)
    {
        if (errno == EINTR)
        {
            if (czy_ewakuacja())
            {
                return -1;
            }
            continue;
        }
        if (errno == EINVAL || errno == EIDRM)
        {
            return -1;
        }
        perror("semop() | Nie udalo sie wykonac operacji semafor P");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int semafor_p_bez_ewakuacji(int semNum)
{
    // funkcja nie sprawdza ewakuacji na poczatku zywana gdy proces musi dokonczyc operacje nawet podczas ewakuacji
    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = -1;
    buffer.sem_flg = SEM_UNDO;

    while (semop(semafor_id, &buffer, 1) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EINVAL || errno == EIDRM)
        {
            return -1;
        }
        perror("semop() | Nie udalo sie wykonac operacji semafor P (bez ewakuacji)");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void semafor_v(int semNum)
{
    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = 1;
    buffer.sem_flg = SEM_UNDO;

    while (semop(semafor_id, &buffer, 1) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EINVAL || errno == EIDRM)
        {
            return;
        }
        perror("semop() | Nie udalo sie wykonac operacji semafor V");
        exit(EXIT_FAILURE);
    }
}

// wersja bez SEM_UNDO do sygnalizacji zakonczenia procesu
void semafor_v_bez_undo(int semNum)
{
    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = 1;
    buffer.sem_flg = 0;

    while (semop(semafor_id, &buffer, 1) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EINVAL || errno == EIDRM)
        {
            return;
        }
        perror("semop() | Nie udalo sie wykonac operacji semafor V final");
        exit(EXIT_FAILURE);
    }
}

void utworz_shm(key_t klucz_shm)
{
    shmid = shmget(klucz_shm, sizeof(PamiecDzielona), IPC_CREAT | IPC_EXCL | 0600);
    if (shmid == -1)
    {
        if (errno == EEXIST)
        {
            shmid = shmget(klucz_shm, sizeof(PamiecDzielona), 0600);
            if (shmid == -1)
            {
                perror("shmget() | Nie udalo sie podlaczyc do shm.");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            perror("shmget() | Nie udalo sie utworzyc shm.");
            exit(EXIT_FAILURE);
        }
    }
}

void dolacz_shm(PamiecDzielona **wsk)
{
    *wsk = (PamiecDzielona *)shmat(shmid, NULL, 0);
    if (*wsk == (PamiecDzielona *)-1)
    {
        perror("shmat() | Nie udalo sie dolaczyc shm do pamieci adresowej procesu.");
        exit(EXIT_FAILURE);
    }
    shm_ptr = *wsk;
}

void odlacz_shm(PamiecDzielona *adr)
{
    if (adr == NULL || adr == (PamiecDzielona *)-1)
    {
        return;
    }
    if (shmdt(adr) == -1)
    {
        if (errno != EINVAL)
        {
            perror("shmdt() | Nie udalo sie odlaczyc shm od pamiec adresowej procesu.");
        }
    }
}

void usun_shm(void)
{
    if (shmid == -1)
    {
        return;
    }
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        if (errno != EINVAL && errno != EIDRM)
        {
            perror("shmctl() | Nie udalo sie usunac shm.");
        }
    }
}

int utworz_msq(key_t klucz_msq)
{
    int msqid = msgget(klucz_msq, IPC_CREAT | IPC_EXCL | 0600);
    if (msqid == -1)
    {
        if (errno == EEXIST)
        {
            msqid = msgget(klucz_msq, 0600);
            if (msqid == -1)
            {
                perror("msgget() | Nie udalo sie podlaczyc do kolejki komunikatow");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            perror("msgget() | Nie udalo sie utworzyc kolejki komunikatow");
            exit(EXIT_FAILURE);
        }
    }

    return msqid;
}

int msq_send(int msqid, void *msg, size_t msg_size)
{
    size_t msgsz = msg_size - sizeof(long);

    while (true)
    {
        if (czy_ewakuacja())
        {
            return -1;
        }

        if (msgsnd(msqid, msg, msgsz, IPC_NOWAIT) == 0)
        {
            return 0;
        }

        if (errno == EAGAIN)
        {
            continue;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EIDRM || errno == EINVAL)
        {
            return -1;
        }

        const char *error_msg = "Blad krytyczny wysylania wiadomosci\n";
        (void)write(2, error_msg, strlen(error_msg));
        exit(EXIT_FAILURE);
    }
}

ssize_t msq_receive(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci)
{
    ssize_t res;
    while (true)
    {
        if (czy_ewakuacja())
        {
            return -1;
        }

        res = msgrcv(msqid, buffer, buffer_size - sizeof(long), typ_wiadomosci, 0);
        if (res == -1)
        {
            if (errno == EINTR)
            {
                if (ewakuacja_aktywna)
                {
                    return -1;
                }
                continue;
            }

            if (errno == EIDRM || errno == EINVAL)
            {
                return -1;
            }
            perror("msgrcv() | Nie udalo sie odebrac wiadomosci.");
            exit(EXIT_FAILURE);
        }
        return res;
    }
}

ssize_t msq_receive_no_wait(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci)
{
    ssize_t res;
    res = msgrcv(msqid, buffer, buffer_size - sizeof(long), typ_wiadomosci, IPC_NOWAIT);
    if (res == -1)
    {
        if (errno == ENOMSG || errno == EINVAL || errno == EIDRM || errno == EINTR)
        {
            return -1;
        }

        perror("msgrcv() | Nie udalo sie odberac wiadomosci.");
        exit(EXIT_FAILURE);
    }

    return res;
}

void usun_msq(int msqid)
{
    if (msqid == -1)
    {
        return;
    }
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
    {
        if (errno != EINVAL && errno != EIDRM)
        {
            perror("msgctl() | Nie udalo sie usunac kolejki komunikatow");
        }
    }
}

bool zaktualizuj_wynik_kandydata(pid_t pid, char komisja, float wynik, PamiecDzielona *shm)
{
    if (shm == NULL)
    {
        printf("zaktualizuj_wynik_kandydata() | Przekazano pusty wskaznik\n");
        exit(EXIT_FAILURE);
    }

    bool znaleziono = false;

    if (semafor_p(SEMAFOR_MUTEX) == -1)
    {
        return false;
    }
    for (int i = 0; i < shm->index_kandydaci; i++)
    {
        if (shm->LISTA_KANDYDACI[i].pid == pid)
        {
            if (komisja == 'A')
            {
                shm->LISTA_KANDYDACI[i].wynik_a = wynik;
            }
            else if (komisja == 'B')
            {
                shm->LISTA_KANDYDACI[i].wynik_b = wynik;
            }
            znaleziono = true;
            break;
        }
    }
    semafor_v(SEMAFOR_MUTEX);

    return znaleziono;
}

void wypisz_liste_rankingowa(PamiecDzielona *shm)
{
    char buffer[512];

    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, "\n========== PELNA LISTA RANKINGOWA ==========\n");
    snprintf(buffer, sizeof(buffer), "Liczba kandydatow na liscie: %d\n\n", shm->index_rankingowa);
    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);

    for (int i = 0; i < shm->index_rankingowa; i++)
    {
        snprintf(buffer, sizeof(buffer), "Miejsce %d: PID %d Nr:%d | Wynik: %.2f (A: %.2f, B: %.2f)\n", i + 1, shm->LISTA_RANKINGOWA[i].pid, shm->LISTA_RANKINGOWA[i].numer_na_liscie, shm->LISTA_RANKINGOWA[i].wynik_koncowy, shm->LISTA_RANKINGOWA[i].wynik_a, shm->LISTA_RANKINGOWA[i].wynik_b);
        loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);
    }

    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, "\n========== LISTA PRZYJETYCH (TOP M) ==========\n");
    int przyjetych = (shm->index_rankingowa < M) ? shm->index_rankingowa : M;
    snprintf(buffer, sizeof(buffer), "Liczba miejsc: %d | Przyjetych: %d\n\n", M, przyjetych);
    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);

    for (int i = 0; i < przyjetych; i++)
    {
        snprintf(buffer, sizeof(buffer), "Miejsce %d: PID %d Nr:%d | Wynik: %.2f (A: %.2f, B: %.2f) - PRZYJETY\n", i + 1, shm->LISTA_RANKINGOWA[i].pid, shm->LISTA_RANKINGOWA[i].numer_na_liscie, shm->LISTA_RANKINGOWA[i].wynik_koncowy, shm->LISTA_RANKINGOWA[i].wynik_a, shm->LISTA_RANKINGOWA[i].wynik_b);
        loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);
    }

    if (shm->index_rankingowa > M)
    {
        loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, "\n========== LISTA NIEPRZYJETYCH (brak miejsc) ==========\n");
        int nieprzyjetych = shm->index_rankingowa - M;
        snprintf(buffer, sizeof(buffer), "Liczba nieprzyjetych mimo zdanego egzaminu: %d\n\n", nieprzyjetych);
        loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);

        for (int i = M; i < shm->index_rankingowa; i++)
        {
            snprintf(buffer, sizeof(buffer), "Miejsce %d: PID %d Nr:%d | Wynik: %.2f (A: %.2f, B: %.2f) - NIEPRZYJETY (brak miejsc)\n", i + 1, shm->LISTA_RANKINGOWA[i].pid, shm->LISTA_RANKINGOWA[i].numer_na_liscie, shm->LISTA_RANKINGOWA[i].wynik_koncowy, shm->LISTA_RANKINGOWA[i].wynik_a, shm->LISTA_RANKINGOWA[i].wynik_b);
            loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);
        }
    }

    loguj(SEMAFOR_LOGI_LISTA_ODRZUCONYCH, LOGI_LISTA_ODRZUCONYCH, "\n========== LISTA ODRZUCONYCH ==========\n");
    snprintf(buffer, sizeof(buffer), "Liczba odrzuconych: %d\n\n", shm->index_odrzuceni);
    loguj(SEMAFOR_LOGI_LISTA_ODRZUCONYCH, LOGI_LISTA_ODRZUCONYCH, buffer);

    for (int i = 0; i < shm->index_odrzuceni; i++)
    {
        const char *powod;
        if (!shm->LISTA_ODRZUCONYCH[i].czy_zdal_mature)
        {
            powod = "Brak matury";
        }
        else if (shm->LISTA_ODRZUCONYCH[i].wynik_a < 30)
        {
            powod = "Zbyt niski wynik A";
        }
        else if (shm->LISTA_ODRZUCONYCH[i].wynik_b < 30)
        {
            powod = "Zbyt niski wynik B";
        }
        else
        {
            powod = "Zbyt niski wynik";
        }

        snprintf(buffer, sizeof(buffer), "PID %d Nr:%d | A: %.2f | B: %.2f | Powod: %s\n", shm->LISTA_ODRZUCONYCH[i].pid, shm->LISTA_ODRZUCONYCH[i].numer_na_liscie, shm->LISTA_ODRZUCONYCH[i].wynik_a >= 0 ? shm->LISTA_ODRZUCONYCH[i].wynik_a : 0.0f, shm->LISTA_ODRZUCONYCH[i].wynik_b >= 0 ? shm->LISTA_ODRZUCONYCH[i].wynik_b : 0.0f, powod);
        loguj(SEMAFOR_LOGI_LISTA_ODRZUCONYCH, LOGI_LISTA_ODRZUCONYCH, buffer);
    }

    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, "\n========== PODSUMOWANIE ==========\n");
    snprintf(buffer, sizeof(buffer),
             "Kandydatow lacznie: %d\n"
             "Zdalo egzamin (lista rankingowa): %d\n"
             "Przyjetych (TOP %d): %d\n"
             "Nieprzyjetych (brak miejsc): %d\n"
             "Odrzuconych (nie zdali): %d\n",
             shm->index_rankingowa + shm->index_odrzuceni,
             shm->index_rankingowa,
             M,
             przyjetych,
             (shm->index_rankingowa > M) ? shm->index_rankingowa - M : 0,
             shm->index_odrzuceni);
    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);
}
