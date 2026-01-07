#include "egzamin.h"

volatile sig_atomic_t egzamin_aktywny = true;

void *nadzorca(void *args);
void *czlonek(void *args);

pthread_mutex_t mutex;

int msqid_A = -1;
int msqid_dziekan_komisja = -1;
PamiecDzielona *pamiec_shm;
Sala_A miejsca[3] = {0};
int numery_czlonkow[LICZBA_CZLONKOW_A] = {0};

int main()
{
    char msg_buffer[200];

    srand(time(NULL) ^ getpid());

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    semafor_v(SEMAFOR_KOMISJA_A_GOTOWA);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    msqid_A = utworz_msq(klucz_msq_A);

    key_t klucz_msq_dziekan_komisja = utworz_klucz(MSQ_DZIEKAN_KOMISJA);
    msqid_dziekan_komisja = utworz_msq(klucz_msq_dziekan_komisja);

    pthread_mutex_init(&mutex, NULL);

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Czekam na rozpoczecie egzaminu\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

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

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Komisja rozpoczyna prace\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

    pthread_t czlonkowie_komisji[LICZBA_CZLONKOW_A];
    for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
    {
        numery_czlonkow[i] = i;
        if (i == 0)
        {
            if (pthread_create(&czlonkowie_komisji[i], NULL, nadzorca, &numery_czlonkow[i]) != 0)
            {
                perror("pthread_create() | Nie udalo sie stowrzyc watku nadzorca komisja A.");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            if (pthread_create(&czlonkowie_komisji[i], NULL, czlonek, &numery_czlonkow[i]) != 0)
            {
                perror("pthread_create() | Nie udalo sie stwrozyc watku czlonek komisja A.");
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

    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A] PID:%d | Egzamin dobiegl konca\n", getpid());
    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

    for (int i = 0; i < LICZBA_CZLONKOW_A; i++)
    {
        int ret = pthread_join(czlonkowie_komisji[i], NULL);
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

    return 0;
}

void *nadzorca(void *args)
{
    int numer_czlonka = *(int *)args;
    char msg_buffer[512];

    while (true)
    {
        ssize_t rozmiar_odpowiedz;

        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int liczba_osob = pamiec_shm->liczba_osob_w_A;
        int kandydatow = pamiec_shm->kandydatow_procesow;
        semafor_v(SEMAFOR_MUTEX);

        if (!egzamin_trwa && kandydatow == 0)
        {
            break;
        }

        if (liczba_osob < 3)
        {
            MSG_KANDYDAT_WCHODZI_DO_A prosba;
            rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &prosba, sizeof(prosba), KANDYDAT_WCHODZI_DO_A);

            if (rozmiar_odpowiedz != -1)
            {
                bool znaleziono = false;

                pthread_mutex_lock(&mutex);
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

                        znaleziono = true;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);

                if (znaleziono)
                {
                    semafor_p(SEMAFOR_MUTEX);
                    pamiec_shm->liczba_osob_w_A++;
                    semafor_v(SEMAFOR_MUTEX);

                    MSG_KANDYDAT_WCHODZI_DO_A_POTWIERDZENIE ok;
                    ok.mtype = MTYPE_A_POTWIERDZENIE + prosba.pid;
                    msq_send(msqid_A, &ok, sizeof(ok));

                    snprintf(msg_buffer, sizeof(msg_buffer), "[NADZORCA A] PID:%d | Wpuscilem kandydata PID:%d do sali.\n", getpid(), prosba.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
                }
            }
        }

        MSG_KANDYDAT_POWTARZA weryfikacja;
        rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &weryfikacja, sizeof(weryfikacja), NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO);

        if (rozmiar_odpowiedz != -1)
        {
            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A NADZORCA] PID:%d | Zglasza sie do mnie kandydat PID:%d ,ktory ma zdana czesc teorytyczna egzaminu\n", getpid(), weryfikacja.pid);
            loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

            MSG_KANDYDAT_POWTARZA_ODPOWIEDZ_NADZORCY weryfikacja_odpowiedz;
            weryfikacja_odpowiedz.mtype = MTYPE_A_WERYFIKACJA_ODP + weryfikacja.pid;
            weryfikacja_odpowiedz.zgoda = (weryfikacja.wynik_a >= 30 && weryfikacja.wynik_a <= 100);

            if (weryfikacja_odpowiedz.zgoda)
            {
                pthread_mutex_lock(&mutex);
                for (int i = 0; i < 3; i++)
                {
                    if (miejsca[i].pid == weryfikacja.pid)
                    {
                        memset(&miejsca[i], 0, sizeof(Sala_A));
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);

                semafor_p(SEMAFOR_MUTEX);
                pamiec_shm->liczba_osob_w_A--;
                semafor_v(SEMAFOR_MUTEX);

                MSG_WYNIK_KONCOWY_DZIEKAN wynik_dla_dziekana;
                wynik_dla_dziekana.mtype = NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA;
                wynik_dla_dziekana.komisja = 'A';
                wynik_dla_dziekana.pid = weryfikacja.pid;
                wynik_dla_dziekana.wynik_koncowy = weryfikacja.wynik_a;
                msq_send(msqid_dziekan_komisja, &wynik_dla_dziekana, sizeof(wynik_dla_dziekana));
            }

            msq_send(msqid_A, &weryfikacja_odpowiedz, sizeof(weryfikacja_odpowiedz));
        }

        if (semafor_wartosc(SEMAFOR_ODPOWIEDZ_A) > 0)
        {
            pid_t kandydat_pid = 0;
            pthread_mutex_lock(&mutex);

            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid != 0 && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
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
                pytanie.mtype = MTYPE_A_PYTANIE + kandydat_pid;
                pytanie.pid = kandydat_pid;
                msq_send(msqid_A, &pytanie, sizeof(pytanie));

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A NADZORCA] PID:%d |Zadaje pytanie dla kandydata PID:%d\n", getpid(), kandydat_pid);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
            }
        }

        MSG_ODPOWIEDZ odpowiedz;
        rozmiar_odpowiedz = msq_receive_no_wait(msqid_A, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);

        if (rozmiar_odpowiedz != -1)
        {
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid == odpowiedz.pid)
                {
                    int ocena = rand() % 101;
                    miejsca[i].oceny[numer_czlonka] = ocena;
                    miejsca[i].liczba_ocen++;

                    MSG_WYNIK wynik;
                    wynik.mtype = MTYPE_A_WYNIK + odpowiedz.pid;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;
                    msq_send(msqid_A, &wynik, sizeof(wynik));

                    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A NADZORCA] PID:%d |Otrzymalem odpowiedz od kandydat PID:%d\n", getpid(), odpowiedz.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        pid_t kandydat_do_oceny = 0;
        float srednia_do_wyslania = 0.0f;

        pthread_mutex_lock(&mutex);
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

                memset(&miejsca[i], 0, sizeof(Sala_A));
                break;
            }
        }
        pthread_mutex_unlock(&mutex);

        if (kandydat_do_oceny != 0)
        {
            MSG_WYNIK_KONCOWY wynik_koncowy;
            wynik_koncowy.mtype = MTYPE_A_WYNIK_KONCOWY + kandydat_do_oceny;
            wynik_koncowy.wynik_koncowy = srednia_do_wyslania;
            wynik_koncowy.czy_zdal = (srednia_do_wyslania >= 30 && srednia_do_wyslania <= 100);

            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A NADZORCA] PID:%d | Kandydat PID:%d otrzymal wynik koncowy za czesc teorytyczna=%.2f.\n", getpid(), kandydat_do_oceny, srednia_do_wyslania);
            loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

            MSG_WYNIK_KONCOWY_DZIEKAN wynik_dla_dziekana;
            wynik_dla_dziekana.mtype = NADZORCA_PRZESYLA_WYNIK_DO_DZIEKANA;
            wynik_dla_dziekana.komisja = 'A';
            wynik_dla_dziekana.pid = kandydat_do_oceny;
            wynik_dla_dziekana.wynik_koncowy = srednia_do_wyslania;

            msq_send(msqid_A, &wynik_koncowy, sizeof(wynik_koncowy));
            msq_send(msqid_dziekan_komisja, &wynik_dla_dziekana, sizeof(wynik_dla_dziekana));

            snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A NADZORCA] PID:%d | Przeyslam do Dziekana wynik kandydata PID:%d\n", getpid(), kandydat_do_oceny);
            loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

            semafor_p(SEMAFOR_MUTEX);
            pamiec_shm->liczba_osob_w_A--;
            semafor_v(SEMAFOR_MUTEX);
        }

        // usleep(10000);
    }

    return NULL;
}

void *czlonek(void *args)
{
    char msg_buffer[512];
    ssize_t res;
    int numer_czlonka = *(int *)args;

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

        if (semafor_wartosc(SEMAFOR_ODPOWIEDZ_A) > 0)
        {
            pid_t kandydat_pid = 0;
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid != 0 && (miejsca[i].czy_dostal_pytanie[numer_czlonka] == false))
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
                pytanie.mtype = MTYPE_A_PYTANIE + kandydat_pid;
                pytanie.pid = kandydat_pid;
                msq_send(msqid_A, &pytanie, sizeof(pytanie));

                snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A CZLONEK %d] PID:%d | Zadaje pytanie dla kandydat PID:%d\n", numer_czlonka + 1, getpid(), kandydat_pid);
                loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);
            }
        }

        MSG_ODPOWIEDZ odpowiedz;
        res = msq_receive_no_wait(msqid_A, &odpowiedz, sizeof(odpowiedz), numer_czlonka + 1);

        if (res != -1)
        {
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < 3; i++)
            {
                if (miejsca[i].pid == odpowiedz.pid)
                {
                    int ocena = rand() % 101;
                    miejsca[i].oceny[numer_czlonka] = ocena;
                    miejsca[i].liczba_ocen++;

                    MSG_WYNIK wynik;
                    wynik.mtype = MTYPE_A_WYNIK + odpowiedz.pid;
                    wynik.numer_czlonka_komisj = numer_czlonka;
                    wynik.ocena = ocena;
                    msq_send(msqid_A, &wynik, sizeof(wynik));

                    snprintf(msg_buffer, sizeof(msg_buffer), "[KOMISJA A CZLONEK %d] PID:%d | Otrzymalem odpowiedz od kandydat PID:%d\n", numer_czlonka + 1, getpid(), odpowiedz.pid);
                    loguj(SEMAFOR_LOGI_KOMISJA_A, LOGI_KOMISJA_A, msg_buffer);

                    break;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        // usleep(10000);
    }

    return NULL;
}