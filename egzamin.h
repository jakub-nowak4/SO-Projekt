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

extern int semafor_id;
extern int shmid;

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

typedef enum
{
    SEMAFOR_EGZAMIN_START,
    SEMAFOR_STD_OUT
} Semafory;

void pobierz_czas(struct tm *wynik);
void wypisz_wiadomosc(char *msg);

// ---- SEMAFORY ----

key_t utworz_klucz(int arg);
void usun_semafory(void);
void utworz_semafory(key_t klucz_sem);
void semafor_p(int semNum);
void semafor_v(int semNum);
int semafor_wartosc(int semNum);

// ---- SHM ----

void utworz_shm(key_t klucz_shm);
void dolacz_shm(PamiecDzielona **wsk);
void odlacz_shm(PamiecDzielona *adr);
void usun_shm(void);

#endif
