#ifndef EGZAMIN_H
#define EGZAMIN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#define M 12 // W docelowej symulacji M = 120
#define LICZBA_KANDYDATOW (10 * M)
#define CZAS_OPRACOWANIE_PYTAN 5 // Czas Ti na opracownie pytan od komisji

int semafor_id = -1;
int shmid = -1;

typedef enum
{
    NIEZNANY,
    KOLEJKA_PRZED_BUDYNKIEM,
    WERYFIKACJA_MATURY,
    KOLEJKA_A,
    EGZAMIN_A_CZEKA_NA_PYTANIA,
    EGZAMIN_A_OPRACOWUJE_PYTANIA,
    EGZAMIN_A_ODPOWIADA,
    KOLEJKA_B,
    EGZAMIN_B_CZEKA_NA_PYTANIA,
    EGZAMIN_B_OPRACOWUJE_PYTANIA,
    EGZAMIN_B_ODPOWIADA,
    KONIEC_EGZAMINU
} Kandydat_Status;

typedef struct
{
    pid_t pid;
    Kandydat_Status status;

    float wynik_matura;
    bool czy_powtarza_egzamin;

    float wynik_a;
    float wynik_b;
    float wynik_koncowy;

} Kandydat;

typedef struct
{
    Kandydat LISTA_KANDYDACI[LICZBA_KANDYDATOW];
    Kandydat LISTA_ODRZUCONYCH[LICZBA_KANDYDATOW];
} PamiecDzielona;

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

typedef enum
{
    SEMAFOR_EGZAMIN_START,
    SEMAFOR_STD_OUT
} Semafory;

void usun_semafory()
{
    if (semctl(semafor_id, 0, IPC_RMID) == -1)
    {
        perror("semctl() | Nie udalo sie usunac zbioru semaforow");
        exit(EXIT_FAILURE);
    }
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
        if (semctl(semafor_id, SEMAFOR_EGZAMIN_START, SETVAL, 0) == -1)
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

void usun_shm(void)
{
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("shmctl() | Nie udalo sie usunac shm.");
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
}

void odlacz_shm(PamiecDzielona *adr)
{
    if (shmdt(adr) == -1)
    {
        perror("shmdt() | Nie udalo sie odlaczyc shm od pamiec adresowej procesu.");
        exit(EXIT_FAILURE);
    }
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

#endif
