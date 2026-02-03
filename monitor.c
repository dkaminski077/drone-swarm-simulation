/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 * Plik: monitor.c (Wizualizacja Stanu)
 * Opis: Pobiera dane z SHM i semaforów, prezentując je w czytelnej formie.
 */

#include "common.h"

static void P(int sem_id, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, -1, 0 };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        perror("semop P");
        exit(1);
    }
}

static void V(int sem_id, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, 1, 0 };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        perror("semop V");
        exit(1);
    }
}

int main() {
    key_t k_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t k_sem = ftok(FTOK_PATH, 'S');

    int shm_id = shmget(k_shm, sizeof(struct StanRoju), 0666);
    int sem_id = semget(k_sem, ILOSC_SEMAFOROW, 0666);

    if (shm_id == -1 || sem_id == -1) {
        perror("Brak IPC (uruchom operatora)");
        return 1;
    }

    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);
    if (roj == (void*)-1) { perror("shmat"); return 1; }

    while (1) {
        int baza, limit, maxlog;
        int aktywne = 0;

        P(sem_id, SEM_PAMIEC);

        baza   = roj->pojemnosc_bazy;
        limit  = roj->aktualny_limit_dronow;
        maxlog = roj->max_limit_logiczny;

        // ile dronów aktualnie działa (nie jest STAN_WOLNY)
        for (int i = 0; i < limit; i++) {
            if (roj->drony[i].stan != STAN_WOLNY) aktywne++;
        }

        V(sem_id, SEM_PAMIEC);

        printf("\033[H\033[J");
        printf("=== MONITOR ROJU (10Hz) ===\n");
        printf("--------------------------\n");
        printf(" POJEMNOŚĆ BAZY:      %d\n", baza);
        printf(" DRONY AKTYWNE:       %d\n", aktywne);
        printf(" LIMIT OBECNIE:       %d / %d\n", limit, maxlog);
        printf("--------------------------\n");
        printf(" [Ctrl+C aby zamknąć]\n");

        fflush(stdout);
        usleep(100000); // 10Hz
    }

    shmdt(roj);
    return 0;
}
