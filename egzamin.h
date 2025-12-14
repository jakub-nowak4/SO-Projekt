#ifndef EGZAMIN
#define EGZAMIN

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>

#define M 10                    // W docelowej symulacji M = 120
#define CZAS_OPRACOWNIE_PYTAN 5 // Czas Ti na opracownie pytan od komisji

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

#endif
