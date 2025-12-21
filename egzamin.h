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
#include <pthread.h>

#define M 12 // W docelowej symulacji M = 120
#define LICZBA_KANDYDATOW (10 * M)
#define CZAS_OPRACOWANIE_PYTAN 5 // Czas Ti na opracownie pytan od komisji
#define LICZBA_CZLONKOW_A 5
#define LICZBA_CZLONKOW_B 3

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
    MSQ_KOLEJKA_BUDYNEK = 8,
    MSQ_KOLEJKA_EGZAMIN_A = 9,
    MSQ_KOLEJKA_EGZAMIN_B = 10
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

typedef struct
{
    pid_t pid;
    int numer_na_liscie;
    bool czy_dostal_pytanie[LICZBA_CZLONKOW_A];
    int oceny[LICZBA_CZLONKOW_A];
    int liczba_ocen;
    float wynik_koncowy_za_A;
} Sala_A;

bool losuj_czy_zdal_matura();
bool losuj_czy_powtarza_egzamin();
void init_kandydat(pid_t pid, Kandydat *k);

typedef struct
{
    bool egzamin_trwa;
    int index_kandydaci;
    int index_odrzuceni;
    Kandydat LISTA_KANDYDACI[LICZBA_KANDYDATOW];
    Kandydat LISTA_ODRZUCONYCH[LICZBA_KANDYDATOW];

    int nastepny_do_komisja_A;
    int liczba_osob_w_A;
    int pozostalo_kandydatow;
} PamiecDzielona;

typedef enum
{
    SEMAFOR_BUDYNEK,
    SEMAFOR_STD_OUT,
    SEMAFOR_MUTEX,
    SEMAFOR_ODPOWIEDZ_A,
    SEMAFOR_ODPOWIEDZ_B,
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
    KANDYDAT_PRZESYLA_MATURE = 100,
    KANDYDAT_WCHODZI_DO_A = 101,
    NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO = 102

} MSG_MTYPE;

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

typedef struct
{
    long mtype;
    pid_t pid;
    int numer_na_liscie;
} MSG_KANDYDAT_WCHODZI_DO_A;

typedef struct
{
    long mtype;
    pid_t pid;
    float wynika_a;

} MSG_KANDYDAT_POWTARZA;

typedef struct
{
    long mtype;
    bool zgoda;
} MSG_KANDYDAT_POWTARZA_ODPOWIEDZ_NADZORCY;

typedef struct
{
    long mtype;
    pid_t pid;

} MSG_PYTANIE;

typedef struct
{
    long mtype;
    pid_t pid;
} MSG_ODPOWIEDZ;

typedef struct
{
    long mtype;
    int ocena;
    int numer_czlonka_komisj;
} MSG_WYNIK;

typedef struct
{
    long mtype;
    float wynik_koncowy;

} MSG_WYNIK_A;

int utworz_msq(key_t klucz_msq);
void msq_send(int msqid, void *msg, size_t msg_size);
void msq_receive(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci);
ssize_t msq_receive_no_wait(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci);
void usun_msq(int msqid);

#endif
