#include "egzamin.h"

int msqid_B = -1;
int msqid_dziekan_komisja = -1;
int numery_czlonkow[LICZBA_CZLONKOW_B] = {0};
PamiecDzielona *pamiec_shm;
Sala_B miejsca[3] = {0};

pthread_mutex_t mutex;

void *nadzorca(void *args);
void *czlonek(void *args);

int main()
{
    signal(SIGINT, SIG_IGN);
    ustaw_handler_ewakuacji();

    char msg_buffer[512];

    srand(time(NULL) ^ getpid());

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    // Sprawdz ewakuacje zaraz po starcie
    if (sprawdz_ewakuacje(pamiec_shm))
    {
        odlacz_shm(pamiec_shm);
        return 0;
    }

    pthread_mutex_init(&mutex, NULL);

    key_t klucz_msq_B = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_B);
    msqid_B = utworz_msq(klucz_msq_B);

    key_t klucz_msq_dziekan_komisja = utworz_klucz(MSQ_DZIEKAN_KOMISJA);
    msqid_dziekan_komisja = utworz_msq(klucz_msq_dziekan_komisja);

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Czekam na rozpoczecie egzaminu\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

    semafor_v(SEMAFOR_KOMISJA_B_GOTOWA);

    // Czekaj na start egzaminu lub ewakuacje
    while (true)
    {
        if (sprawdz_ewakuacje(pamiec_shm))
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | EWAKUACJA przed startem!\n", getpid());
            loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            pthread_mutex_destroy(&mutex);
            odlacz_shm(pamiec_shm);
            return 0;
        }

        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (egzamin_trwa)
        {
            break;
        }
        usleep(1000);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Komisja rozpoczyna prace.\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

    pthread_t czlonkowie_komisji[LICZBA_CZLONKOW_B];
    for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
    {
        numery_czlonkow[i] = i;
        if (i == 0)
        {
            if (pthread_create(&czlonkowie_komisji[i], NULL, nadzorca, &numery_czlonkow[i]) != 0)
            {
                perror("pthread_create() | Nie udalo sie utworzyc watku nadzorca Komisji B.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (pthread_create(&czlonkowie_komisji[i], NULL, czlonek, &numery_czlonkow[i]) != 0)
            {
                perror("pthread_create() | Nie udalo sie utworzyc watku czlonek Komisji B.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Czekaj na koniec egzaminu lub ewakuacje
    while (true)
    {
        if (sprawdz_ewakuacje(pamiec_shm))
        {
            break;
        }

        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa)
        {
            break;
        }
        usleep(10000);
    }

    if (sprawdz_ewakuacje(pamiec_shm))
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | EWAKUACJA!\n", getpid());
        loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
    }
    else
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Komisja konczy prace.\n", getpid());
        loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
    }

    for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
    {
        int ret = pthread_join(czlonkowie_komisji[i], NULL);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_join() | Nie udalo sie przylaczyc watku: %s\n", strerror(ret));
            exit(EXIT_FAILURE);
        }
    }

    pthread_mutex_destroy(&mutex);
    odlacz_shm(pamiec_shm);
    return 0;
}

void *nadzorca(void *args)
{
    int numer_czlonka = *(int *)args;
    ssize_t res;
    char msg_buffer[512];

    while (true)
    {
        if (sprawdz_ewakuacje(pamiec_shm))
        {
            break;
        }

        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int liczba_osob = pamiec_shm->liczba_osob_w_B;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa)
        {
            break;
        }

        // Wpuszczanie kandydatow do sali
        if (liczba_osob < 3)
        {
            MSG_KANDYDAT_WCHODZI_DO_B zgloszenie_B;
            res = msq_receive_no_wait(msqid_B, &zgloszenie_B, sizeof(zgloszenie_B), KANDYDAT_WCHODZI_DO_B);

            if (res != -1)
            {
                bool znaleziono = false;

                pthread_mutex_lock(&mutex);
                for (int i = 0; i < 3; i++)
                {
                    if (miejsca[i].pid == 0)
                    {
                        miejsca[i].pid = zgloszenie_B.pid;
                        miejsca[i].numer_na_liscie = zgloszenie_B.numer_na_liscie;
                        miejsca[i].liczba_ocen = 0;
                        miejsca[i].odpowiada = false;

                        for (int j = 0; j < LICZBA_CZLONKOW_B; j++)
                        {
                            miejsca[i].czy_dostal_pytanie[j] = false;
                            miejsca[i].oceny[j] = 0;
                        }

                        znaleziono = true;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);

                if (znaleziono)
                {
                    semafor_p(SEMAFOR_MUTEX);
                    pamiec_shm->liczba_osob_w_B++;
                    semafor_v(SEMAFOR_MUTEX);

                    MSG_KANDYDAT_WCHODZI_DO_B_POTWIERDZENIE potwierdzenie;
                    potwierdzenie.mtype = zgloszenie_B.pid + MTYPE_OFFSET_POTWIERDZENIE_B;
                    msq_send(msqid_B, &potwierdzenie, sizeof(potwierdzenie));

                    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d | Do sali wchodzi kandydat PID:%d\n", getpid(), zgloszenie_B.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
                }
            }
        }

        // Sprawdz czy ktos odpowiada
        pthread_mutex_lock(&mutex);
        bool ktos_teraz_odpowiada = false;
        for (int i = 0; i < 3; i++)
        {
            if (miejsca[i].odpowiada)
            {
                ktos_teraz_odpowiada = true;
                break;
            }
        }
        pthread_mutex_unlock(&mutex);

        // Zadawanie pytan
        if (semafor_wartosc(SEMAFOR_ODPOWIEDZ_B) > 0 && !ktos_teraz_odpowiada)
        {
            pid_t kandydat_pid = 0;
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid != 0 && !miejsca[i].odpowiada && !miejsca[i].czy_dostal_pytanie[numer_czlonka])
                {
                    kandydat_pid = miejsca[i].pid;
                    miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (kandydat_pid != 0)
            {
                usleep(CZAS_PYTANIE);

                MSG_PYTANIE pytanie;
                pytanie.mtype = kandydat_pid + MTYPE_OFFSET_PYTANIE;
                pytanie.pid = kandydat_pid;
                msq_send(msqid_B, &pytanie, sizeof(pytanie));

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d | Zadaje pytanie dla kandydata PID:%d\n", getpid(), kandydat_pid);
                loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            }
        }

        // Odbieranie odpowiedzi
        MSG_ODPOWIEDZ odpowiedz;
        res = msq_receive_no_wait(msqid_B, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);
        if (res != -1)
        {
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (odpowiedz.pid == miejsca[i].pid)
                {
                    miejsca[i].odpowiada = true;

                    int ocena = rand() % 101;
                    miejsca[i].oceny[numer_czlonka] = ocena;
                    miejsca[i].liczba_ocen++;

                    MSG_WYNIK wynik;
                    wynik.mtype = odpowiedz.pid + MTYPE_OFFSET_WYNIK;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;

                    msq_send(msqid_B, &wynik, sizeof(wynik));

                    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d | Otrzymalem odpowiedz od kandydata PID:%d\n", getpid(), odpowiedz.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        // Obliczanie wyniku koncowego
        pid_t kandydat_do_oceny = 0;
        float srednia_do_wyslania = 0.0f;

        pthread_mutex_lock(&mutex);
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

                memset(&miejsca[i], 0, sizeof(Sala_B));
                break;
            }
        }
        pthread_mutex_unlock(&mutex);

        if (kandydat_do_oceny != 0)
        {
            MSG_WYNIK_KONCOWY wynik_koncowy;
            wynik_koncowy.mtype = kandydat_do_oceny + MTYPE_OFFSET_WYNIK_KONCOWY;
            wynik_koncowy.wynik_koncowy = srednia_do_wyslania;
            wynik_koncowy.czy_zdal = (srednia_do_wyslania >= 30 && srednia_do_wyslania <= 100);

            MSG_WYNIK_KONCOWY_DZIEKAN wynik_dla_dziekana;
            wynik_dla_dziekana.mtype = NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA;
            wynik_dla_dziekana.komisja = 'B';
            wynik_dla_dziekana.pid = kandydat_do_oceny;
            wynik_dla_dziekana.wynik_koncowy = srednia_do_wyslania;

            msq_send(msqid_B, &wynik_koncowy, sizeof(wynik_koncowy));
            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B Nadzorca] PID:%d | Kandydat PID:%d otrzymal wynik=%.2f.\n", getpid(), kandydat_do_oceny, srednia_do_wyslania);
            loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

            msq_send(msqid_dziekan_komisja, &wynik_dla_dziekana, sizeof(wynik_dla_dziekana));
            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d | Przesylam do Dziekana wynik kandydata PID:%d\n", getpid(), kandydat_do_oceny);
            loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

            semafor_p(SEMAFOR_MUTEX);
            pamiec_shm->liczba_osob_w_B--;
            semafor_v(SEMAFOR_MUTEX);
        }

        usleep(10000);
    }

    return NULL;
}

void *czlonek(void *args)
{
    int numer_czlonka = *(int *)args;
    ssize_t res;
    char msg_buffer[512];

    while (true)
    {
        if (sprawdz_ewakuacje(pamiec_shm))
        {
            break;
        }

        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa)
        {
            break;
        }

        // Sprawdz czy ktos odpowiada
        pthread_mutex_lock(&mutex);
        bool ktos_teraz_odpowiada = false;
        for (int i = 0; i < 3; i++)
        {
            if (miejsca[i].odpowiada)
            {
                ktos_teraz_odpowiada = true;
                break;
            }
        }
        pthread_mutex_unlock(&mutex);

        // Zadawanie pytan
        if (semafor_wartosc(SEMAFOR_ODPOWIEDZ_B) > 0 && !ktos_teraz_odpowiada)
        {
            pid_t kandydat_pid = 0;
            pthread_mutex_lock(&mutex);

            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid != 0 && !miejsca[i].odpowiada && !miejsca[i].czy_dostal_pytanie[numer_czlonka])
                {
                    kandydat_pid = miejsca[i].pid;
                    miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex);

            if (kandydat_pid != 0)
            {
                usleep(rand() % CZAS_PYTANIE);

                MSG_PYTANIE pytanie;
                pytanie.mtype = kandydat_pid + MTYPE_OFFSET_PYTANIE;
                pytanie.pid = kandydat_pid;
                msq_send(msqid_B, &pytanie, sizeof(pytanie));

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B CZLONEK %d] PID:%d | Zadaje pytanie dla kandydata PID:%d\n", numer_czlonka + 1, getpid(), kandydat_pid);
                loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            }
        }

        // Odbieranie odpowiedzi
        MSG_ODPOWIEDZ odpowiedz;
        res = msq_receive_no_wait(msqid_B, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);
        if (res != -1)
        {
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (odpowiedz.pid == miejsca[i].pid)
                {
                    int ocena = rand() % 101;
                    miejsca[i].oceny[numer_czlonka] = ocena;
                    miejsca[i].liczba_ocen++;

                    MSG_WYNIK wynik;
                    wynik.mtype = odpowiedz.pid + MTYPE_OFFSET_WYNIK;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;

                    msq_send(msqid_B, &wynik, sizeof(wynik));
                    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B CZLONEK %d] PID:%d | Otrzymalem odpowiedz od kandydata PID:%d\n", numer_czlonka + 1, getpid(), odpowiedz.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        usleep(10000);
    }

    return NULL;
}