#include "egzamin.h"

int main()
{

    srand(time(NULL) ^ getpid());

    char msg_buffer[200];
    PamiecDzielona *pamiec_shm;

    key_t klucz_sem = utworz_klucz(66);
    utworz_semafory(klucz_sem);

    key_t klucz_shm = utworz_klucz(77);
    utworz_shm(klucz_shm);
    dolacz_shm(&pamiec_shm);

    key_t klucz_msq_budynek = utworz_klucz(MSQ_KOLEJKA_BUDYNEK);
    int msqid_budynek = utworz_msq(klucz_msq_budynek);

    Kandydat kandydat;
    init_kandydat(getpid(), &kandydat);

    // Kandydat ustawia sie przed budynkiem

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Ustawia sie w kolejce przed budynkiem.\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    MSG_ZGLOSZENIE zgloszenie;
    zgloszenie.mtype = KANDYDAT_PRZESYLA_MATURE;
    zgloszenie.kandydat = kandydat;
    msq_send(msqid_budynek, &zgloszenie, sizeof(zgloszenie));

    // Kandydat czeka na decyzje dziekana
    MSG_DECYZJA decyzja;
    msq_receive(msqid_budynek, &decyzja, sizeof(decyzja), getpid());

    MSG_POTWIERDZENIE potwierdzenie;

    if (decyzja.numer_na_liscie == -1)
    {
        potwierdzenie.mtype = getpid();
        potwierdzenie.pid = getpid();
        msq_send(msqid_budynek, &potwierdzenie, sizeof(potwierdzenie));

        semafor_p(SEMAFOR_MUTEX);
        pamiec_shm->pozostalo_kandydatow--;
        if (pamiec_shm->pozostalo_kandydatow == 0)
        {
            pamiec_shm->egzamin_trwa = false;
        }

        semafor_v(SEMAFOR_MUTEX);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem decyzje od dziekana i koncze udzial w egzaminie.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        odlacz_shm(pamiec_shm);
        exit(EXIT_SUCCESS);
    }
    else
    {
        potwierdzenie.mtype = getpid();
        potwierdzenie.pid = getpid();

        msq_send(msqid_budynek, &potwierdzenie, sizeof(potwierdzenie));
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem decyzje od dziekana ustawiam sie w kolejce do komisji A.\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // KOMUNIKACJA Z KOMISJA A

    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    int msqid_A = utworz_msq(klucz_msq_A);

    while (true)
    {
        semafor_p(SEMAFOR_MUTEX);
        bool egzamin_trwa = pamiec_shm->egzamin_trwa;
        int aktualny = pamiec_shm->nastepny_do_komisja_A;
        int osoby = pamiec_shm->liczba_osob_w_A;
        semafor_v(SEMAFOR_MUTEX);

        if (egzamin_trwa && (aktualny == decyzja.numer_na_liscie) && osoby < 3)
        {
            // Wysyla komunikat do nadzorcy komisji A

            MSG_KANDYDAT_WCHODZI_DO_A zgloszenia_A;
            zgloszenia_A.mtype = KANDYDAT_WCHODZI_DO_A;
            zgloszenia_A.numer_na_liscie = decyzja.numer_na_liscie;
            zgloszenia_A.pid = getpid();
            msq_send(msqid_A, &zgloszenia_A, sizeof(zgloszenia_A));

            semafor_p(SEMAFOR_MUTEX);
            pamiec_shm->nastepny_do_komisja_A++;
            pamiec_shm->liczba_osob_w_A++;
            semafor_v(SEMAFOR_MUTEX);

            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Wchodze na czesc teorytyczna egzaminu do Komisji A.\n", getpid());
            wypisz_wiadomosc(msg_buffer);

            break;
        }

        usleep(1000);
    }

    bool czy_musze_zdawac = false;

    if (kandydat.czy_powtarza_egzamin)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Mam zdana czesc teorytyczna egzaminu. Czekam na weryfikacje od nadzorcy Komisji A.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        MSG_KANDYDAT_POWTARZA weryfikacja;
        weryfikacja.mtype = NADZORCA_KOMISJI_A_WERYFIKUJE_WYNIK_POWTARZAJACEGO;
        weryfikacja.pid = getpid();
        weryfikacja.wynika_a = kandydat.wynik_a;

        msq_send(msqid_A, &weryfikacja, sizeof(weryfikacja));

        MSG_KANDYDAT_POWTARZA_ODPOWIEDZ_NADZORCY weryfikacja_odpowiedz;
        msq_receive(msqid_A, &weryfikacja_odpowiedz, sizeof(weryfikacja_odpowiedz), getpid());

        if (weryfikacja_odpowiedz.zgoda)
        {

            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Nadzorca Komisji A uznaje moj wynik z egazminu. Ustawiam sie w kolejce do Komisji B.\n", getpid());
            wypisz_wiadomosc(msg_buffer);
        }
        else
        {
            czy_musze_zdawac = true;
            // Tutaj trzeba sie zastanowic czy wystepuje taki przypadek
            snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Nadzorca Komisji A nie uznal mojego wyniku z egazminu. Czekam na pytania od Komisji A.\n", getpid());
            wypisz_wiadomosc(msg_buffer);
        }
    }
    else
    {
        czy_musze_zdawac = true;
    }

    if (czy_musze_zdawac)
    {
        // Czekam na pytania od komisji A
        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Czekam na otrzymanie wszytskich pytan od czlonkow Komisji A.\n", getpid());
        wypisz_wiadomosc(msg_buffer);
        for (int i = 0; i < 5; i++)
        {
            MSG_PYTANIE pytanie;
            msq_receive(msqid_A, &pytanie, sizeof(pytanie), getpid());
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem wszystkie pytania od Komisji A. Zaczynam opracowywac odpowiedzi.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        usleep(rand() % 3000);

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Opracowalem pytania od komisji A. Czekam az bede mogl odpowiadac.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        semafor_p(SEMAFOR_ODPOWIEDZ_A);
        for (int i = 1; i <= 5; i++)
        {
            MSG_ODPOWIEDZ odpowiedz;
            odpowiedz.mtype = i;
            odpowiedz.pid = getpid();

            msq_send(msqid_A, &odpowiedz, sizeof(odpowiedz));
        }

        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Udzieliem odpowiedzi na wszytskie pytania Komisji A. Czekam na wyniki za czesc teorytyczna egzaminu.\n", getpid());
        wypisz_wiadomosc(msg_buffer);

        for (int i = 0; i < 5; i++)
        {
            MSG_WYNIK wynik;
            msq_receive(msqid_A, &wynik, sizeof(wynik), getpid());
        }
        semafor_v(SEMAFOR_ODPOWIEDZ_A);
    }

    semafor_p(SEMAFOR_MUTEX);
    pamiec_shm->pozostalo_kandydatow--;
    if (pamiec_shm->pozostalo_kandydatow == 0)
    {
        pamiec_shm->egzamin_trwa = false;
    }
    semafor_v(SEMAFOR_MUTEX);

    odlacz_shm(pamiec_shm);

    return 0;
}