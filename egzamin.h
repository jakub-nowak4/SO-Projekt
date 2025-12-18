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
#include <sys/msg.h>
#include <sys/ipc.h>

#define M 120 // W docelowej symulacji M = 120
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

typedef enum
{
    MSQ_KOLEJKA_BUDYNEK = 88,
    MSQ_KOLEJKA_EGZAMIN_A = 881,
    MSQ_KOLEJKA_EGZAMIN_B = 882
} MSQ;

typedef struct
{
    pid_t pid;
    Kandydat_Status status;

    bool czy_zdal_mature;
    bool czy_powtarza_egzamin;

    int wynik_a;
    int wynik_b;
    float wynik_koncowy;

} Kandydat;

bool losuj_czy_zdal_matura();
bool losuj_czy_powtarza_egzamin();
void init_kandydat(pid_t pid, Kandydat *k);

typedef struct
{
    Kandydat LISTA_KANDYDACI[LICZBA_KANDYDATOW];
    Kandydat LISTA_ODRZUCONYCH[LICZBA_KANDYDATOW];
} PamiecDzielona;

typedef enum
{
    SEMAFOR_BUDYNEK,
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

// ---- Kolejki Komunikatow ----

typedef enum
{
    DZIEKAN = 1,
    KOMISJA_A_NADZORCA = 2,
    KOMISJA_A_CZLONEK_2 = 3,
    KOMISJA_A_CZLONEK_3 = 4,
    KOMISJA_A_CZLONEK_4 = 5,
    KOMISJA_A_CZLONEK_5 = 6,
    KOMISJA_B_NADZORCA = 7,
    KOMISJA_B_CZLONEK_2 = 8,
    KOMISJA_B_CZLONEK_3 = 9

} MSG_Odbiorca;

typedef struct
{
    long mtype;
    Kandydat kandydat;
} MSG_ZGLOSZENIE;

typedef struct
{
    long mtype;
    bool dopuszczony_do_egzamin;
    int numer_na_liscie;
} MSG_DECYZJA;

typedef struct
{
    long mtype;
    pid_t pid;
} MSG_POTWIERDZENIE;

int utworz_msq(key_t klucz_msq);
void msq_send(int msqid, void *msg, size_t msg_size);
void msq_receive(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci);
void usun_msq(int msqid);

#endif
