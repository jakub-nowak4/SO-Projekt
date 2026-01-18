#include "egzamin.h"

volatile sig_atomic_t egzamin_aktywny = true;
volatile sig_atomic_t ewakuacja_aktywna = false;
void *nadzorca(void *args);
void *czlonek(void *args);

void handler_sigterm(int sigNum);
void handler_sigusr1(int sigNum);

pthread_mutex_t mutex;

pthread_t watki_komisji[LICZBA_CZLONKOW_A];
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

int msqid_A = -1;
int msqid_dziekan_komisja = -1;
PamiecDzielona *pamiec_shm;
Sala_A miejsca[3] = {0};
bool kandydat_gotowy[3] = {false};
int numery_czlonkow[LICZBA_CZLONKOW_A] = {0};
pid_t kandydat_odpowiada = 0;

int main()
{
    // SIGINT
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal() | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    // SIGTERM
    struct sigaction sa;
    sa.sa_handler = handler_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction(SIGTERM) | Nie udalo sie dodac signal handler.");
        exit(EXIT_FAILURE);
    }

    // SIGUSR1
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

    semafor_v(SEMAFOR_KOMISJA_A_GOTOWA);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    if (pamiec_shm->ewakuacja)
    {
        ewakuacja_aktywna = true;
    }

    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    msqid_A = utworz_msq(klucz_msq_A);

    key_t klucz_msq_dziekan_komisja = utworz_klucz(MSQ_DZIEKAN_KOMISJA);
    msqid_dziekan_komisja = utworz_msq(klucz_msq_dziekan_komisja);

    pthread_mutex_init(&mutex, NULL);

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Czekam na rozpoczecie egzaminu\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

    while (true)
    {
        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            fprintf(stderr, "[KOMISJA A] Blad semafora przy inicjalizacji pamieci dzielonej\n");
            break;
        }
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (egzamin_trwa)
        {
            break;
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Komisja rozpoczyna prace\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

    for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
    {
        numery_czlonkow[i] = i;
        if (i == 0)
        {
            if (pthread_create(&watki_komisji[i], NULL, nadzorca, &numery_czlonkow[i]) != 0)
            {
                perror("pthread_create() | Nie udalo sie stowrzyc watku nadzorca komisja A.");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (pthread_create(&watki_komisji[i], NULL, czlonek, &numery_czlonkow[i]) != 0)
            {
                perror("pthread_create() | Nie udalo sie stwrozyc watku czlonek komisja A.");
                exit(EXIT_FAILURE);
            }
        }
        liczba_watkow++;
    }

    while (egzamin_aktywny && !ewakuacja_aktywna)
    {
        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            fprintf(stderr, "[KOMISJA A] Blad semafora przy inicjalizacji pamieci dzielonej\n");
            break;
        }
        int procesow = pamiec_shm->kandydatow_procesow;
        int osoby_w_sali = pamiec_shm->liczba_osob_w_A;
        semafor_v(SEMAFOR_MUTEX);

        if (procesow == 0 && osoby_w_sali == 0)
        {
            egzamin_aktywny = false;
            break;
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Egzamin dobiegl konca (lub ewakuacja). Czekam na watki.\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

    for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
    {
        int ret = pthread_join(watki_komisji[i], NULL);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_join() | Nie udalo sie przylaczyc watku: %s\n", strerror(ret));
            exit(EXIT_FAILURE);
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "Komisja A konczy prace\n");
    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

    pthread_mutex_destroy(&mutex);
    odlacz_shm(pamiec_shm);

    semafor_v_bez_undo(SEMAFOR_KOMISJA_A_KONIEC);

    return 0;
}

void *nadzorca(void *args)
{
    int numer_czlonka = *(int *)args;
    char msg_buffer[512];

    while (!ewakuacja_aktywna)
    {
        ssize_t rozmiar_odpowiedz;

        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            fprintf(stderr, "[KOMISJA A] Blad semafora przy inicjalizacji pamieci dzielonej\n");
            break;
        }
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int liczba_osob = pamiec_shm->liczba_osob_w_A;
        int kandydatow = pamiec_shm->kandydatow_procesow;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa && kandydatow == 0 && liczba_osob == 0)
        {
            break;
        }

        if (liczba_osob < 3)
        {
            MSG_KANDYDAT_WCHODZI_DO_A prosba;
            rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &prosba, sizeof(prosba), KANDYDAT_WCHODZI_DO_A);

            if (rozmiar_odpowiedz != -1)
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
                        miejsca[i].pid = prosba.pid;
                        miejsca[i].numer_na_liscie = prosba.numer_na_liscie;
                        miejsca[i].liczba_ocen = 0;

                        for (int j = 0; j < LICZBA_CZLONKOW_A; j++)
                        {
                            miejsca[i].czy_dostal_pytanie[j] = false;
                            miejsca[i].oceny[j] = 0;
                        }

                        kandydat_gotowy[i] = false;
                        pamiec_shm->liczba_osob_w_A++;
                        slot = i;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);
                semafor_v(SEMAFOR_MUTEX);

                // Komunikat musi byc wysylany poza sekcja krytyczna jakb kolejka byla zapelniona
                if (slot != -1)
                {
                    MSG_KANDYDAT_WCHODZI_DO_A_POTWIERDZENIE ok;
                    ok.mtype = MTYPE_A_POTWIERDZENIE + prosba.pid;
                    if (msq_send(msqid_A, &ok, sizeof(ok)) == -1)
                        break;

                    snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Wpuscilem kandydata PID:%d Nr:%d do sali.\n", getpid(), prosba.pid, prosba.numer_na_liscie);
                    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
                }
            }
        }

        // Odbieramy potwierdzenie gotowości od kandydata
        MSG_KANDYDAT_GOTOWY gotowy;
        rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &gotowy, sizeof(gotowy), KANDYDAT_GOTOWY_A);
        if (rozmiar_odpowiedz != -1)
        {
            int nr_liscie_gotowy = -1;
            if (safe_mutex_lock(&mutex) == -1)
                break;
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid == gotowy.pid)
                {
                    kandydat_gotowy[i] = true;
                    nr_liscie_gotowy = miejsca[i].numer_na_liscie; 
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (nr_liscie_gotowy != -1)
            {
                snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Kandydat PID:%d Nr:%d gotowy do odpowiadania.\n", getpid(), gotowy.pid, nr_liscie_gotowy);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
            }
        }

        MSG_KANDYDAT_POWTARZA weryfikacja;
        rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &weryfikacja, sizeof(weryfikacja), NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO);

        if (rozmiar_odpowiedz != -1)
        {
            int nr_liscie_weryf = -1;    
            if (safe_mutex_lock(&mutex) == -1)
                break;
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid == weryfikacja.pid)
                {
                    nr_liscie_weryf = miejsca[i].numer_na_liscie; 
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Zglasza sie do mnie kandydat PID:%d Nr:%d, ktory ma zdana czesc teoretyczna egzaminu.\n", getpid(), weryfikacja.pid, nr_liscie_weryf);
            loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

            MSG_KANDYDAT_POWTARZA_ODPOWIEDZ_NADZORCY weryfikacja_odpowiedz;
            weryfikacja_odpowiedz.mtype = MTYPE_A_WERYFIKACJA_ODP + weryfikacja.pid;
            weryfikacja_odpowiedz.zgoda = (weryfikacja.wynik_a >= 30 && weryfikacja.wynik_a <= 100);

            if (weryfikacja_odpowiedz.zgoda)
            {
                snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Weryfikacja pozytywna dla kandydata PID:%d Nr:%d - przepuszczam do Komisji B.\n", getpid(), weryfikacja.pid, nr_liscie_weryf);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

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
                    if (miejsca[i].pid == weryfikacja.pid)
                    {
                        memset(&miejsca[i], 0, sizeof(Sala_A));
                        kandydat_gotowy[i] = false;
                        pamiec_shm->liczba_osob_w_A--;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);
                semafor_v(SEMAFOR_MUTEX);

                MSG_WYNIK_KONCOWY_DZIEKAN wynik_dla_dziekana;
                wynik_dla_dziekana.mtype = NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA;
                wynik_dla_dziekana.komisja = 'A';
                wynik_dla_dziekana.pid = weryfikacja.pid;
                wynik_dla_dziekana.wynik_koncowy = weryfikacja.wynik_a;
                if (msq_send(msqid_dziekan_komisja, &wynik_dla_dziekana, sizeof(wynik_dla_dziekana)) == -1)
                    break;
            }
            else
            {
                snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Weryfikacja negatywna dla kandydata PID:%d Nr:%d - wynik %.2f jest nieprawidlowy.\n", getpid(), weryfikacja.pid, nr_liscie_weryf, weryfikacja.wynik_a);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
            }

            if (msq_send(msqid_A, &weryfikacja_odpowiedz, sizeof(weryfikacja_odpowiedz)) == -1)
                break;
        }

        // Pytania wysylane tylko gdy nikt nie odpowiada
        if (safe_mutex_lock(&mutex) == -1)
            break;
        bool ktos_odpowiada = (kandydat_odpowiada != 0);
        pthread_mutex_unlock(&mutex);

        if (!ktos_odpowiada)
        {
            pid_t kandydat_pid = 0;
            int nr_liscie_pytanie = -1;
            if (safe_mutex_lock(&mutex) == -1)
                break;

            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid != 0 && kandydat_gotowy[i] && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
                {
                    kandydat_pid = miejsca[i].pid;
                    nr_liscie_pytanie = miejsca[i].numer_na_liscie;     
                    miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (kandydat_pid != 0)
            {
                MSG_PYTANIE pytanie;
                pytanie.mtype = MTYPE_A_PYTANIE + kandydat_pid;
                pytanie.pid = kandydat_pid;
                if (msq_send(msqid_A, &pytanie, sizeof(pytanie)) == -1)
                    break;

                snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Zadaje pytanie dla kandydata PID:%d Nr:%d\n", getpid(), kandydat_pid, nr_liscie_pytanie);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
            }
        }

        MSG_ODPOWIEDZ odpowiedz;
        rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);

        if (rozmiar_odpowiedz != -1)
        {
            MSG_WYNIK wynik_do_wyslania;
            bool wyslij_wynik = false;
            pid_t pid_do_logu = 0;
            int nr_do_logu = -1; 

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
                    if (miejsca[i].pid == odpowiedz.pid)
                    {
                        int ocena = rand() % 101;
                        miejsca[i].oceny[numer_czlonka] = ocena;
                        miejsca[i].liczba_ocen++;

                        // Przygotuj dane do wysłania POZA mutexem
                        wynik_do_wyslania.mtype = MTYPE_A_WYNIK + odpowiedz.pid;
                        wynik_do_wyslania.numer_czlonka_komisj = numer_czlonka;
                        wynik_do_wyslania.ocena = ocena;
                        wyslij_wynik = true;
                        pid_do_logu = odpowiedz.pid;
                        nr_do_logu = miejsca[i].numer_na_liscie; 

                        break;
                    }
                }
            }
            pthread_mutex_unlock(&mutex);

            if (wyslij_wynik)
            {
                if (msq_send(msqid_A, &wynik_do_wyslania, sizeof(wynik_do_wyslania)) == -1)
                    break;
                snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Otrzymalem odpowiedz od kandydata PID:%d Nr:%d, Ocena: %d\n", getpid(), pid_do_logu, nr_do_logu, wynik_do_wyslania.ocena);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
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
            if (miejsca[i].pid != 0 && miejsca[i].liczba_ocen == LICZBA_CZLONKOW_A)
            {
                float srednia = 0.0f;
                for (int j = 0; j < LICZBA_CZLONKOW_A; j++)
                {
                    srednia += miejsca[i].oceny[j];
                }

                srednia = srednia / LICZBA_CZLONKOW_A;

                kandydat_do_oceny = miejsca[i].pid;
                srednia_do_wyslania = srednia;
                numer_na_liscie_kandydata = miejsca[i].numer_na_liscie; 

                memset(&miejsca[i], 0, sizeof(Sala_A));
                kandydat_gotowy[i] = false;
                pamiec_shm->liczba_osob_w_A--;
                break;
            }
        }
        pthread_mutex_unlock(&mutex);
        semafor_v(SEMAFOR_MUTEX);

        if (kandydat_do_oceny != 0)
        {
            if (safe_mutex_lock(&mutex) == -1)
                break;
            if (kandydat_odpowiada == kandydat_do_oceny)
            {
                kandydat_odpowiada = 0;
            }
            pthread_mutex_unlock(&mutex);

            MSG_WYNIK_KONCOWY wynik_koncowy;
            wynik_koncowy.mtype = MTYPE_A_WYNIK_KONCOWY + kandydat_do_oceny;
            wynik_koncowy.wynik_koncowy = srednia_do_wyslania;
            wynik_koncowy.czy_zdal = (srednia_do_wyslania >= 30 && srednia_do_wyslania <= 100);

            snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Kandydat PID:%d Nr:%d otrzymal wynik koncowy za czesc teoretyczna: %.2f\n", getpid(), kandydat_do_oceny, numer_na_liscie_kandydata, srednia_do_wyslania);
            loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

            MSG_WYNIK_KONCOWY_DZIEKAN wynik_dla_dziekana;
            wynik_dla_dziekana.mtype = NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA;
            wynik_dla_dziekana.komisja = 'A';
            wynik_dla_dziekana.pid = kandydat_do_oceny;
            wynik_dla_dziekana.wynik_koncowy = srednia_do_wyslania;

            if (msq_send(msqid_A, &wynik_koncowy, sizeof(wynik_koncowy)) == -1)
                break;
            if (msq_send(msqid_dziekan_komisja, &wynik_dla_dziekana, sizeof(wynik_dla_dziekana)) == -1)
                break;

            snprintf(msg_buffer, sizeof(msg_buffer), "[Nadzorca A] PID:%d | Przesylam do Dziekana wynik kandydata PID:%d Nr:%d\n", getpid(), kandydat_do_oceny, numer_na_liscie_kandydata);
            loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
        }
    }

    return NULL;
}

void *czlonek(void *args)
{
    char msg_buffer[512];
    ssize_t res;
    int numer_czlonka = *(int *)args;

    while (!ewakuacja_aktywna)
    {
        if (semafor_p(SEMAFOR_MUTEX) == -1)
        {
            break;
        }
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int kandydatow = pamiec_shm->kandydatow_procesow;
        int liczba_osob = pamiec_shm->liczba_osob_w_A;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa && kandydatow == 0 && liczba_osob == 0)
        {
            break;
        }

        // Pytania wysylane tylko gdy nikt nie odpowiada
        if (safe_mutex_lock(&mutex) == -1)
            break;
        bool ktos_odpowiada = (kandydat_odpowiada != 0);
        pthread_mutex_unlock(&mutex);

        if (!ktos_odpowiada)
        {
            pid_t kandydat_pid = 0;
            int nr_liscie_pytanie = -1;
            if (safe_mutex_lock(&mutex) == -1)
                break;
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid != 0 && kandydat_gotowy[i] && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
                {
                    kandydat_pid = miejsca[i].pid;
                    nr_liscie_pytanie = miejsca[i].numer_na_liscie;     
                    miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (kandydat_pid != 0)
            {
                MSG_PYTANIE pytanie;
                pytanie.mtype = MTYPE_A_PYTANIE + kandydat_pid;
                pytanie.pid = kandydat_pid;
                if (msq_send(msqid_A, &pytanie, sizeof(pytanie)) == -1)
                    break;

                snprintf(msg_buffer, sizeof(msg_buffer), "[Czlonek A %d] PID:%d | Zadaje pytanie dla kandydata PID:%d Nr:%d\n", numer_czlonka + 1, getpid(), kandydat_pid, nr_liscie_pytanie);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
            }
        }

        MSG_ODPOWIEDZ odpowiedz;
        res = msq_receive_no_wait(msqid_A, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);

        if (res != -1)
        {
            MSG_WYNIK wynik_do_wyslania;
            bool wyslij_wynik = false;
            pid_t pid_do_logu = 0;
            int nr_do_logu = -1; 

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
                    if (miejsca[i].pid == odpowiedz.pid)
                    {
                        int ocena = rand() % 101;
                        miejsca[i].oceny[numer_czlonka] = ocena;
                        miejsca[i].liczba_ocen++;

                        // Przygotuj dane do wysłania POZA mutexem
                        wynik_do_wyslania.mtype = MTYPE_A_WYNIK + odpowiedz.pid;
                        wynik_do_wyslania.numer_czlonka_komisj = numer_czlonka;
                        wynik_do_wyslania.ocena = ocena;
                        wyslij_wynik = true;
                        pid_do_logu = odpowiedz.pid;
                        nr_do_logu = miejsca[i].numer_na_liscie; 

                        break;
                    }
                }
            }
            pthread_mutex_unlock(&mutex);

            if (wyslij_wynik)
            {
                if (msq_send(msqid_A, &wynik_do_wyslania, sizeof(wynik_do_wyslania)) == -1)
                    break;
                snprintf(msg_buffer, sizeof(msg_buffer), "[Czlonek A %d] PID:%d | Otrzymalem odpowiedz od kandydata PID:%d Nr:%d, Ocena: %d\n", numer_czlonka + 1, getpid(), pid_do_logu, nr_do_logu, wynik_do_wyslania.ocena);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
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

    const char *msg = "[Komisja A] Otrzymano SIGTERM - ewakuacja.\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    // Sygnalizuj watki tylko jesli zostaly utworzone
    if (liczba_watkow > 0)
    {
        // Sygnalizuj watek głowny
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
    // Potrzebne aby obudzic watek z blokujacych operacji systemowych
    (void)sigNum;
}