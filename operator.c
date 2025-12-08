#include "common.h"

void P(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

void V(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(sem_id, &op, 1);
}

int main() {
    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_sem = ftok(FTOK_PATH, 'S');

    if (klucz_shm == -1 || klucz_sem == -1) {
        perror("Błąd ftok");
        return 1;
    }

    //---PAMIEC DZIELONA---
    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666 | IPC_CREAT);

    if (shm_id == -1) {
        perror("Błąd tworzenia pamięci (shmget)");
        return 1;
    }

    //---SEMAFORY---
    int sem_id = semget(klucz_sem, ILOSC_SEMAFOROW, 0666 | IPC_CREAT);

    if (sem_id == -1) {
        perror("Błąd tworzenia semaforów");
        return 1;
    }

    if (semctl(sem_id, SEM_BAZA, SETVAL, POJEMNOSC_BAZY) == -1) {
        perror("Błąd inizjalizacji SEM_BAZA");
        return 1;
    }

    if (semctl(sem_id, SEM_WEJSCIE_1, SETVAL, 1) == -1) perror("Błąd SEM_WEJSCIE_1");
    if (semctl(sem_id, SEM_WEJSCIE_2, SETVAL, 1) == -1) perror("Błąd SEM_WEJSCIE_2");
    if (semctl(sem_id, SEM_PAMIEC, SETVAL, 1) == -1) perror("Błąd SEM_PAMIEC");

    printf("[OPERATOR] Semfory ustawione. Baza: %d miejsc, Wejścia otwarte.\n", POJEMNOSC_BAZY);


    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);

    if (roj == (void*) -1) {
        perror("Błąd shmat");
        return 1;
    }

    roj->pojemnosc_bazy = POJEMNOSC_BAZY;
    printf("[OPERATOR] Baza gotowa. Pojemność: %d\n", roj->pojemnosc_bazy);

    for (int i=0; i<N*2; i++) {
        roj->drony[i].stan = STAN_WOLNY;
    }

    printf("[OPERATOR] ENTER aby zakonczyc\n");
    getchar();

    shmdt(roj);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    printf("[OPERATOR] Zasoby usunięte. KONIEC\n");

    return 0;
}
