#include "egzamin.h"

void *nadzorca(void *args);
void *czlonek(void *args);

pthread_mutex_t mutex;

int msqid_A = -1;
PamiecDzielona *pamiec_shm;
Sala_A miejsca[3] = {0};
int numery_czlonkow[LICZBA_CZLONKOW_A] = {0};

int main()
{
    char msg_buffer[200];

    srand(time(NULL) ^ getpid());

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    msqid_A = utworz_msq(klucz_msq_A);

    pthread_mutex_init(&mutex, NULL);

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Czekam na rozpoczecie egzaminu\n", getpid());
    wypisz_wiadomosc(msg_buffer);

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

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Komisja rozpoczyna prace\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    pthread_t czlonkowie_komisji[LICZBA_CZLONKOW_A];
    for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
    {
        numery_czlonkow[i] = i;
        if (i == 0)
        {
            if (pthread_create(&czlonkowie_komisji[i], NULL, nadzorca, &numery_czlonkow[i]) == -1)
            {
                perror("pthread_create() | Nie udalo sie stowrzyc watku nadzorca komisja A.");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (pthread_create(&czlonkowie_komisji[i], NULL, czlonek, &numery_czlonkow[i]) == -1)
            {
                perror("pthread_create() | Nie udalo sie stwrozyc watku czlonek komisja A.");
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

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Egzamin dobiegl konca\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
    {
        if (pthread_join(czlonkowie_komisji[i], NULL) == -1)
        {
            perror("pthread_join() | Nie udalo sie przylaczyc watku czlonek komisja A.");
            exit(EXIT_FAILURE);
        }
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "Komisja A kończy prace\n");
    wypisz_wiadomosc(msg_buffer);

    odlacz_shm(pamiec_shm);

    return 0;
}

void *nadzorca(void *args)
{
    int numer_czlonka = *(int *)args;
    char msg_buffer[200];

    while (true)
    {
        ssize_t rozmiar_odpowiedz;

        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa)
        {
            break;
        }

        // Nadzorca sprawdza czy ktos nowy nie wszedl do sali
        MSG_KANDYDAT_WCHODZI_DO_A kandydat_wchodzi;
        rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &kandydat_wchodzi, sizeof(kandydat_wchodzi), KANDYDAT_WCHODZI_DO_A);

        if (rozmiar_odpowiedz != -1)
        {
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid == 0)
                {

                    // Nadzorca znalazl wolne miejsce
                    miejsca[i].pid = kandydat_wchodzi.pid;
                    miejsca[i].numer_na_liscie = kandydat_wchodzi.numer_na_liscie;

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        // Nadzorca sprawdza czy jakis kandydat nie zglasza mu ze ma zdana to czesc egzaminu
        MSG_KANDYDAT_POWTARZA weryfikacja;
        rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &weryfikacja, sizeof(weryfikacja), NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO);

        if (rozmiar_odpowiedz != -1)
        {
            MSG_KANDYDAT_POWTARZA_ODPOWIEDZ_NADZORCY weryfikacja_odpowiedz;
            weryfikacja_odpowiedz.mtype = weryfikacja.pid;
            weryfikacja_odpowiedz.zgoda = (weryfikacja.wynika_a >= 30 && weryfikacja.wynika_a <= 100);

            if (weryfikacja_odpowiedz.zgoda)
            {
                pthread_mutex_lock(&mutex);
                for (int i = 0; i < 3; i++)
                {
                    if (miejsca[i].pid == weryfikacja.pid)
                    {
                        memset(&miejsca[i], 0, sizeof(Sala_A));

                        semafor_p(SEMAFOR_MUTEX);
                        pamiec_shm->liczba_osob_w_A--;
                        semafor_v(SEMAFOR_MUTEX);
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);
            }
            else
            {
                // Nadzorca nie uznal wyniku - kandydat musi zdawac egzamin
                pthread_mutex_lock(&mutex);
                for (int i = 0; i < 3; i++)
                {
                    if (miejsca[i].pid == weryfikacja.pid)
                    {
                        // Resetujemy tylko niezbedne pola, żeby kandydat mógł zdawać
                        memset(miejsca[i].czy_dostal_pytanie, 0, sizeof(miejsca[i].czy_dostal_pytanie));
                        memset(miejsca[i].oceny, 0, sizeof(miejsca[i].oceny));
                        miejsca[i].liczba_ocen = 0;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);
            }

            msq_send(msqid_A, &weryfikacja_odpowiedz, sizeof(weryfikacja_odpowiedz));
        }

        // Nadzorca sprawdza czy wyslal pytania do wszytskich
        for (int i = 0; i < 3; i++)
        {
            pid_t kandydat_pid = 0;
            pthread_mutex_lock(&mutex);

            if (miejsca[i].pid != 0 && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
            {
                kandydat_pid = miejsca[i].pid;
                miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
            }
            pthread_mutex_unlock(&mutex);

            if (kandydat_pid != 0)
            {

                usleep(rand() % 2000 + 5000);

                MSG_PYTANIE pytanie;
                pytanie.mtype = miejsca[i].pid;
                pytanie.pid = miejsca[i].pid;
                msq_send(msqid_A, &pytanie, sizeof(pytanie));
            }
        }

        MSG_ODPOWIEDZ odpowiedz;
        rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);

        if (rozmiar_odpowiedz != -1)
        {
            // odpowiedz otrzymana
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid == odpowiedz.pid)
                {
                    int ocena = rand() % 101;
                    miejsca[i].oceny[numer_czlonka] = ocena;
                    miejsca[i].liczba_ocen++;

                    MSG_WYNIK wynik;
                    wynik.mtype = odpowiedz.pid;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;
                    msq_send(msqid_A, &wynik, sizeof(wynik));

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        pthread_mutex_lock(&mutex);

        for (int i = 0; i < 3; i++)
        {

            if (miejsca[i].liczba_ocen == 5)
            {
                float srednia = 0.0f;
                for (int j = 0; j < LICZBA_CZLONKOW_A; j++)
                {
                    srednia += miejsca[i].oceny[j];
                }

                srednia = srednia / LICZBA_CZLONKOW_A;

                MSG_WYNIK_KONCOWY wynik_koncowy;
                wynik_koncowy.mtype = miejsca[i].pid;
                wynik_koncowy.wynik_koncowy = srednia;
                wynik_koncowy.czy_zdal = (srednia >= 30 && srednia <= 100);
                msq_send(msqid_A, &wynik_koncowy, sizeof(wynik_koncowy));

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Kandydat PID:%d otrzymal wynik koncowy za czesc teorytyczna=%.2f.\n", getpid(), miejsca[i].pid, srednia);
                wypisz_wiadomosc(msg_buffer);

                memset(&miejsca[i], 0, sizeof(Sala_A));

                semafor_p(SEMAFOR_MUTEX);
                pamiec_shm->liczba_osob_w_A--;
                semafor_v(SEMAFOR_MUTEX);

                break;
            }
        }
        pthread_mutex_unlock(&mutex);

        usleep(1000);
    }

    return NULL;
}

void *czlonek(void *args)
{
    ssize_t res;
    int numer_czlonka = *(int *)args;

    while (true)
    {
        semafor_p(SEMAFOR_MUTEX);
        bool egazmin_trwa = pamiec_shm->egzamin_trwa;
        semafor_v(SEMAFOR_MUTEX);

        if (!egazmin_trwa)
        {
            break;
        }
        // Nadzorca sprawdza czy wyslal pytania do wszytskich
        for (int i = 0; i < 3; i++)
        {
            pid_t kandydat_pid = 0;
            pthread_mutex_lock(&mutex);

            if (miejsca[i].pid != 0 && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
            {
                kandydat_pid = miejsca[i].pid;
                miejsca[i].czy_dostal_pytanie[numer_czlonka] = true;
            }
            pthread_mutex_unlock(&mutex);

            if (kandydat_pid != 0)
            {

                usleep(rand() % 2000 + 5000);

                MSG_PYTANIE pytanie;
                pytanie.mtype = miejsca[i].pid;
                pytanie.pid = miejsca[i].pid;
                msq_send(msqid_A, &pytanie, sizeof(pytanie));
            }
        }

        MSG_ODPOWIEDZ odpowiedz;
        res = msq_receive_no_wait(msqid_A, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);

        if (res != -1)
        {
            // Czlonek sprawdza czy otrzymal pytanie
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid == odpowiedz.pid)
                {
                    int ocena = rand() % 101;
                    miejsca[i].oceny[numer_czlonka] = ocena;
                    miejsca[i].liczba_ocen++;

                    MSG_WYNIK wynik;
                    wynik.mtype = odpowiedz.pid;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;
                    msq_send(msqid_A, &wynik, sizeof(wynik));
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        usleep(1000);
    }

    return NULL;
}