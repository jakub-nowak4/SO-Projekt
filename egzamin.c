#include "egzamin.h"

int semafor_id = -1;
int shmid = -1;

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
    int len;

    semafor_p(sem_index);

    pobierz_czas_precyzyjny(&czas, &ms);

    len = snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d:%03d | %s", czas.tm_hour, czas.tm_min, czas.tm_sec, ms, msg);

    int fd = open(file_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd != -1)
    {
        write(fd, buffer, len);
        close(fd);
    }
    semafor_v(sem_index);

    semafor_p(SEMAFOR_STD_OUT);
    write(STDOUT_FILENO, buffer, len);
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

    semafor_p(SEMAFOR_STD_OUT);
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

void init_kandydat(pid_t pid, Kandydat *k)
{

    k->pid = pid;
    k->numer_na_liscie = -1;
    k->status = KOLEJKA_PRZED_BUDYNKIEM;

    k->czy_zdal_mature = losuj_czy_zdal_matura();

    if (k->czy_zdal_mature)
    {
        k->czy_powtarza_egzamin = losuj_czy_powtarza_egzamin();
    }
    else
    {
        k->czy_powtarza_egzamin = false;
    }

    if (k->czy_powtarza_egzamin)
    {
        k->wynik_a = rand() % (100 - 30 + 1) + 30;
    }
    else
    {
        k->wynik_a = -1;
    }

    k->wynik_b = -1;
    k->wynik_koncowy = -1;
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
    semafor_id = semget(klucz_sem, 11, IPC_CREAT | IPC_EXCL | 0600);
    if (semafor_id == -1)
    {
        if (errno == EEXIST)
        {
            semafor_id = semget(klucz_sem, 11, 0600);
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
    }
}

void usun_semafory(void)
{
    if (semctl(semafor_id, 0, IPC_RMID) == -1)
    {
        perror("semctl() | Nie udalo sie usunac zbioru semaforow");
        exit(EXIT_FAILURE);
    }
}

void semafor_p(int semNum)
{
    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = -1;
    buffer.sem_flg = SEM_UNDO;

    if (semop(semafor_id, &buffer, 1) == -1)
    {
        perror("semop() | Nie udalo sie wykonac operacji semafor P");
        exit(EXIT_FAILURE);
    }
}

void semafor_v(int semNum)
{
    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = 1;
    buffer.sem_flg = SEM_UNDO;

    if (semop(semafor_id, &buffer, 1) == -1)
    {
        perror("semop() | Nie udalo sie wykonac operacji semafor V");
        exit(EXIT_FAILURE);
    }
}

int semafor_wartosc(int semNum)
{
    int wartosc = semctl(semafor_id, semNum, GETVAL);
    if (wartosc == -1)
    {
        perror("semop() | Nie udalo sie odczytac wartosci semafora");
        exit(EXIT_FAILURE);
    }

    return wartosc;
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
}

void odlacz_shm(PamiecDzielona *adr)
{
    if (shmdt(adr) == -1)
    {
        perror("shmdt() | Nie udalo sie odlaczyc shm od pamiec adresowej procesu.");
        exit(EXIT_FAILURE);
    }
}

void usun_shm(void)
{
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("shmctl() | Nie udalo sie usunac shm.");
        exit(EXIT_FAILURE);
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

void msq_send(int msqid, void *msg, size_t msg_size)
{
    size_t msgsz = msg_size - sizeof(long);

    while (msgsnd(msqid, msg, msgsz, 0) == -1)
    {
        if (errno == EINTR)
            continue;
        if (errno == EIDRM || errno == EINVAL)
        {
            exit(EXIT_SUCCESS);
        }
        perror("msgsnd() | Krytyczny blad wysylania");
        exit(EXIT_FAILURE);
    }
}

void msq_receive(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci)
{

    if (msgrcv(msqid, buffer, buffer_size - sizeof(long), typ_wiadomosci, 0) == -1)
    {
        if (errno == EIDRM || errno == EINVAL)
            exit(EXIT_SUCCESS);
        perror("msgrcv() | Nie udalo sie odberac wiadomosci.");
        exit(EXIT_FAILURE);
    }
}

ssize_t msq_receive_no_wait(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci)
{
    ssize_t res;
    if ((res = msgrcv(msqid, buffer, buffer_size - sizeof(long), typ_wiadomosci, IPC_NOWAIT)) == -1)
    {
        if (errno == ENOMSG)
        {
            return res;
        }
        else
        {
            perror("msgrcv() | Nie udalo sie odberac wiadomosci.");
            exit(EXIT_FAILURE);
        }
    }

    return res;
}

void usun_msq(int msqid)
{
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
    {
        perror("msgctl() | Nie udalo sie usunac kolejki komunikatow");
        exit(EXIT_FAILURE);
    }
}

int znajdz_kandydata(pid_t pid, PamiecDzielona *shm)
{
    if (shm == NULL)
    {
        printf("znajdz_index() | Przekazano pusty wskaznik\n");
        exit(EXIT_FAILURE);
    }

    int index = -1;

    semafor_p(SEMAFOR_MUTEX);
    for (int i = 0; i < shm->index_kandydaci; i++)
    {
        if (shm->LISTA_KANDYDACI[i].pid == pid)
        {
            index = i;
            break;
        }
    }
    semafor_v(SEMAFOR_MUTEX);

    return index;
}

void wypisz_liste_rankingowa(PamiecDzielona *shm)
{
    char buffer[512];

    // 1. PEŁNA LISTA RANKINGOWA
    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, "\n========== PEŁNA LISTA RANKINGOWA ==========\n");
    snprintf(buffer, sizeof(buffer), "Liczba kandydatów na liście: %d\n\n", shm->index_rankingowa);
    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);

    for (int i = 0; i < shm->index_rankingowa; i++)
    {
        snprintf(buffer, sizeof(buffer), "Miejsce %d: PID %d | Wynik: %.2f (A: %.2f, B: %.2f)\n", i + 1, shm->LISTA_RANKINGOWA[i].pid, shm->LISTA_RANKINGOWA[i].wynik_koncowy, shm->LISTA_RANKINGOWA[i].wynik_a, shm->LISTA_RANKINGOWA[i].wynik_b);
        loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);
    }

    // 2. LISTA PRZYJĘTYCH (TOP M)
    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, "\n========== LISTA PRZYJĘTYCH (TOP M) ==========\n");
    int przyjętych = (shm->index_rankingowa < M) ? shm->index_rankingowa : M;
    snprintf(buffer, sizeof(buffer), "Liczba miejsc: %d | Przyjętych: %d\n\n", M, przyjętych);
    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);

    for (int i = 0; i < przyjętych; i++)
    {
        snprintf(buffer, sizeof(buffer), "Miejsce %d: PID %d | Wynik: %.2f (A: %.2f, B: %.2f) - PRZYJĘTY\n", i + 1, shm->LISTA_RANKINGOWA[i].pid, shm->LISTA_RANKINGOWA[i].wynik_koncowy, shm->LISTA_RANKINGOWA[i].wynik_a, shm->LISTA_RANKINGOWA[i].wynik_b);
        loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);
    }

    // 3. LISTA NIEPRZYJĘTYCH
    if (shm->index_rankingowa > M)
    {
        loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, "\n========== LISTA NIEPRZYJĘTYCH (brak miejsc) ==========\n");
        int nieprzyjętych = shm->index_rankingowa - M;
        snprintf(buffer, sizeof(buffer), "Liczba nieprzyjętych mimo zdanego egzaminu: %d\n\n", nieprzyjętych);
        loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);

        for (int i = M; i < shm->index_rankingowa; i++)
        {
            snprintf(buffer, sizeof(buffer), "Miejsce %d: PID %d | Wynik: %.2f (A: %.2f, B: %.2f) - NIEPRZYJĘTY (brak miejsc)\n", i + 1, shm->LISTA_RANKINGOWA[i].pid, shm->LISTA_RANKINGOWA[i].wynik_koncowy, shm->LISTA_RANKINGOWA[i].wynik_a, shm->LISTA_RANKINGOWA[i].wynik_b);
            loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);
        }
    }

    // 4. LISTA ODRZUCONYCH (nie zdali egzaminu)
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

        snprintf(buffer, sizeof(buffer), "PID %d | A: %.2f | B: %.2f | Powod: %s\n", shm->LISTA_ODRZUCONYCH[i].pid, shm->LISTA_ODRZUCONYCH[i].wynik_a >= 0 ? shm->LISTA_ODRZUCONYCH[i].wynik_a : 0.0f, shm->LISTA_ODRZUCONYCH[i].wynik_b >= 0 ? shm->LISTA_ODRZUCONYCH[i].wynik_b : 0.0f, powod);
        loguj(SEMAFOR_LOGI_LISTA_ODRZUCONYCH, LOGI_LISTA_ODRZUCONYCH, buffer);
    }

    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, "\n========== PODSUMOWANIE ==========\n");
    snprintf(buffer, sizeof(buffer),
             "Kandydatów łącznie: %d\n"
             "Zdało egzamin (lista rankingowa): %d\n"
             "Przyjętych (TOP %d): %d\n"
             "Nieprzyjętych (brak miejsc): %d\n"
             "Odrzuconych (nie zdali): %d\n",
             shm->index_kandydaci + shm->index_odrzuceni,
             shm->index_rankingowa,
             M,
             przyjętych,
             (shm->index_rankingowa > M) ? shm->index_rankingowa - M : 0,
             shm->index_odrzuceni);
    loguj(SEMAFOR_LOGI_LISTA_RANKINGOWA, LOGI_LISTA_RANKINGOWA, buffer);
}