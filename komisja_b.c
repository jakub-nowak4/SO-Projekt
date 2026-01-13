#include "egzamin.h"

volatile sig_atomic_t egzamin_aktywny = true;
volatile sig_atomic_t ewakuacja_aktywna = false;
void handler_sigterm(int sigNum);
void handler_sigusr1(int sigNum);

int msqid_B = -1;
int msqid_dziekan_komisja = -1;
int numery_czlonkow[LICZBA_CZLONKOW_B] = {0};
PamiecDzielona *pamiec_shm;
Sala_B miejsca[3] = {0};
bool kandydat_gotowy[3] = {false, false, false};
pid_t kandydat_odpowiada = 0;

pthread_mutex_t mutex;

pthread_t watki_komisji[LICZBA_CZLONKOW_B];
volatile sig_atomic_t liczba_watkow = 0;
pthread_t watek_glowny;

int safe_mutex_lock(pthread_mutex_t *mtx)
{
    if (ewakuacja_aktywna)
        return -1;
    pthread_mutex_lock(mtx);
    if (ewakuacja_aktywna)
    {
        pthread_mutex_unlock(mtx);
        return -1;
    }
    return 0;
}

void *nadzorca(void *args);
void *czlonek(void *args);
int main()
{
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal() | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }
    struct sigaction sa;
    sa.sa_handler = handler_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction(SIGTERM) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGINT) | Nie udalo sie zignorowac SIGINT.");
        exit(EXIT_FAILURE);
    }
    struct sigaction sa_usr1;
    sa_usr1.sa_handler = handler_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1)
    {
        perror("sigaction(SIGUSR1) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    watek_glowny = pthread_self();

    char msg_buffer[200];

    srand(time(NULL) ^ getpid());

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    semafor_v(SEMAFOR_KOMISJA_B_GOTOWA);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    if (pamiec_shm->ewakuacja)
    {
        ewakuacja_aktywna = true;
    }

    pthread_mutex_init(&mutex, NULL);

    key_t klucz_msq_B = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_B);
    msqid_B = utworz_msq(klucz_msq_B);

    key_t klucz_msq_dziekan_komisja = utworz_klucz(MSQ_DZIEKAN_KOMISJA);
    msqid_dziekan_komisja = utworz_msq(klucz_msq_dziekan_komisja);

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Czekam na rozpoczecie egzaminu\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

    while (true)
    {
        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            fprintf(stderr, "[KOMISJA B] Blad semafora przy inicjalizacji pamieci dzielonej\n");
            break;
        }
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (egzamin_trwa)
        {
            break;
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Komisja rozpoczyna prace.\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

    for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
    {
        numery_czlonkow[i] = i;
        if (i == 0)
        {
            if (pthread_create(&watki_komisji[i], NULL, nadzorca, &numery_czlonkow[i]) != 0)
            {
                perror("pthread_create() | Nie udalo sie utworzyc watku nadzorca Komisji B.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (pthread_create(&watki_komisji[i], NULL, czlonek, &numery_czlonkow[i]) != 0)
            {
                perror("pthread_create() | Nie udalo sie utworzyc watku czlonek Komisji B.\n");
                exit(EXIT_FAILURE);
            }
        }
        liczba_watkow++;
    }

    while (egzamin_aktywny && !ewakuacja_aktywna)
    {
        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            fprintf(stderr, "[KOMISJA B] Blad semafora przy inicjalizacji pamieci dzielonej\n");
            break;
        }
        int procesow = pamiec_shm->kandydatow_procesow;
        int osoby_w_sali = pamiec_shm->liczba_osob_w_B;
        semafor_v(SEMAFOR_MUTEX);

        if (procesow == 0 && osoby_w_sali == 0)
        {
            egzamin_aktywny = false;
            break;
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Komisja konczy prace.\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

    for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
    {
        int ret = pthread_join(watki_komisji[i], NULL);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_join() | Nie udalo sie przylaczyc watku: %s\n", strerror(ret));
            exit(EXIT_FAILURE);
        }
    }

    pthread_mutex_destroy(&mutex);
    odlacz_shm(pamiec_shm);

    semafor_v_bez_undo(SEMAFOR_KOMISJA_B_KONIEC);

    return 0;
}

void *nadzorca(void *args)
{
    int numer_czlonka = *(int *)args;
    ssize_t res;
    char msg_buffer[200];

    while (!ewakuacja_aktywna)
    {
        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            fprintf(stderr, "[KOMISJA B] Blad semafora przy inicjalizacji pamieci dzielonej\n");
            break;
        }
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int liczba_osob = pamiec_shm->liczba_osob_w_B;
        int kandydatow = pamiec_shm->kandydatow_procesow;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa && kandydatow == 0 && liczba_osob == 0)
        {
            break;
        }

        if (liczba_osob < 3)
        {
            MSG_KANDYDAT_WCHODZI_DO_B zgloszenie_B;
            res = msq_receive_no_wait(msqid_B, &zgloszenie_B, sizeof(zgloszenie_B), KANDYDAT_WCHODZI_DO_B);

            if (res != -1)
            {
                int slot = -1;

                if (semafor_p(SEMAFOR_MUTEX) == -1)
                {
                    break;
                }
                if (safe_mutex_lock(&mutex) == -1)
                {
                    semafor_v(SEMAFOR_MUTEX);
                    break;
                }
                for (int i = 0; i < 3; i++)
                {
                    if (miejsca[i].pid == 0)
                    {
                        miejsca[i].pid = zgloszenie_B.pid;
                        miejsca[i].numer_na_liscie = zgloszenie_B.numer_na_liscie;
                        miejsca[i].liczba_ocen = 0;

                        for (int j = 0; j < LICZBA_CZLONKOW_B; j++)
                        {
                            miejsca[i].czy_dostal_pytanie[j] = false;
                            miejsca[i].oceny[j] = 0;
                        }

                        kandydat_gotowy[i] = false;
                        pamiec_shm->liczba_osob_w_B++;
                        slot = i;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);
                semafor_v(SEMAFOR_MUTEX);

                // Komunikat musi byc wysylany poza sekcja krytyczna jakb kolejka byla zapelniona
                if (slot != -1)
                {
                    MSG_KANDYDAT_WCHODZI_DO_B_POTWIERDZENIE potwierdzenie;
                    potwierdzenie.mtype = MTYPE_B_POTWIERDZENIE + zgloszenie_B.pid;
                    if (msq_send(msqid_B, &potwierdzenie, sizeof(potwierdzenie)) == -1)
                        break;

                    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B  NADZORCA] PID:%d | Do sali wchodzi kandydat PID:%d\n", getpid(), zgloszenie_B.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
                }
            }
        }

        // Odbieramy potwierdzenie gotowości od kandydata
        MSG_KANDYDAT_GOTOWY gotowy;
        ssize_t gotowy_res = msq_receive_no_wait(msqid_B, &gotowy, sizeof(gotowy), KANDYDAT_GOTOWY_B);
        if (gotowy_res != -1)
        {
            if (safe_mutex_lock(&mutex) == -1)
                break;
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid == gotowy.pid)
                {
                    kandydat_gotowy[i] = true;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        // Pytania wysyłane tylko gdy nikt nie odpowiada
        if (safe_mutex_lock(&mutex) == -1)
            break;
        bool ktos_odpowiada = (kandydat_odpowiada != 0);
        pthread_mutex_unlock(&mutex);

        if (!ktos_odpowiada)
        {
            pid_t kandydat_pid = 0;
            if (safe_mutex_lock(&mutex) == -1)
                break;
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid != 0 && kandydat_gotowy[i] && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
                {
                    kandydat_pid = miejsca[i].pid;
                    miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (kandydat_pid != 0)
            {
                MSG_PYTANIE pytanie;
                pytanie.mtype = MTYPE_B_PYTANIE + kandydat_pid;
                pytanie.pid = kandydat_pid;
                if (msq_send(msqid_B, &pytanie, sizeof(pytanie)) == -1)
                    break;

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d |Zadaje pytanie dla kandydata PID:%d\n", getpid(), kandydat_pid);
                loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            }
        }

        MSG_ODPOWIEDZ odpowiedz;
        res = msq_receive_no_wait(msqid_B, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);
        if (res != -1)
        {
            MSG_WYNIK wynik_do_wyslania;
            bool wyslij_wynik = false;
            pid_t pid_do_logu = 0;

            if (safe_mutex_lock(&mutex) == -1)
                break;

            if (kandydat_odpowiada == 0)
            {
                kandydat_odpowiada = odpowiedz.pid;
            }

            if (odpowiedz.pid == kandydat_odpowiada)
            {
                for (int i = 0; i < 3; i++)
                {
                    if (odpowiedz.pid == miejsca[i].pid)
                    {
                        int ocena = rand() % 101;
                        miejsca[i].oceny[numer_czlonka] = ocena;
                        miejsca[i].liczba_ocen++;

                        wynik_do_wyslania.mtype = MTYPE_B_WYNIK + odpowiedz.pid;
                        wynik_do_wyslania.numer_czlonka_komisj = numer_czlonka;
                        wynik_do_wyslania.ocena = ocena;
                        wyslij_wynik = true;
                        pid_do_logu = odpowiedz.pid;

                        break;
                    }
                }
            }
            pthread_mutex_unlock(&mutex);

            if (wyslij_wynik)
            {
                if (msq_send(msqid_B, &wynik_do_wyslania, sizeof(wynik_do_wyslania)) == -1)
                    break;
                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d |Otrzymalem odpowiedz od kandydat PID:%d\n", getpid(), pid_do_logu);
                loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            }
        }

        pid_t kandydat_do_oceny = 0;
        float srednia_do_wyslania = 0.0f;
        int numer_na_liscie_kandydata = -1;

        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            break;
        }
        if (safe_mutex_lock(&mutex) == -1)
        {
            semafor_v(SEMAFOR_MUTEX);
            break;
        }
        for (int i = 0; i < 3; i++)
        {
            if (miejsca[i].pid != 0 && miejsca[i].liczba_ocen == LICZBA_CZLONKOW_B)
            {
                float srednia = 0.0f;
                for (int j = 0; j < LICZBA_CZLONKOW_B; j++)
                {
                    srednia += miejsca[i].oceny[j];
                }

                srednia = srednia / LICZBA_CZLONKOW_B;

                kandydat_do_oceny = miejsca[i].pid;
                srednia_do_wyslania = srednia;
                numer_na_liscie_kandydata = miejsca[i].numer_na_liscie;

                memset(&miejsca[i], 0, sizeof(Sala_B));
                kandydat_gotowy[i] = false;
                pamiec_shm->liczba_osob_w_B--;
                break;
            }
        }
        pthread_mutex_unlock(&mutex);
        semafor_v(SEMAFOR_MUTEX);

        if (kandydat_do_oceny != 0)
        {
            (void)numer_na_liscie_kandydata;

            if (safe_mutex_lock(&mutex) == -1)
                break;
            if (kandydat_odpowiada == kandydat_do_oceny)
            {
                kandydat_odpowiada = 0;
            }
            pthread_mutex_unlock(&mutex);

            MSG_WYNIK_KONCOWY wynik_koncowy;
            wynik_koncowy.mtype = MTYPE_B_WYNIK_KONCOWY + kandydat_do_oceny;
            wynik_koncowy.wynik_koncowy = srednia_do_wyslania;
            wynik_koncowy.czy_zdal = (srednia_do_wyslania >= 30 && srednia_do_wyslania <= 100);

            MSG_WYNIK_KONCOWY_DZIEKAN wynik_dla_dziekana;
            wynik_dla_dziekana.mtype = NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA;
            wynik_dla_dziekana.komisja = 'B';
            wynik_dla_dziekana.pid = kandydat_do_oceny;
            wynik_dla_dziekana.wynik_koncowy = srednia_do_wyslania;

            if (msq_send(msqid_B, &wynik_koncowy, sizeof(wynik_koncowy)) == -1)
                break;
            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B Nadzorca] PID:%d | Kandydat PID:%d otrzymal wynik koncowy za czesc praktyczna=%.2f.\n", getpid(), kandydat_do_oceny, srednia_do_wyslania);
            loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

            if (msq_send(msqid_dziekan_komisja, &wynik_dla_dziekana, sizeof(wynik_dla_dziekana)) == -1)
                break;
            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d | Przeyslam do Dziekana wynik kandydata PID:%d\n", getpid(), kandydat_do_oceny);
            loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
        }
    }

    return NULL;
}

void *czlonek(void *args)
{
    int numer_czlonka = *(int *)args;
    ssize_t res;
    char msg_buffer[512];

    while (!ewakuacja_aktywna)
    {
        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            break;
        }
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int kandydatow = pamiec_shm->kandydatow_procesow;
        int liczba_osob = pamiec_shm->liczba_osob_w_B;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa && kandydatow == 0 && liczba_osob == 0)
        {
            break;
        }

        // Pytania wysyłane tylko gdy nikt nie odpowiada
        if (safe_mutex_lock(&mutex) == -1)
            break;
        bool ktos_odpowiada = (kandydat_odpowiada != 0);
        pthread_mutex_unlock(&mutex);

        if (!ktos_odpowiada)
        {
            pid_t kandydat_pid = 0;
            if (safe_mutex_lock(&mutex) == -1)
                break;

            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid != 0 && kandydat_gotowy[i] && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
                {
                    kandydat_pid = miejsca[i].pid;
                    miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (kandydat_pid != 0)
            {
                MSG_PYTANIE pytanie;
                pytanie.mtype = MTYPE_B_PYTANIE + kandydat_pid;
                pytanie.pid = kandydat_pid;
                if (msq_send(msqid_B, &pytanie, sizeof(pytanie)) == -1)
                    break;

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B CZLONEK %d] PID:%d | Zadaje pytanie dla kandydat PID:%d\n", numer_czlonka + 1, getpid(), kandydat_pid);
                loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            }
        }

        MSG_ODPOWIEDZ odpowiedz;
        res = msq_receive_no_wait(msqid_B, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);
        if (res != -1)
        {
            MSG_WYNIK wynik_do_wyslania;
            bool wyslij_wynik = false;
            pid_t pid_do_logu = 0;

            if (safe_mutex_lock(&mutex) == -1)
                break;

            if (kandydat_odpowiada == 0)
            {
                kandydat_odpowiada = odpowiedz.pid;
            }

            if (odpowiedz.pid == kandydat_odpowiada)
            {
                for (int i = 0; i < 3; i++)
                {
                    if (odpowiedz.pid == miejsca[i].pid)
                    {
                        int ocena = rand() % 101;
                        miejsca[i].oceny[numer_czlonka] = ocena;
                        miejsca[i].liczba_ocen++;

                        wynik_do_wyslania.mtype = MTYPE_B_WYNIK + odpowiedz.pid;
                        wynik_do_wyslania.numer_czlonka_komisj = numer_czlonka;
                        wynik_do_wyslania.ocena = ocena;
                        wyslij_wynik = true;
                        pid_do_logu = odpowiedz.pid;

                        break;
                    }
                }
            }
            pthread_mutex_unlock(&mutex);

            // Wysyłanie POZA sekcją krytyczną
            if (wyslij_wynik)
            {
                if (msq_send(msqid_B, &wynik_do_wyslania, sizeof(wynik_do_wyslania)) == -1)
                    break;
                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B CZLONEK %d] PID:%d | Otrzymalem odpowiedz od kandydat PID:%d\n", numer_czlonka + 1, getpid(), pid_do_logu);
                loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            }
        }
    }

    return NULL;
}

void handler_sigterm(int sigNum)
{
    (void)sigNum;
    ewakuacja_aktywna = true;
    if (pamiec_shm != NULL)
    {
        pamiec_shm->ewakuacja = true;
    }

    const char *msg = "[Komisja B] Otrzymano SIGTERM - ewakuacja.\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    // Sygnalizuj watki tylko jesli zostaly utworzone
    if (liczba_watkow > 0)
    {
        // Sygnalizuj waek glowny
        pthread_t self = pthread_self();
        if (!pthread_equal(watek_glowny, self))
        {
            pthread_kill(watek_glowny, SIGUSR1);
        }

        // Sygnalizuj watki robocze
        for (int i = 0; i < liczba_watkow; i++)
        {
            if (!pthread_equal(watki_komisji[i], self))
            {
                pthread_kill(watki_komisji[i], SIGUSR1);
            }
        }
    }
}

void handler_sigusr1(int sigNum)
{
    // Pusty handler - tylko do budzenia watkow z blokujacych wywolan systemowych
    (void)sigNum;
}