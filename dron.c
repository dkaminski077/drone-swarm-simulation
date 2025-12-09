#include "common.h"

void P(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

void V(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(sem_id, &op, 1);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Błąd: Dron musi być uruchomiony przez Operatora (brak ID)!\n");
        return 1;
    }
    int id_wew = atoi(argv[1]);

    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_sem = ftok(FTOK_PATH, 'S');

    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666);
    int sem_id = semget(klucz_sem, ILOSC_SEMAFOROW, 0666);

    if (shm_id == -1 || sem_id == -1) {
        perror("Błąd podłączenia drona do zasobów");
        return 1;
    }

    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);
    if (roj == (void*) -1) {
        perror("Błąd shmat w dronie");
        return 1;
    }

    printf("   [DRON %d] PID: %d. Uruchomiony z pliku exec. Czekam 5s...\n", id_wew, getpid());

    sleep(5);

    printf("   [DRON %d] Koniec pracy. Zwalniam slot i bazę.\n", id_wew);

    P(sem_id, SEM_PAMIEC);
    roj->drony[id_wew].stan = STAN_WOLNY;
    V(sem_id, SEM_PAMIEC);

    V(sem_id, SEM_BAZA);

    shmdt(roj);
    return 0;
}
