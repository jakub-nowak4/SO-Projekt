#include "egzamin.h"

int msqid_B = -1;
int numery_czlonkow[LICZBA_CZLONKOW_B] = {0};
PamiecDzielona *pamiec_shm;
Sala_B miejsca[3] = {0};

pthread_mutex_t mutex;

void *nadzorca(void *args);
void *czlonek(void *args);

int main()
{
    char msg_buffer[200];

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    pthread_mutex_init(&mutex, NULL);

    key_t klucz_msq_B = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_B);
    msqid_B = utworz_msq(klucz_msq_B);

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Czekam na rozpoczecie egzaminu\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // Oczekiwanie na start
    while (true)
    {
        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (egzamin_trwa)
        {
            break;
        }
        usleep(1000);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID%d | Komisja rozpoczyna prace.\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    pthread_t czlonkowie_komisji[LICZBA_CZLONKOW_B];
    for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
    {
        numery_czlonkow[i] = i;
        if (i == 0)
        {
            if (pthread_create(&czlonkowie_komisji[i], NULL, nadzorca, &numery_czlonkow[i]) == -1)
            {
                perror("pthread_create() | Nie udalo sie utworzyc watku nadzorca Komisji B.\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (pthread_create(&czlonkowie_komisji[i], NULL, czlonek, &numery_czlonkow[i]) == -1)
            {
                perror("pthread_create() | Nie udalo sie utworzyc watku czlonek Komisji B.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    while (true)
    {
        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa)
        {
            break;
        }
        usleep(1000);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B] PID:%d | Komisja konczy prace.\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    for (int i = 0; i < LICZBA_CZLONKOW_B; i++)
    {
        if (pthread_join(czlonkowie_komisji[i], NULL) == -1)
        {
            perror("pthread_join() | Nie udalo sie przylaczyc watku czlonek Komisji B.\n");
            exit(EXIT_FAILURE);
        }
    }

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
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa)
            break;

        // Sprawdzamy czy ktos nowy nie czeka w kolejce

        semafor_p(SEMAFOR_MUTEX);
        int liczba_osob = pamiec_shm->liczba_osob_w_B;
        semafor_v(SEMAFOR_MUTEX);

        // Ktos nowy wszedl - szukamy dla niego wolnego miejsce jesli sala jest pelna nie odbieramy wiadomosci

        if (liczba_osob < 3)
        {
            MSG_KANDYDAT_WCHODZI_DO_B zgloszenie_B;
            res = msq_receive_no_wait(msqid_B, &zgloszenie_B, sizeof(zgloszenie_B), KANDYDAT_WCHODZI_DO_B);

            if (res != -1)
            {
                pthread_mutex_lock(&mutex);
                for (int i = 0; i < 3; i++)
                {
                    if (miejsca[i].pid == 0)
                    {
                        miejsca[i].pid = zgloszenie_B.pid;
                        miejsca[i].numer_na_liscie = zgloszenie_B.numer_na_liscie;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);

                MSG_KANDYDAT_WCHODZI_DO_B_POTWIERDZENIE potwierdzenie;
                potwierdzenie.mtype = zgloszenie_B.pid;
                msq_send(msqid_B, &potwierdzenie, sizeof(potwierdzenie));
            }
        }

        // Nadzorca spawdza czy wyslal wszytskiem swoje pytanie
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < 3; i++)
        {
            if (miejsca[i].pid != 0 && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
            {
                pthread_mutex_unlock(&mutex);

                usleep(rand() % 2000 + 5000);

                pthread_mutex_lock(&mutex);
                MSG_PYTANIE pytanie;
                pytanie.mtype = miejsca[i].pid;
                pytanie.pid = miejsca[i].pid;

                msq_send(msqid_B, &pytanie, sizeof(pytanie));

                miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
            }
        }
        pthread_mutex_unlock(&mutex);

        // Nadzorca sprawdza czy nie otrzymal odpowiedzi na pytanie
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
                    wynik.mtype = odpowiedz.pid;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;

                    msq_send(msqid_B, &wynik, sizeof(wynik));

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        // Nadzorca sprawdza czy kandydat odpowiedzial na 3 pytan
        pthread_mutex_lock(&mutex);

        for (int i = 0; i < 3; i++)
        {

            if (miejsca[i].liczba_ocen == LICZBA_CZLONKOW_B)
            {
                float srednia = 0.0f;
                for (int j = 0; j < LICZBA_CZLONKOW_B; j++)
                {
                    srednia += miejsca[i].oceny[j];
                }

                srednia = srednia / LICZBA_CZLONKOW_B;

                MSG_WYNIK_KONCOWY wynik_koncowy;
                wynik_koncowy.mtype = miejsca[i].pid;
                wynik_koncowy.wynik_koncowy = srednia;
                wynik_koncowy.czy_zdal = (srednia >= 30 && srednia <= 100);
                msq_send(msqid_B, &wynik_koncowy, sizeof(wynik_koncowy));

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA B Nadzorca] PID:%d | Kandydat PID:%d otrzymal wynik koncowy za czesc praktyczna=%.2f.\n", getpid(), miejsca[i].pid, srednia);
                wypisz_wiadomosc(msg_buffer);

                memset(&miejsca[i], 0, sizeof(Sala_B));

                semafor_p(SEMAFOR_MUTEX);
                pamiec_shm->liczba_osob_w_B--;
                semafor_v(SEMAFOR_MUTEX);

                break;
            }
        }
        pthread_mutex_unlock(&mutex);

        usleep(1000);
    }

    return 0;
}

void *czlonek(void *args)
{
    int numer_czlonka = *(int *)args;
    ssize_t res;

    while (true)
    {
        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa)
            break;

        // Czlonek  spawdza czy wyslal wszytskiem swoje pytanie
        for (int i = 0; i < 3; i++)
        {
            pthread_mutex_lock(&mutex);
            if (miejsca[i].pid != 0 && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
            {
                pthread_mutex_unlock(&mutex);

                usleep(rand() % 2000 + 5000);

                pthread_mutex_lock(&mutex);
                MSG_PYTANIE pytanie;
                pytanie.mtype = miejsca[i].pid;
                pytanie.pid = miejsca[i].pid;
                miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;

                msq_send(msqid_B, &pytanie, sizeof(pytanie));
            }
            pthread_mutex_unlock(&mutex);
        }

        // Czlonek sprawdza czy nie otrzymal odpowiedzi na pytanie
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
                    wynik.mtype = odpowiedz.pid;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;

                    msq_send(msqid_B, &wynik, sizeof(wynik));

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        usleep(1000);
    }

    return 0;
}