# Projekt Systemy Operacyjne - Temat 17: Egzamin Wstępny

## Wersja i dystrybucja

| Parametr | Wartość |
|----------|---------|
| Operating System | Ubuntu 24.04.1 LTS |
| Virtualization | WSL |
| Kernel | Linux 6.6.87.2-microsoft-standard-WSL2 |
| Architecture | x86-64 |
| Kompilator | gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0 |

### Wymagania wstępne

* `gcc` - kompilator języka C
* `make` - narzędzie do automatycznej kompilacji

### Uruchomienie symulacji

W katalogu projektu należy wykonać polecenie:
```bash
make clean
make
./main
```

### Ewakuacja podczas symulacji

Aby wywołać ewakuację należy użyć jednej z opcji:
- `Ctrl+C` - wysyła sygnał `SIGINT` do procesu głównego
- `kill -SIGUSR2 <PID_DZIEKANA>` - bezpośrednie wywołanie ewakuacji

---

## 1. Cel Projektu

Celem projektu było stworzenie wieloprocesowej symulacji egzaminu wstępnego na kierunek informatyka w środowisku systemu Linux. Program ma za zadanie odwzorować realne zależności czasowe, zarządzanie ograniczonymi zasobami (miejsca w salach) oraz komunikację między różnymi podmiotami (kandydaci, komisje, dziekan).


## 2. Założenia Techniczne

Projekt został zrealizowany w oparciu o niskopoziomowe mechanizmy systemu Linux/Unix.

### 2.1 Architektura wieloprocesowa

Zgodnie z wymaganiami projekt unika rozwiązań scentralizowanych. Każdy element symulacji jest osobnym procesem tworzonym przez wywołania systemowe `fork()` i `exec()`:

| Proces | Plik źródłowy | Opis |
|--------|---------------|------|
| **main** | `main.c` | Proces główny - koordynator |
| **dziekan** | `dziekan.c` | Weryfikacja matury, lista rankingowa, zarządzanie ewakuacją |
| **komisja_a** | `komisja_a.c` | Komisja teoretyczna (5 wątków) |
| **komisja_b** | `komisja_b.c` | Komisja praktyczna (3 wątki) |
| **kandydat** | `kandydat.c` | Proces kandydata (10×M instancji) |

### 2.2 Wielowątkowość komisji

Każdy proces komisji jest wielowątkowy. Wykorzystano bibliotekę `pthread` do stworzenia wątków reprezentujących członków komisji. Synchronizacja między wątkami odbywa się za pomocą mutexów (`pthread_mutex_t`).

#### Komisja A (część teoretyczna)

Proces `komisja_a` tworzy **5 wątków** reprezentujących członków komisji teoretycznej:

| Wątek | Funkcja | Rola |
|-------|---------|------|
| Wątek 0 | `nadzorca()` | Przewodniczący komisji |
| Wątek 1 | `czlonek()` | Egzaminator |
| Wątek 2 | `czlonek()` | Egzaminator |
| Wątek 3 | `czlonek()` | Egzaminator |
| Wątek 4 | `czlonek()` | Egzaminator |

**Nadzorca Komisji A (wątek 0):**
- Wpuszcza kandydatów do sali (maksymalnie 3 jednocześnie)
- Weryfikuje kandydatów powtarzających egzamin (sprawdza poprawność wyniku z poprzedniej sesji)
- Zadaje pytania kandydatom (tak jak pozostali członkowie)
- Odbiera i ocenia odpowiedzi kandydatów
- Oblicza wynik końcowy (średnia arytmetyczna 5 ocen)
- Przesyła wynik końcowy do kandydata i do dziekana
- Zwalnia miejsce w sali po zakończeniu egzaminu kandydata

**Członkowie Komisji A (wątki 1-4):**
- Zadają pytania kandydatom gotowym do odpowiadania
- Odbierają odpowiedzi od kandydatów
- Oceniają odpowiedzi losowo (0-100%)
- Wysyłają oceny do kandydata

**Zmienne współdzielone w Komisji A:**
- `miejsca[3]` - tablica 3 slotów dla kandydatów w sali (typ `Sala_A`)
- `kandydat_gotowy[3]` - flagi gotowości kandydatów
- `kandydat_odpowiada` - PID kandydata aktualnie odpowiadającego
- `mutex` - mutex do synchronizacji dostępu do zmiennych

#### Komisja B (część praktyczna)

Proces `komisja_b` tworzy **3 wątki** reprezentujących członków komisji praktycznej:

| Wątek | Funkcja | Rola |
|-------|---------|------|
| Wątek 0 | `nadzorca()` | Przewodniczący komisji |
| Wątek 1 | `czlonek()` | Egzaminator |
| Wątek 2 | `czlonek()` | Egzaminator |

**Nadzorca Komisji B (wątek 0):**
- Wpuszcza kandydatów do sali (maksymalnie 3 jednocześnie)
- Zadaje pytania kandydatom (tak jak pozostali członkowie)
- Odbiera i ocenia odpowiedzi kandydatów
- Oblicza wynik końcowy (średnia arytmetyczna 3 ocen)
- Przesyła wynik końcowy do kandydata i do dziekana
- Zwalnia miejsce w sali po zakończeniu egzaminu kandydata

**Członkowie Komisji B (wątki 1-2):**
- Zadają pytania kandydatom gotowym do odpowiadania
- Odbierają odpowiedzi od kandydatów
- Oceniają odpowiedzi losowo (0-100%)
- Wysyłają oceny do kandydata

**Zmienne współdzielone w Komisji B:**
- `miejsca[3]` - tablica 3 slotów dla kandydatów w sali (typ `Sala_B`)
- `kandydat_gotowy[3]` - flagi gotowości kandydatów
- `kandydat_odpowiada` - PID kandydata aktualnie odpowiadającego
- `mutex` - mutex do synchronizacji dostępu do zmiennych

#### Różnice między Komisją A i B

| Aspekt | Komisja A | Komisja B |
|--------|-----------|-----------|
| Liczba wątków | 5 | 3 |
| Liczba pytań | 5 | 3 |
| Typ egzaminu | Teoretyczny | Praktyczny |
| Weryfikacja powtarzających | Tak | Nie |
| Próg zaliczenia | >= 30% | >= 30% |
| Kolejka komunikatów | `MSQ_KOLEJKA_EGZAMIN_A` | `MSQ_KOLEJKA_EGZAMIN_B` |
| Semafor odpowiedzi | `SEMAFOR_ODPOWIEDZ_A` | `SEMAFOR_ODPOWIEDZ_B` |

#### Synchronizacja wątków

Obie komisje używają funkcji `safe_mutex_lock()` która sprawdza flagę ewakuacji:

```c
int safe_mutex_lock(pthread_mutex_t *mtx) {
    if (ewakuacja_aktywna)
        return -1;
    pthread_mutex_lock(mtx);
    if (ewakuacja_aktywna) {
        pthread_mutex_unlock(mtx);
        return -1;
    }
    return 0;
}
```

Dzięki temu wątki mogą natychmiast przerwać pracę w przypadku ewakuacji, bez ryzyka zakleszczeń.

### 2.3 Komunikacja IPC (System V)

Do wymiany danych i synchronizacji między procesami wykorzystano mechanizmy Systemu V:

#### Kolejki komunikatów (Message Queues)

| Kolejka | Klucz | Zastosowanie |
|---------|-------|--------------|
| `MSQ_KOLEJKA_BUDYNEK` | 8 | Komunikacja kandydat ↔ dziekan |
| `MSQ_KOLEJKA_EGZAMIN_A` | 9 | Komunikacja kandydat ↔ komisja A |
| `MSQ_KOLEJKA_EGZAMIN_B` | 10 | Komunikacja kandydat ↔ komisja B |
| `MSQ_DZIEKAN_KOMISJA` | 11 | Wyniki z komisji do dziekana |

#### Pamięć dzielona (Shared Memory)

Struktura `PamiecDzielona` przechowuje:
```c
typedef struct {
    bool egzamin_trwa;                        // Flaga stanu egzaminu
    volatile bool ewakuacja;                  // Flaga ewakuacji
    int index_kandydaci;                      // Licznik kandydatów
    int index_odrzuceni;                      // Licznik odrzuconych
    int index_rankingowa;                     // Licznik na liście rankingowej
    Kandydat LISTA_KANDYDACI[LICZBA_KANDYDATOW];     // Lista wszystkich
    Kandydat LISTA_ODRZUCONYCH[LICZBA_KANDYDATOW];   // Lista odrzuconych
    Kandydat LISTA_RANKINGOWA[LICZBA_KANDYDATOW];    // Lista rankingowa
    int nastepny_do_komisja_A;                // Numer następnego kandydata
    int liczba_osob_w_A;                      // Osoby w sali A
    int liczba_osob_w_B;                      // Osoby w sali B
    int kandydatow_procesow;                  // Aktywne procesy kandydatów
} PamiecDzielona;
```

#### Semafory

| Semafor | Wartość początkowa | Zastosowanie |
|---------|-------------------|--------------|
| `SEMAFOR_STD_OUT` | 1 | Synchronizacja dostępu do stdout |
| `SEMAFOR_MUTEX` | 1 | Sekcja krytyczna pamięci dzielonej |
| `SEMAFOR_ODPOWIEDZ_A` | 1 | Jeden kandydat odpowiada w A |
| `SEMAFOR_ODPOWIEDZ_B` | 1 | Jeden kandydat odpowiada w B |
| `SEMAFOR_LOGI_*` | 1 | Synchronizacja plików logów |
| `SEMAFOR_DZIEKAN_GOTOWY` | 0 | Sygnalizacja gotowości |
| `SEMAFOR_KOMISJA_A_GOTOWA` | 0 | Sygnalizacja gotowości |
| `SEMAFOR_KOMISJA_B_GOTOWA` | 0 | Sygnalizacja gotowości |
| `SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM` | `POJEMNOSC_BUDYNKU` | Limit osób w budynku |
| `SEMAFOR_KONIEC_KANDYDATOW` | 0 | Licznik zakończonych kandydatów |
| `SEMAFOR_KOMISJA_A_KONIEC` | 0 | Sygnalizacja końca komisji A |
| `SEMAFOR_KOMISJA_B_KONIEC` | 0 | Sygnalizacja końca komisji B |

### 2.4 Bezpieczeństwo i obsługa błędów

Wszystkie kluczowe wywołania systemowe są weryfikowane. W przypadku błędu następuje:
- Wypisanie komunikatu diagnostycznego (`perror`)
- Odpowiedni kod błędu (`errno`)
- Bezpieczne zakończenie procesu

---

## 3. Logika Symulacji i Wymagania Funkcjonalne

### 3.1 Stałe konfiguracyjne

| Stała | Wartość | Opis |
|-------|---------|------|
| `M` | 100 | Liczba miejsc do przyjęcia |
| `POJEMNOSC_BUDYNKU` | 500 | Max kandydatów w budynku |
| `LICZBA_KANDYDATOW` | 10×M | Całkowita liczba kandydatów |
| `LICZBA_CZLONKOW_A` | 5 | Członkowie komisji A |
| `LICZBA_CZLONKOW_B` | 3 | Członkowie komisji B |

### 3.2 Inicjalizacja i Status Kandydata

* **Liczba kandydatów:** 10×M (stosunek 10 chętnych na 1 miejsce).
* **Weryfikacja Matury:** Każdy kandydat losuje status matury ~2% nie spełnia wymogu - są weryfikowani i odrzucani przez dziekana.
* **Kandydaci "Powtarzający":** ~2% kandydatów ma zaliczoną teorię. Zgłaszają się do Komisji A tylko w celu weryfikacji, następnie przechodzą bezpośrednio do Komisji B.

### 3.3 Przebieg Egzaminu

Przebieg egzaminu dla każdego kandydata składa się z następujących kroków:

1. **Start procesu kandydata** - proces kandydata zostaje utworzony przez `main` za pomocą `fork()` i `exec()`

2. **Kolejka przed budynkiem** - kandydat czeka na wejście do budynku. Semafor `SEMAFOR_KOLEJKA_PRZED_BUDYNKIEM` ogranicza liczbę kandydatów w budynku do `POJEMNOSC_BUDYNKU` (500)

3. **Weryfikacja matury** - kandydat wysyła zgłoszenie do dziekana zawierające informację o maturze. Dziekan sprawdza czy kandydat zdał maturę:
   - **Brak matury** → kandydat zostaje **odrzucony** i kończy proces
   - **Zdana matura** → kandydat otrzymuje numer na liście i przechodzi dalej

4. **Kolejka do Komisji A** - kandydat czeka aż jego numer będzie równy `nastepny_do_komisja_A`. Gwarantuje to kolejność FIFO

5. **Rozgałęzienie - typ kandydata:**
   - **Kandydat powtarzający egzamin** (~2%) → zgłasza się do nadzorcy Komisji A celem weryfikacji poprzedniego wyniku. Jeśli weryfikacja pozytywna, przechodzi bezpośrednio do kroku 8
   - **Normalny kandydat** → przechodzi do kroku 6

6. **Egzamin w Komisji A** (część teoretyczna):
   - Kandydat wchodzi do sali (max 3 osoby jednocześnie)
   - Sygnalizuje gotowość do otrzymywania pytań
   - Odbiera 5 pytań od członków komisji
   - Przygotowuje odpowiedzi
   - Udziela odpowiedzi każdemu członkowi komisji
   - Odbiera 5 ocen od członków komisji
   - Odbiera wynik końcowy (średnia z 5 ocen)

7. **Decyzja po Komisji A:**
   - **Wynik < 30%** → kandydat zostaje **odrzucony** i kończy proces
   - **Wynik >= 30%** → kandydat przechodzi do Komisji B

8. **Egzamin w Komisji B** (część praktyczna):
   - Kandydat wchodzi do sali (max 3 osoby jednocześnie)
   - Sygnalizuje gotowość do egzaminu
   - Odbiera 3 pytania od członków komisji
   - Przygotowuje odpowiedzi
   - Udziela odpowiedzi każdemu członkowi komisji
   - Odbiera 3 oceny od członków komisji
   - Odbiera wynik końcowy (średnia z 3 ocen)

9. **Decyzja po Komisji B:**
   - **Wynik < 30%** → kandydat zostaje **odrzucony**
   - **Wynik >= 30%** → kandydat zostaje wpisany na **listę rankingową**

10. **Zakończenie procesu** - kandydat zmniejsza licznik `kandydatow_procesow` w pamięci dzielonej, sygnalizuje zakończenie i kończy proces

### 3.4 Ograniczenie sali

Do sali każdej komisji może wejść jednocześnie maksymalnie **3 kandydatów**. Pozostali oczekują w kolejce. Limit jest kontrolowany przez:
- Zmienną `liczba_osob_w_A` / `liczba_osob_w_B` w pamięci dzielonej
- Semafor `SEMAFOR_MUTEX` chroni dostęp do tej zmiennej

### 3.5 Synchronizacja pytań

1. Kandydat nie może rozpocząć odpowiadania, dopóki nie otrzyma kompletu pytań:
   - 5 pytań od Komisji A
   - 3 pytania od Komisji B
2. Pytania są generowane losowo przez wątki komisji
3. Kandydat czeka na skompletowanie zestawu przed przygotowaniem odpowiedzi

### 3.6 Warunki Zaliczenia i Ocenianie

* **Ocena odpowiedzi:** Każda odpowiedź jest oceniana losowo w zakresie 0–100%
* **Wynik końcowy:** Średnia arytmetyczna ocen wszystkich członków komisji
* **Próg przejścia do części B:** wynik z A >= 30%
* **Próg wpisania na listę rankingową:** wynik z A >= 30% ORAZ wynik z B >= 30%
* **Wynik końcowy:** (wynik_A + wynik_B) / 2

### 3.7 Obsługa Ewakuacji

System obsługuje sygnały symulujące ewakuację budynku:

| Sygnał | Źródło | Akcja |
|--------|--------|-------|
| `SIGINT` (Ctrl+C) | Terminal | main → dziekan (SIGUSR2) |
| `SIGUSR2` | main → dziekan | Rozpoczęcie ewakuacji |
| `SIGTERM` | dziekan → wszystkie procesy | Natychmiastowe zakończenie |

---

## 4. Opis Szczegółowy Kodu

### 4.1 Pliki źródłowe

| Plik | Opis |
|------|------|
| **`main.c`** | Inicjalizacja IPC, tworzenie procesów, czekanie na zakończenie |
| **`dziekan.c`** | Weryfikacja matur, odbiór wyników, lista rankingowa |
| **`komisja_a.c`** | Komisja teoretyczna (5 wątków) |
| **`komisja_b.c`** | Komisja praktyczna (3 wątki) |
| **`kandydat.c`** | Symulacja zachowania kandydata |
| **`egzamin.c`** | Biblioteka funkcji pomocniczych |
| **`egzamin.h`** | Definicje struktur, stałych i prototypów |

### 4.2 Algorytm wpuszczania do sali A (FIFO)

Kolejność kandydatów jest gwarantowana przez mechanizm numerów na liście:

```
ALGORYTM KOLEJKA_FIFO_DO_KOMISJI_A:

    [W PROCESIE DZIEKANA - przy akceptacji kandydata]
    1. Atomowo pobierz i zwiększ index_kandydaci
    2. Przypisz kandydatowi numer_na_liscie = index_kandydaci
    3. Wyślij decyzję do kandydata
    
    [W PROCESIE KANDYDATA - oczekiwanie na wejście]
    4. WHILE (true):
        a. ZABLOKUJ SEMAFOR_MUTEX
        b. aktualny = pamiec_shm->nastepny_do_komisja_A
        c. JEŚLI aktualny == mój_numer_na_liscie:
            - moja_kolej = true
            - pamiec_shm->nastepny_do_komisja_A++
        d. ODBLOKUJ SEMAFOR_MUTEX
        e. JEŚLI moja_kolej:
            - Wyślij zgłoszenie do KOMISJA_A
            - Czekaj na potwierdzenie
            - BREAK
    
    [W PROCESIE KOMISJA_A - nadzorca]
    5. WHILE (liczba_osob < 3):
        a. Odbierz zgłoszenie z kolejki (msq_receive_no_wait)
        b. ZABLOKUJ SEMAFOR_MUTEX + MUTEX
        c. Znajdź wolne miejsce (slot) w tablicy miejsca[3]
        d. Zapisz dane kandydata
        e. pamiec_shm->liczba_osob_w_A++
        f. ODBLOKUJ MUTEX + SEMAFOR_MUTEX
        g. Wyślij potwierdzenie do kandydata
```

**Gwarancja FIFO:**
- Numer na liście jest przydzielany sekwencyjnie przez dziekana
- Kandydat może zgłosić się do komisji tylko gdy jego numer == `nastepny_do_komisja_A`
- Po zgłoszeniu, `nastepny_do_komisja_A` jest atomowo zwiększany

### 4.3 Algorytm przydzielania miejsca przez Nadzorcę

```
FUNKCJA nadzorca_przydziel_miejsce():
    
    // 1. Sprawdź czy jest wolne miejsce
    IF liczba_osob_w_A < 3 THEN:
        
        // 2. Odbierz zgłoszenie bez blokowania
        prosba = msq_receive_no_wait(KANDYDAT_WCHODZI_DO_A)
        IF prosba == NULL THEN RETURN
        
        // 3. Sekcja krytyczna - znajdź wolny slot
        SEMAFOR_P(SEMAFOR_MUTEX)
        MUTEX_LOCK(mutex)
        
        slot = -1
        FOR i = 0 TO 2:
            IF miejsca[i].pid == 0 THEN:
                miejsca[i].pid = prosba.pid
                miejsca[i].numer_na_liscie = prosba.numer_na_liscie
                miejsca[i].liczba_ocen = 0
                
                // Zresetuj flagi pytań
                FOR j = 0 TO LICZBA_CZLONKOW-1:
                    miejsca[i].czy_dostal_pytanie[j] = false
                    miejsca[i].oceny[j] = 0
                
                kandydat_gotowy[i] = false
                pamiec_shm->liczba_osob_w_A++
                slot = i
                BREAK
        
        MUTEX_UNLOCK(mutex)
        SEMAFOR_V(SEMAFOR_MUTEX)
        
        // 4. Wyślij potwierdzenie (poza sekcją krytyczną!)
        IF slot != -1 THEN:
            potwierdzenie.mtype = MTYPE_A_POTWIERDZENIE + prosba.pid
            msq_send(msqid_A, potwierdzenie)
```


### 4.4 Synchronizacja logowania

Przy wielu procesach równoczesne pisanie na `stdout` powodowało "rozjeżdżanie się" tekstu. Zastosowano funkcję `loguj()` która synchronizuje dostęp do terminala i plików za pomocą semaforów.

Każdy plik logu ma dedykowany semafor, a dodatkowo semafor `SEMAFOR_STD_OUT` synchronizuje dostęp do standardowego wyjścia. Wiadomości są formatowane z precyzyjnym czasem (milisekundy) i kolorowane w zależności od źródła.



### 4.7 Offset na mtype wiadomości

Aby umożliwić adresowanie wiadomości do konkretnych kandydatów, używane są offsety:

```c
#define MTYPE_BUDYNEK_DECYZJA 100000      
#define MTYPE_A_POTWIERDZENIE 200000      
#define MTYPE_A_WERYFIKACJA_ODP 300000    
#define MTYPE_A_PYTANIE 400000            
#define MTYPE_A_WYNIK 500000              
#define MTYPE_A_WYNIK_KONCOWY 600000      
#define KANDYDAT_GOTOWY_A 700000          
#define MTYPE_B_POTWIERDZENIE 800000      
#define MTYPE_B_PYTANIE 900000            
#define MTYPE_B_WYNIK 1000000             
#define MTYPE_B_WYNIK_KONCOWY 1100000     
#define KANDYDAT_GOTOWY_B 1200000         
```

**Przykład:**
```c
// Komisja wysyła pytanie do kandydata o PID 12345
pytanie.mtype = MTYPE_A_PYTANIE + 12345;  // = 412345
msq_send(msqid_A, &pytanie, sizeof(pytanie));

// Kandydat 12345 odbiera pytanie
msq_receive(msqid_A, &pytanie, sizeof(pytanie), MTYPE_A_PYTANIE + getpid());
```

---

## 5. Problemy Napotkane w Projekcie i Rozwiązania

### Problem 1: Procesy zombie

**Opis:** Przy dużej liczbie kończących się procesów kandydatów, tworzyły się procesy zombie zajmujące zasoby systemowe.

**Rozwiązanie:** Wprowadzenie asynchronicznego wątku w procesie `main`, który odpowiedzialny jest za ciągłe zbieranie zakończonych procesów (zombie). Wątek ten działa w tle i cyklicznie wywołuje funkcję `waitpid()` z flagą `WNOHANG`:

```c
void *zbieraj_procesy(void *arg)
{
    (void)arg;
    char msg_buffer[512];

    while (watek_dziala)
    {
        int status;
        int zebrane_w_iteracji = 0;

        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        {
            zebrane_w_iteracji++;
        }

        if (zebrane_w_iteracji > 0)
        {
            pthread_mutex_lock(&mutex_procesow);
            liczba_zakonczonych_procesow += zebrane_w_iteracji;
            int zakonczone = liczba_zakonczonych_procesow;
            int utworzone = liczba_utworzonych_procesow;

            if (zakonczone >= utworzone || zakonczone % 100 == 0)
            {
                pthread_cond_signal(&cond_procesow);
            }
            pthread_mutex_unlock(&mutex_procesow);

            if (zakonczone >= utworzone && utworzone > 0)
            {
                break;
            }

            continue;
        }

        if (pid == -1)
        {
            if (errno == ECHILD)
            {
                pthread_mutex_lock(&mutex_procesow);
                int zakonczone = liczba_zakonczonych_procesow;
                int utworzone = liczba_utworzonych_procesow;
                pthread_mutex_unlock(&mutex_procesow);

                if (zakonczone >= utworzone && utworzone > 0)
                {
                    break;
                }
            }
        }
    }
    
    // Oczekiwanie na resztę procesów (jeśli jakieś zostały)
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
    {
        pthread_mutex_lock(&mutex_procesow);
        liczba_zakonczonych_procesow++;
        pthread_mutex_unlock(&mutex_procesow);
    }

    pthread_mutex_lock(&mutex_procesow);
    pthread_cond_signal(&cond_procesow);
    pthread_mutex_unlock(&mutex_procesow);

    return NULL;
}
```

---

### Problem 2: Blokowanie SIGINT przed startem egzaminu

**Opis:** Ewakuacja (Ctrl+C) mogła zostać wywołana przed rozpoczęciem egzaminu, gdy procesy nie były jeszcze gotowe do prawidłowej obsługi sygnału.

**Rozwiązanie:** Blokowanie sygnału `SIGINT` za pomocą `sigprocmask()` na początku programu `main`. Sygnał jest odblokowywany dopiero po:
1. Utworzeniu wszystkich procesów potomnych
2. Wysłaniu sygnału `SIGUSR1` do dziekana (start egzaminu)

```c
// Na początku main() - blokujemy SIGINT
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
    perror("sigprocmask(SIG_BLOCK)");
    exit(EXIT_FAILURE);
}

// ... tworzenie procesów i inicjalizacja ...

// Po wysłaniu SIGUSR1 do dziekana - odblokowujemy SIGINT
if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
    perror("sigprocmask(SIG_UNBLOCK)");
    exit(EXIT_FAILURE);
}
```

Dzięki temu ewakuacja może zostać wywołana tylko gdy egzamin już trwa i wszystkie procesy są gotowe do jej obsługi.

---

### Problem 3: Przepełnienie kolejki komunikatów

**Opis:** Przy dużej skali (M=500+) kolejki komunikatów przepełniały się (limit systemowy).

**Rozwiązanie:**
1. Wprowadzenie `POJEMNOSC_BUDYNKU` < `MAX_POJEMNOSC_BUDYNKU` aby ograniczyć liczbę jednoczesnych kandydatów
2. Walidacja przy starcie programu:

```c
if (POJEMNOSC_BUDYNKU >= MAX_POJEMNOSC_BUDYNKU) {
    fprintf(stderr, "BLAD: POJEMNOSC_BUDYNKU (%d) musi byc < %d!\n", 
            POJEMNOSC_BUDYNKU, MAX_POJEMNOSC_BUDYNKU);
    exit(EXIT_FAILURE);
}
```

---

### Problem 4: Problemy z `SEM_UNDO` przy sygnalizacji zakończenia

**Opis:** Semafory z flagą `SEM_UNDO` były automatycznie cofane przy zakończeniu procesu, co powodowało nieprawidłowe zliczanie zakończonych kandydatów.

**Rozwiązanie:** Wprowadzenie funkcji `semafor_v_bez_undo()` do sygnalizacji zakończenia:

```c
void semafor_v_bez_undo(int semNum) {
    struct sembuf buffer;
    buffer.sem_num = semNum;
    buffer.sem_op = 1;
    buffer.sem_flg = 0;  // Bez SEM_UNDO!
    
    while (semop(semafor_id, &buffer, 1) == -1) {
        if (errno == EINTR) continue;
        if (errno == EINVAL || errno == EIDRM) return;
        exit(EXIT_FAILURE);
    }
}
```

---

## 6. Testy

### Test 1: Test obciążeniowy (M = 1800)

* **Opis:** Symulacja w dużej skali
* **Stałe:** `M=1800`, `LICZBA_KANDYDATOW=18000`
* **Oczekiwany rezultat:** Symulacja poprawnie się kończy bez zakleszczeń
* **Weryfikacja:**
```
========== PODSUMOWANIE ==========
Kandydatow lacznie: 18000
Zdalo egzamin (lista rankingowa): 14544
Przyjetych (TOP 1800): 1800
Nieprzyjetych (brak miejsc): 12744
Odrzuconych (nie zdali): 3456

[DZIEKAN] Komisje zakonczone. Dziekan konczy prace.
[main] PODSUMOWANIE: Utworzono 18003 procesów, zakończyło się 18003 procesów
```
* **Wynik:** ✅ **Zaliczony**

---

### Test 2: Limit miejsc w Komisji A (max 3 osoby)

* **Opis:** W sali egzaminacyjnej A może przebywać maksymalnie 3 kandydatów jednocześnie
* **Stałe:** `M=120`, `LICZBA_KANDYDATOW=1200`
* **Weryfikacja w logach:**
```
14:39:31:46 | [Nadzorca A] PID:115518 | Wpuscilem kandydata PID:115527 Nr:0 do sali.
14:39:31:46 | [Nadzorca A] PID:115518 | Wpuscilem kandydata PID:115526 Nr:1 do sali.
14:39:31:46 | [Nadzorca A] PID:115518 | Wpuscilem kandydata PID:115523 Nr:2 do sali.
14:39:31:47 | [Nadzorca A] PID:115518 | Kandydat PID:115527 Nr:0 otrzymal wynik koncowy...
14:39:31:47 | [Nadzorca A] PID:115518 | Wpuscilem kandydata PID:115522 Nr:3 do sali.
```
* **Wynik:** ✅ **Zaliczony**

---

### Test 3: Limit miejsc w Komisji B (max 3 osoby)

* **Opis:** W sali egzaminacyjnej B może przebywać maksymalnie 3 kandydatów jednocześnie
* **Stałe:** `M=120`, `LICZBA_KANDYDATOW=1200`
* **Weryfikacja w logach:**
```
14:39:31:48 | [Nadzorca B] PID:115519 | Do sali wchodzi kandydat PID:115520 Nr:5
14:39:31:48 | [Nadzorca B] PID:115519 | Do sali wchodzi kandydat PID:115524 Nr:6
14:39:31:48 | [Nadzorca B] PID:115519 | Do sali wchodzi kandydat PID:115521 Nr:7
14:39:31:48 | [Nadzorca B] PID:115519 | Kandydat PID:115520 Nr:5 otrzymal wynik koncowy...
14:39:31:48 | [Nadzorca B] PID:115519 | Do sali wchodzi kandydat PID:115525 Nr:8
```
* **Wynik:** ✅ **Zaliczony**

---

### Test 4: ~2% kandydatów bez matury

* **Opis:** Około 2% kandydatów nie ma zdanej matury i nie jest dopuszczonych do egzaminu
* **Stałe:** `M=120`, `LICZBA_KANDYDATOW=1200`
* **Oczekiwany rezultat:** Około 24 kandydatów odrzuconych z powodu braku matury
* **Weryfikacja:**
```bash
$ grep "brak matury" logi/logi_dziekan.txt | wc -l
25
```
* **Wynik:** ✅ **Zaliczony**

---

### Test 5: ~2% kandydatów powtarza egzamin

* **Opis:** Około 2% kandydatów ma już zaliczoną część teoretyczną
* **Stałe:** `M=120`, `LICZBA_KANDYDATOW=1200`
* **Oczekiwany rezultat:** Około 24 kandydatów przechodzi bezpośrednio do komisji B gdy nadzorca komisji A zatwierdzi weryfikacje
* **Weryfikacja:**
```bash
$ grep "Mam zdana czesc teoretyczna" logi/logi_kandydaci.txt | wc -l
24
```
* **Wynik:** ✅ **Zaliczony**

---

### Test 6: Ewakuacja (Ctrl+C)

* **Opis:** Podczas symulacji użytkownik wciska Ctrl+C
* **Stałe:** `M=120`, `LICZBA_KANDYDATOW=1200`
* **Oczekiwany rezultat:**
  - Wszystkie procesy kończą działanie
  - Publikowana jest lista ewakuacyjna (częściowa)
  - Zasoby IPC są usuwane
* **Weryfikacja:**
```
========== PODSUMOWANIE ==========
Kandydatow lacznie: 1200
Zdalo egzamin (lista rankingowa): 624
Przyjetych (TOP 120): 120
Odrzuconych (nie zdali): 576

[DZIEKAN] Ewakuacja w trakcie egzaminu - przerywam egzamin i generuje ranking
[main] PODSUMOWANIE: Utworzono 1203 procesów, zakończyło się 1203 procesów

$ grep -c "Ewakuacja - opuszczam budynek" logi/logi_kandydaci.txt
412

$ ipcs -a
------ Message Queues --------
------ Shared Memory Segments --------
------ Semaphore Arrays --------
```
* **Wynik:** ✅ **Zaliczony**

---

### Test 7: Kolejność FIFO w Komisji A

* **Opis:** Kandydaci są obsługiwani w kolejności zgłoszeń (według numeru na liście)
* **Stałe:** `M=120`, `LICZBA_KANDYDATOW=1200`
* **Weryfikacja:**
```

✅ SUKCES: Kolejność FIFO zachowana!
```
* **Przykład z logów (numery rosnące: 0, 1, 2, 3...):**
```
14:39:31:46 | [Nadzorca A] Wpuscilem kandydata PID:115527 Nr:0 do sali.
14:39:31:46 | [Nadzorca A] Wpuscilem kandydata PID:115526 Nr:1 do sali.
14:39:31:46 | [Nadzorca A] Wpuscilem kandydata PID:115523 Nr:2 do sali.
14:39:31:47 | [Nadzorca A] Wpuscilem kandydata PID:115522 Nr:3 do sali.
14:39:31:47 | [Nadzorca A] Wpuscilem kandydata PID:115528 Nr:4 do sali.
14:39:31:47 | [Nadzorca A] Wpuscilem kandydata PID:115520 Nr:5 do sali.
```
* **Wynik:** ✅ **Zaliczony**

---

## 7. Pliki logów

Symulacja tworzy katalog `logi/` z następującymi plikami:

| Plik | Zawartość |
|------|-----------|
| `logi_main.txt` | Logi procesu głównego |
| `logi_dziekan.txt` | Logi dziekana (weryfikacja matur, wyniki) |
| `logi_kandydaci.txt` | Logi wszystkich kandydatów |
| `logi_komisja_a.txt` | Logi Komisji A |
| `logi_komisja_b.txt` | Logi Komisji B |
| `logi_lista_rankingowa.txt` | Finalna lista rankingowa |
| `logi_lista_odrzuconych.txt` | Lista odrzuconych kandydatów |


---

## 8. Wymagane funkcje systemowe i linki do kodu

### A. Tworzenie i obsługa plików

* **open()**
[main.c (Linia 67)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L67)
```c
int fd = open(files[i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
```
[egzamin.c (Linia 55)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L55)
```c
int fd = open(file_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
```

* **close()**
[main.c (Linia 69)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L69)
```c
close(fd);
```
[egzamin.c (Linia 59)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L59)
```c
close(fd);
```

* **write()**
[egzamin.c (Linia 58)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L58)
```c
(void)write(fd, buffer, len);
```
[egzamin.c (Linia 114)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L114)
```c
(void)write(STDOUT_FILENO, color_buffer, color_len);
```
[egzamin.c (Linia 137)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L137)
```c
if (write(STDOUT_FILENO, buffor, len) == -1)
```

### B. Tworzenie procesów

* **fork()**
[main.c (Linia 127) - Proces Dziekana](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L127)
```c
pid_dziekan = fork();
```
[main.c (Linia 148) - Procesy Komisji](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L148)
```c
switch (fork())
```
[main.c (Linia 176) - Procesy Kandydatów](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L176)
```c
switch (fork())
```

* **exec() (execl)**
[main.c (Linia 135) - Uruchomienie Dziekana](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L135)
```c
execl("./dziekan", "dziekan", NULL);
```
[main.c (Linia 156) - Uruchomienie Komisji A](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L156)
```c
execl("./komisja_a", "komisja_a", NULL);
```
[main.c (Linia 158) - Uruchomienie Komisji B](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L158)
```c
execl("./komisja_b", "komisja_b", NULL);
```
[main.c (Linia 183) - Uruchomienie Kandydata](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L183)
```c
execl("./kandydat", "kandydat", NULL);
```

* **exit()**
(Funkcja używana w obsłudze błędów we wszystkich plikach. Poniżej przykłady poprawnego zakończenia)

[main.c (Linia 137)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L137)
```c
exit(EXIT_SUCCESS);
```


* **wait() / waitpid()**
[main.c (Linia 271) - Wątek zbierający procesy](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L271)
```c
while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
```
[main.c (Linia 315) - Oczekiwanie końcowe](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L315)
```c
while (waitpid(-1, &status, WNOHANG) > 0)
```

### C. Tworzenie i obsługa wątków

* **pthread_create()**
[main.c (Linia 117)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L117)
```c
if (pthread_create(&watek_zbierajacy, NULL, zbieraj_procesy, NULL) != 0)
```
[komisja_a.c (Linia 124)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/komisja_a.c#L124)
```c
if (pthread_create(&watki_komisji[i], NULL, nadzorca, &numery_czlonkow[i]) != 0)
```
[komisja_b.c (Linia 132)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/komisja_b.c#L132)
```c
if (pthread_create(&watki_komisji[i], NULL, czlonek, &numery_czlonkow[i]) != 0)
```

* **pthread_join()**
[main.c (Linia 225)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L225)
```c
pthread_join(watek_zbierajacy, NULL);
```
[komisja_a.c (Linia 164)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/komisja_a.c#L164)
```c
int ret = pthread_join(watki_komisji[i], NULL);
```

* **pthread_mutex_lock()**
[komisja_a.c (Linia 21)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/komisja_a.c#L21)
```c
pthread_mutex_lock(mtx);
```

* **pthread_mutex_unlock()**
[komisja_a.c (Linia 24)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/komisja_a.c#L24)
```c
pthread_mutex_unlock(mtx);
```

### D. Obsługa sygnałów

* **kill()**
[main.c (Linia 201)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L201)
```c
if (kill(pid_dziekan, SIGUSR1) == -1)
```
[dziekan.c (Linia 359)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/dziekan.c#L359)
```c
kill(0, SIGTERM);
```

* **signal()**
[main.c (Linia 35)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/main.c#L35)
```c
if (signal(SIGINT, handler_sigint) == SIG_ERR)
```
[dziekan.c (Linia 48)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/dziekan.c#L48)
```c
if (signal(SIGINT, SIG_IGN) == SIG_ERR)
```

* **sigaction()**
[dziekan.c (Linia 19)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/dziekan.c#L19)
```c
if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1)
```

### E. Synchronizacja procesów (wątków)

* **ftok()**
[egzamin.c (Linia 161) - Wrapper `utworz_klucz`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L161)
```c
key_t klucz = ftok(".", arg);
```

* **semget()**
[egzamin.c (Linia 173) - Wrapper `utworz_semafory`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L173)
```c
semafor_id = semget(klucz_sem, 18, IPC_CREAT | IPC_EXCL | 0600);
```

* **semctl()**
[egzamin.c (Linia 193) - Inicjalizacja wartości semaforów](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L193)
```c
if (semctl(semafor_id, SEMAFOR_STD_OUT, SETVAL, 1) == -1)
```
[egzamin.c (Linia 309) - Usuwanie semaforów](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L309)
```c
if (semctl(semafor_id, 0, IPC_RMID) == -1)
```

* **semop()**
[egzamin.c (Linia 330) - Wrapper `semafor_p`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L330)
```c
while (semop(semafor_id, &buffer, 1) == -1)
```
[egzamin.c (Linia 383) - Wrapper `semafor_v`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L383)
```c
while (semop(semafor_id, &buffer, 1) == -1)
```

* **semafor_p_bez_ewakuacji()**
[egzamin.c (Linia 351)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L351)
```c
int semafor_p_bez_ewakuacji(int semNum)
```

* **semafor_v_bez_undo()**
[egzamin.c (Linia 399)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L399)
```c
void semafor_v_bez_undo(int semNum)
```

### G. Segmenty pamięci dzielonej

* **ftok()**
[egzamin.c (Linia 161)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L161)
```c
key_t klucz = ftok(".", arg);
```

* **shmget()**
[egzamin.c (Linia 423)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L423)
```c
shmid = shmget(klucz_shm, sizeof(PamiecDzielona), IPC_CREAT | IPC_EXCL | 0600);
```

* **shmat()**
[egzamin.c (Linia 445)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L445)
```c
*wsk = (PamiecDzielona *)shmat(shmid, NULL, 0);
```

* **shmdt()**
[egzamin.c (Linia 460)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L460)
```c
if (shmdt(adr) == -1)
```

* **shmctl()**
[egzamin.c (Linia 475)](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L475)
```c
if (shmctl(shmid, IPC_RMID, NULL) == -1)
```

### H. Kolejki komunikatów

* **ftok()**
[egzamin.c (Linia 161) - Wrapper `utworz_klucz`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L161)
```c
key_t klucz = ftok(".", arg);
```

* **msgget()**
[egzamin.c (Linia 486) - Wrapper `utworz_msq`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L486)
```c
int msqid = msgget(klucz_msq, IPC_CREAT | IPC_EXCL | 0600);
```

* **msgsnd()**
[egzamin.c (Linia 519) - Wrapper `msq_send`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L519)
```c
if (msgsnd(msqid, msg, msgsz, IPC_NOWAIT) == 0)
```

* **msgrcv()**
[egzamin.c (Linia 555) - Wrapper `msq_receive`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L555)
```c
res = msgrcv(msqid, buffer, buffer_size - sizeof(long), typ_wiadomosci, 0);
```

* **msgctl()**
[egzamin.c (Linia 602) - Wrapper `usun_msq`](https://github.com/jakub-nowak4/SO-Projekt/blob/59422bb98d6497c4b6757994d4d96ae143f2f540/egzamin.c#L602)
```c
if (msgctl(msqid, IPC_RMID, NULL) == -1)
```
