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

    snprintf(msg_buffer, sizeof(msg_buffer), "[Kandydat] PID:%d | Ustawia sie w kolejce przed budynkiem\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    MSG_ZGLOSZENIE zgloszenie;
    zgloszenie.mtype = DZIEKAN;
    zgloszenie.kandydat = kandydat;
    msq_send(msqid_budynek, &zgloszenie, sizeof(zgloszenie));

    // Kandydat czeka na decyzje dziekana
    MSG_DECYZJA decyzja;
    msq_receive(msqid_budynek, &decyzja, sizeof(decyzja), getpid());

    MSG_POTWIERDZENIE potwierdzenie;
    potwierdzenie.mtype = getpid();
    potwierdzenie.pid = getpid();

    msq_send(msqid_budynek, &potwierdzenie, sizeof(potwierdzenie));

    if (decyzja.numer_na_liscie == -1)
    {
        snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem decyzje od dziekana i koncze udzial w egzaminie\n", getpid());
        wypisz_wiadomosc(msg_buffer);
        exit(EXIT_SUCCESS);
    }

    snprintf(msg_buffer, sizeof(msg_buffer), "[KANDYDAT] PID:%d | Otrzymalem decyzje od dziekana ustawiam sie w kolejce do komisji A\n", getpid());
    wypisz_wiadomosc(msg_buffer);

    // KOMUNIKACJA Z KOMISJA A

    key_t klucz_msq_A = utworz_klucz(MSQ_KOLEJKA_EGZAMIN_A);
    int msqid_A = utworz_msq(klucz_msq_A);

    sleep(30);

    odlacz_shm(pamiec_shm);

    return 0;
}