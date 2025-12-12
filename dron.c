#include "common.h"

struct StanRoju *g_roj = NULL;
int g_sem_id = -1;
int g_id_drona = -1;


void P(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

void V(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(sem_id, &op, 1);
}

void atak(int sig) {
    if (g_id_drona == -1) return;

    int bateria = g_roj->drony[g_id_drona].bateria;
    int stan = g_roj->drony[g_id_drona].stan;

    loguj("    [DRON %d] !!! OTRZYMAŁEM ROZKAZ ATAKU SAMOBÓJCZEGO !!! (Bateria: %d%%, Stan: %d)\n", g_id_drona, bateria, stan);

    if (bateria < 20) {
        loguj("    [DRON %d] Bateria zbyt słaba na atak (<20%%). Ignoruje rozkaz.\n", g_id_drona);
        return;
    }

    loguj("    [DRON %d] !!! Atak samobójczy wykonany.\n", g_id_drona);

    P(g_sem_id, SEM_PAMIEC);
    g_roj->drony[g_id_drona].stan = STAN_WOLNY;
    V(g_sem_id, SEM_PAMIEC);

    if(stan == STAN_LADOWANIE) {
        V(g_sem_id, SEM_BAZA);
    }

    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        loguj("Błąd: Dron musi być uruchomiony przez Operatora (brak ID)!\n");
        return 1;
    }
    int id_wew = atoi(argv[1]);

    g_id_drona = id_wew;

    srand(time(NULL) ^ getpid());

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

    g_roj = roj;
    g_sem_id = sem_id;

    signal(SIGUSR1, atak);

    loguj("   [DRON %d] PID: %d. Uruchomiony. Bateria 0%%.\n", id_wew, getpid());

    while(1) {
        sleep(2); //T1

        P(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].bateria = 100;
        roj->drony[id_wew].liczba_cykli++;
        int cykle = roj->drony[id_wew].liczba_cykli;
        V(sem_id, SEM_PAMIEC);

        if (cykle > MAX_CYKLI) {
            loguj("    [DRON %d] Zużyty (cykle: %d). Idę na złom.\n", id_wew, cykle);
            break;
        }

        loguj("    [DRON %d] Naładowany. Czekam na wylot.\n", id_wew);

        int bramka = (rand()%2) + SEM_WEJSCIE_1;

        P(sem_id, bramka);
        loguj("    [DRON %d] Wylatuję bramką %d...\n", id_wew, bramka - SEM_WEJSCIE_1 +1);
        sleep(1);
        V(sem_id, bramka);

        P(sem_id, SEM_PAMIEC);
        int zniszcz_platforme = 0;
        if (roj->platformy_do_usuniecia > 0) {
            roj->platformy_do_usuniecia--;
            zniszcz_platforme = 1;
        }
        V(sem_id, SEM_PAMIEC);

        if (zniszcz_platforme) {
            loguj("    [DRON %d] Demontuje za sobą platformę.\n", id_wew);
        } else {
            V(sem_id, SEM_BAZA);
        }

        P(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_LOT;
        V(sem_id, SEM_PAMIEC);

        loguj("    [DRON %d] Wyleciałem. Latam...\n", id_wew);

        while(1) {
            sleep(1);

            P(sem_id, SEM_PAMIEC);
            roj->drony[id_wew].bateria -= 15;
            int poziom = roj->drony[id_wew].bateria;
            V(sem_id, SEM_PAMIEC);

            if (poziom <= BAT_CRITICAL) {
                loguj("    [DRON %d] Bateria słaba (%d%%). Wracam do bazy.\n", id_wew, poziom);
                break;
            }

            if (poziom <= 0) {
                loguj("    [DRON %d] Bateria 0%%. Rozbity.\n", id_wew);
                P(sem_id, SEM_PAMIEC);
                roj->drony[id_wew].stan = STAN_WOLNY;
                V(sem_id, SEM_PAMIEC);
                exit(0);
            }
        }

        P(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_POWROT;
        V(sem_id, SEM_PAMIEC);

        loguj("    [DRON %d] Zbliżam się do bazy. Próbuję lądować...\n", id_wew);

        while(1) {
            struct sembuf wejdz = {SEM_BAZA, -1, IPC_NOWAIT};

            if (semop(sem_id, &wejdz, 1) == 0) {
                break;
            } else {
                P(sem_id, SEM_PAMIEC);
                roj->drony[id_wew].bateria -= 10;
                int poziom = roj->drony[id_wew].bateria;
                V(sem_id, SEM_PAMIEC);

                loguj("    [DRON %d] Baza pełna. Krążę... (Bateria: %d%%)\n", id_wew, poziom);

                if (poziom <=0 ) {
                    loguj("    [DRON %d] Bateria 0%%. Rozbity.\n", id_wew);
                    P(sem_id, SEM_PAMIEC);
                    roj->drony[id_wew].stan = STAN_WOLNY;
                    V(sem_id, SEM_PAMIEC);
                    exit(0);
                }
                sleep(1);
            }
        }

        bramka = (rand()%2) + SEM_WEJSCIE_1;
        P(sem_id, bramka);
        loguj("    [DRON %d] Ląduję bramką %d...\n", id_wew, bramka - SEM_WEJSCIE_1 + 1);
        sleep(1);
        V(sem_id, bramka);

        P(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_LADOWANIE;
        V(sem_id, SEM_PAMIEC);
    }

    P(sem_id, SEM_PAMIEC);
    roj->drony[id_wew].stan = STAN_WOLNY;
    V(sem_id, SEM_PAMIEC);

    V(sem_id, SEM_BAZA);

    loguj("    [DRON %d] Złomowanie zakończone.\n", id_wew);

    shmdt(roj);
    return 0;
}