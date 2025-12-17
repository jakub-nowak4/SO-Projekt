#ifndef EGZAMIN
#define EGZAMIN

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#define M 12 // W docelowej symulacji M = 120
#define LICZBA_KANDYDATOW 12 * M
#define CZAS_OPRACOWNIE_PYTAN 5 // Czas Ti na opracownie pytan od komisji

int semafor_id;
int shmid;

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

    float wynika_a;
    float wynik_b;
    float wynik_koncowy;

} Kandydat;

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
    SEMAFOR_BUDYNEK,
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
                usun_semafory();
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
        if (semctl(semafor_id, SEMAFOR_BUDYNEK, SETVAL, 0) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_BUDYNEK");
            usun_semafory();
            exit(EXIT_FAILURE);
        }

        if (semctl(semafor_id, SEMAFOR_STD_OUT, SETVAL, 1) == -1)
        {
            perror("semctl() | Nie udalo sie ustawic wartosci poczatkowej SEMAFOR_STD_OUT");
            usun_semafory();
            exit(EXIT_FAILURE);
        }
    }
}

void semafor_p(int semNum)
{
    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = -1;
    buffer.sem_flg = 0;

    if (semop(semafor_id, &buffer, 1) == -1)
    {
        perror("semop() | Nie udalo sie wykonac operacji semafor P");
        usun_semafory();
        exit(EXIT_FAILURE);
    }
}

void semafor_v(int semNum)
{
    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = 1;
    buffer.sem_flg = 0;

    if (semop(semafor_id, &buffer, 1) == -1)
    {
        perror("semop() | Nie udalo sie wykonac operacji semafor V");
        usun_semafory();
        exit(EXIT_FAILURE);
    }
}

int semafor_wartosc(int semNum)
{
    int wartosc = semctl(semafor_id, semNum, GETVAL);
    if (wartosc == -1)
    {
        perror("semop() | Nie udalo sie odczytac wartosci semafora");
        usun_semafory();
        exit(EXIT_FAILURE);
    }

    return wartosc;
}

void wypisz_wiadomosc(char *msg)
{
    if (msg == NULL)
    {
        printf("wypisz_wiadomosc() | Podano nieprawidlowa wiadomosc\n");
        exit(EXIT_FAILURE);
    }

    semafor_p(SEMAFOR_STD_OUT);

    if (write(STDOUT_FILENO, msg, strlen(msg)) == -1)
    {
        perror("write() | Nie udalo sie wypisac wiadomosci na stdout");
        exit(EXIT_FAILURE);
    }

    semafor_v(SEMAFOR_STD_OUT);
}

#endif
