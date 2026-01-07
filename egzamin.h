#ifndef EGZAMIN_H
#define EGZAMIN_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <pthread.h>

#define M 300
#define POJEMNOSC_BUDYNKU 100
#define LICZBA_KANDYDATOW (10 * M)
#define CZAS_OPRACOWANIE_PYTAN 5
#define LICZBA_CZLONKOW_A 5
#define LICZBA_CZLONKOW_B 3
#define GODZINA_ROZPOCZECIA_EGZAMINU 5

#define MTYPE_BUDYNEK_DECYZJA 10000
#define MTYPE_A_POTWIERDZENIE 20000
#define MTYPE_A_WERYFIKACJA_ODP 21000
#define MTYPE_A_PYTANIE 22000
#define MTYPE_A_WYNIK 23000
#define MTYPE_A_WYNIK_KONCOWY 24000
#define MTYPE_B_POTWIERDZENIE 30000
#define MTYPE_B_PYTANIE 31000
#define MTYPE_B_WYNIK 32000
#define MTYPE_B_WYNIK_KONCOWY 33000

#define LOGI_DIR "logi"
#define LOGI_MAIN "logi/logi_main.txt"
#define LOGI_DZIEKAN "logi/logi_dziekan.txt"
#define LOGI_KANDYDACI "logi/logi_kandydaci.txt"
#define LOGI_KOMISJA_A "logi/logi_komisja_a.txt"
#define LOGI_KOMISJA_B "logi/logi_komisja_b.txt"
#define LOGI_LISTA_RANKINGOWA "logi/logi_lista_rankingowa.txt"
#define LOGI_LISTA_ODRZUCONYCH "logi/logi_lista_odrzuconych.txt"

extern int semafor_id;
extern int shmid;

typedef enum
{
    MSQ_KOLEJKA_BUDYNEK = 8,
    MSQ_KOLEJKA_EGZAMIN_A = 9,
    MSQ_KOLEJKA_EGZAMIN_B = 10,
    MSQ_DZIEKAN_KOMISJA = 11
} MSQ;

typedef struct
{
    pid_t pid;
    int numer_na_liscie;
    bool czy_zdal_mature;
    bool czy_powtarza_egzamin;
    float wynik_a;
    float wynik_b;
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

typedef struct
{
    pid_t pid;
    int numer_na_liscie;
    bool czy_dostal_pytanie[LICZBA_CZLONKOW_B];
    int oceny[LICZBA_CZLONKOW_B];
    int liczba_ocen;
    float wynik_koncowy_za_B;
} Sala_B;

bool losuj_czy_zdal_matura();
bool losuj_czy_powtarza_egzamin();

typedef struct
{
    bool egzamin_trwa;
    int index_kandydaci;
    int index_odrzuceni;
    int index_rankingowa;
    Kandydat LISTA_KANDYDACI[LICZBA_KANDYDATOW];
    Kandydat LISTA_ODRZUCONYCH[LICZBA_KANDYDATOW];
    Kandydat LISTA_RANKINGOWA[LICZBA_KANDYDATOW];
    int nastepny_do_komisja_A;
    int liczba_osob_w_A;
    int liczba_osob_w_B;
    int kandydatow_procesow;
} PamiecDzielona;

typedef enum
{
    SEMAFOR_STD_OUT,
    SEMAFOR_LOGI_MAIN,
    SEMAFOR_LOGI_DZIEKAN,
    SEMAFOR_LOGI_KANDYDACI,
    SEMAFOR_LOGI_KOMISJA_A,
    SEMAFOR_LOGI_KOMISJA_B,
    SEMAFOR_LOGI_LISTA_RANKINGOWA,
    SEMAFOR_LOGI_LISTA_ODRZUCONYCH,
    SEMAFOR_MUTEX,
    SEMAFOR_ODPOWIEDZ_A,
    SEMAFOR_ODPOWIEDZ_B,
    SEMAFOR_DZIEKAN_GOTOWY,
    SEMAFOR_KOMISJA_A_GOTOWA,
    SEMAFOR_KOMISJA_B_GOTOWA,
    SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM
} Semafory;

void pobierz_czas(struct tm *wynik);
void loguj(int sem_index, char *file_path, char *msg);
void wypisz_wiadomosc(char *msg);

key_t utworz_klucz(int arg);
void usun_semafory(void);
void utworz_semafory(key_t klucz_sem);
void semafor_p(int semNum);
void semafor_v(int semNum);
int semafor_wartosc(int semNum);

void utworz_shm(key_t klucz_shm);
void dolacz_shm(PamiecDzielona **wsk);
void odlacz_shm(PamiecDzielona *adr);
void usun_shm(void);

typedef enum
{
    KANDYDAT_PRZESYLA_MATURE = 100,
    KANDYDAT_WCHODZI_DO_A = 101,
    NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO = 102,
    KANDYDAT_WCHODZI_DO_B = 103,
    NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA = 104
} MSG_MTYPE;

typedef struct
{
    long mtype;
    pid_t pid;
    bool matura;
    bool czy_powtarza_egzamin;
    float wynik_a;
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
} MSG_KANDYDAT_WCHODZI_DO_A_POTWIERDZENIE;

typedef struct
{
    long mtype;
    pid_t pid;
    int numer_na_liscie;
} MSG_KANDYDAT_WCHODZI_DO_B;

typedef struct
{
    long mtype;
} MSG_KANDYDAT_WCHODZI_DO_B_POTWIERDZENIE;

typedef struct
{
    long mtype;
    pid_t pid;
    float wynik_a;
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
    bool czy_zdal;
    float wynik_koncowy;
} MSG_WYNIK_KONCOWY;

typedef struct
{
    long mtype;
    float wynik_koncowy;
    pid_t pid;
    char komisja;
} MSG_WYNIK_KONCOWY_DZIEKAN;

int utworz_msq(key_t klucz_msq);
void msq_send(int msqid, void *msg, size_t msg_size);
void msq_receive(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci);
ssize_t msq_receive_no_wait(int msqid, void *buffer, size_t buffer_size, long typ_wiadomosci);
void usun_msq(int msqid);

int znajdz_kandydata(pid_t pid, PamiecDzielona *shm);
void wypisz_liste_rankingowa(PamiecDzielona *pamiec_shm);

#endif