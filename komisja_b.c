#include "egzamin.h"

volatile sig_atomic_t egzamin_aktywny = true;

int msqid_B = -1;
int msqid_dziekan_komisja = -1;
int numery_czlonkow[LICZBA_CZLONKOW_B] = {0};
PamiecDzielona *pamiec_shm;
Sala_B miejsca[3] = {0};
bool kandydat_gotowy[3] = {false, false, false};

pthread_mutex_t mutex;

void *nadzorca(void *args);
void *czlonek(void *args);

int main()
{
    char msg_buffer[200];

    srand(time(NULL) ^ getpid());

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    semafor_v(SEMAFOR_KOMISJA_B_GOTOWA);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    pthread_mutex_init(&mutex, NULL);

    key_t klucz_msq_B = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_B);
    msqid_B = utworz_msq(klucz_msq_B);

    key_t klucz_msq_dziekan_komisja = utworz_klucz(MSQ_DZIEKAN_KOMISJA);
    msqid_dziekan_komisja = utworz_msq(klucz_msq_dziekan_komisja);

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Czekam na rozpoczecie egzaminu\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

    while (true)
    {
        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (egzamin_trwa)
        {
            break;
        }
        // usleep(10000);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID%d | Komisja rozpoczyna prace.\n", getpid());
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

    while (egzamin_aktywny)
    {
        semafor_p(SEMAFOR_MUTEX);
        int procesow = pamiec_shm->kandydatow_procesow;
        semafor_v(SEMAFOR_MUTEX);

        if (procesow == 0)
        {
            egzamin_aktywny = false;
            break;
        }

        // usleep(10000);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Komisja konczy prace.\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

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
    char msg_buffer[200];

    while (true)
    {
        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int liczba_osob = pamiec_shm->liczba_osob_w_B;
        int kandydatow = pamiec_shm->kandydatow_procesow;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa && kandydatow == 0)
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

                semafor_p(SEMAFOR_MUTEX);
                pthread_mutex_lock(&mutex);
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
                    msq_send(msqid_B, &potwierdzenie, sizeof(potwierdzenie));

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
            pthread_mutex_lock(&mutex);
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

        if (semafor_wartosc(SEMAFOR_ODPOWIEDZ_B) > 0)
        {
            pid_t kandydat_pid = 0;
            pthread_mutex_lock(&mutex);
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
                // usleep((rand() % 2 + 1) * 1000000);

                MSG_PYTANIE pytanie;
                pytanie.mtype = MTYPE_B_PYTANIE + kandydat_pid;
                pytanie.pid = kandydat_pid;
                msq_send(msqid_B, &pytanie, sizeof(pytanie));

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d |Zadaje pytanie dla kandydata PID:%d\n", getpid(), kandydat_pid);
                loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            }
        }

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
                    wynik.mtype = MTYPE_B_WYNIK + odpowiedz.pid;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;

                    msq_send(msqid_B, &wynik, sizeof(wynik));

                    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d |Otrzymalem odpowiedz od kandydat PID:%d\n", getpid(), odpowiedz.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        pid_t kandydat_do_oceny = 0;
        float srednia_do_wyslania = 0.0f;
        int numer_na_liscie_kandydata = -1;

        semafor_p(SEMAFOR_MUTEX);
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

            MSG_WYNIK_KONCOWY wynik_koncowy;
            wynik_koncowy.mtype = MTYPE_B_WYNIK_KONCOWY + kandydat_do_oceny;
            wynik_koncowy.wynik_koncowy = srednia_do_wyslania;
            wynik_koncowy.czy_zdal = (srednia_do_wyslania >= 30 && srednia_do_wyslania <= 100);

            MSG_WYNIK_KONCOWY_DZIEKAN wynik_dla_dziekana;
            wynik_dla_dziekana.mtype = NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA;
            wynik_dla_dziekana.komisja = 'B';
            wynik_dla_dziekana.pid = kandydat_do_oceny;
            wynik_dla_dziekana.wynik_koncowy = srednia_do_wyslania;

            msq_send(msqid_B, &wynik_koncowy, sizeof(wynik_koncowy));
            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B Nadzorca] PID:%d | Kandydat PID:%d otrzymal wynik koncowy za czesc praktyczna=%.2f.\n", getpid(), kandydat_do_oceny, srednia_do_wyslania);
            loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

            msq_send(msqid_dziekan_komisja, &wynik_dla_dziekana, sizeof(wynik_dla_dziekana));
            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B NADZORCA] PID:%d | Przeyslam do Dziekana wynik kandydata PID:%d\n", getpid(), kandydat_do_oceny);
            loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
        }

        // usleep(10000);
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
        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int kandydatow = pamiec_shm->kandydatow_procesow;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa && kandydatow == 0)
        {
            break;
        }

        if (semafor_wartosc(SEMAFOR_ODPOWIEDZ_B) > 0)
        {
            pid_t kandydat_pid = 0;
            pthread_mutex_lock(&mutex);

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
                // usleep((rand() % 2 + 1) * 1000000);

                MSG_PYTANIE pytanie;
                pytanie.mtype = MTYPE_B_PYTANIE + kandydat_pid;
                pytanie.pid = kandydat_pid;
                msq_send(msqid_B, &pytanie, sizeof(pytanie));

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B CZLONEK %d] PID:%d | Zadaje pytanie dla kandydat PID:%d\n", numer_czlonka + 1, getpid(), kandydat_pid);
                loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);
            }
        }

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
                    wynik.mtype = MTYPE_B_WYNIK + odpowiedz.pid;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;

                    msq_send(msqid_B, &wynik, sizeof(wynik));
                    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B CZLONEK %d] PID:%d | Otrzymalem odpowiedz od kandydat PID:%d\n", numer_czlonka + 1, getpid(), odpowiedz.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_B, LOGI_KOMISJA_B, msg_buffer);

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        // usleep(10000);
    }

    return NULL;
}