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
    return (los < 2) ? false : true;
}

void init_kandydat(pid_t pid, Kandydat *k)
{

    k->pid = pid;
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

    k->wynik_a = -1;
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
    semafor_id = semget(klucz_sem, 2, IPC_CREAT | IPC_EXCL | 0600);
    if (semafor_id == -1)
    {
        if (errno == EEXIST)
        {
            semafor_id = semget(klucz_sem, 2, 0600);
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
        if (semctl(semafor_id, SEMAFOR_BUDYNEK, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_BUDYNEK");
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_STD_OUT, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_STD_OUT");
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
    if (msgsnd(msqid, msg, msg_size - sizeof(long), 0) == -1)
    {
        perror("msgsnd() | Nie udalo sie wyslac wiadomosci.");
        exit(EXIT_FAILURE);
    }
}

void msq_receive(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci)
{
    if (msgrcv(msqid, buffer, buffer_size - sizeof(long), typ_wiadomosci, 0) == -1)
    {
        perror("msgrcv() | Nie udalo sie odberac wiadomosci.");
        exit(EXIT_FAILURE);
    }
}

void usun_msq(int msqid)
{
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
    {
        perror("msgctl() | Nie udalo sie usunac kolejki komunikatow");
        exit(EXIT_FAILURE);
    }
}