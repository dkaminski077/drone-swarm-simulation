#include "common.h"

int g_shm_id = -1;
int g_sem_id = -1;
int g_msg_id = -1;


void P(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

void V(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(sem_id, &op, 1);
}

void sprzatanie(int sig) {
    if (g_shm_id != -1) {
        if(shmctl(g_shm_id, IPC_RMID, NULL) != -1) loguj("\n[OPERATOR] Pamięć dzielona usunięta.\n");
    } 

    if (g_sem_id != -1) {
        if(semctl(g_sem_id, 0, IPC_RMID) != -1) loguj("[OPERATOR] Semafory usunięte.\n");
    } 

    if (g_msg_id != -1) {
        if(msgctl(g_msg_id, IPC_RMID, NULL) != -1) loguj("[OPERATOR] Kolejka komunikatów usunięta.\n");
    } 

    loguj(CZERWONY "[OPERATOR] KONIEC SYMULACJI" RESET "\n");
        
    kill(0, SIGKILL);
    exit(0);

}

int main() {
    signal(SIGINT, sprzatanie);

    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_sem = ftok(FTOK_PATH, 'S');
    key_t klucz_msg = ftok(FTOK_PATH, 'Q');

    if (klucz_shm == -1 || klucz_sem == -1 || klucz_msg == -1) {
        perror("Błąd ftok");
        return 1;
    }

    //---KOLEJKA KOMUNIKATOW----
    int msg_id = msgget(klucz_msg, 0666 | IPC_CREAT);

    if (msg_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        return 1;
    }
    g_msg_id = msg_id;

    //---PAMIEC DZIELONA---
    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666 | IPC_CREAT);

    if (shm_id == -1) {
        perror("Błąd tworzenia pamięci (shmget)");
        return 1;
    }
    g_shm_id = shm_id;

    //---SEMAFORY---
    int sem_id = semget(klucz_sem, ILOSC_SEMAFOROW, 0666 | IPC_CREAT);

    if (sem_id == -1) {
        perror("Błąd tworzenia semaforów");
        return 1;
    }
    g_sem_id = sem_id;

    if (semctl(sem_id, SEM_BAZA, SETVAL, POJEMNOSC_BAZY) == -1) perror("Błąd SEM_BAZA");
    if (semctl(sem_id, SEM_WEJSCIE_1, SETVAL, 1) == -1) perror("Błąd SEM_WEJSCIE_1");
    if (semctl(sem_id, SEM_WEJSCIE_2, SETVAL, 1) == -1) perror("Błąd SEM_WEJSCIE_2");
    if (semctl(sem_id, SEM_PAMIEC, SETVAL, 1) == -1) perror("Błąd SEM_PAMIEC");

    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);

    if (roj == (void*) -1) {
        perror("Błąd shmat");
        return 1;
    }

    roj->pojemnosc_bazy = POJEMNOSC_BAZY;
    roj->aktualny_limit_dronow = N;
    roj->platformy_do_usuniecia = 0;

    for (int i=0; i<MAX_DRONOW; i++) {
        roj->drony[i].stan = STAN_WOLNY;
    }

    loguj(ZIELONY "[OPERATOR] START SYSTEMU. Baza: %d | Drony %d/%d (Ctrl+C aby zakończyć)" RESET "\n", POJEMNOSC_BAZY, N, MAX_DRONOW);

    int start_index = 0;

    while(1) {  
        struct Komunikat msg;

        while (msgrcv(msg_id, &msg, sizeof(int), 0, IPC_NOWAIT) != -1) {
            if (msg.mtype == TYP_DODAJ_PLATFORMY) {
                if(roj->aktualny_limit_dronow < MAX_DRONOW) {
                    roj->aktualny_limit_dronow++;
                    if((roj->pojemnosc_bazy + 1) * 2 < roj->aktualny_limit_dronow) {
                        V(sem_id, SEM_BAZA);
                        roj->pojemnosc_bazy++;
                        loguj(ZIELONY "[SYGNAŁ 1] Rozbudowa! Baza: %d, Drony: %d" RESET "\n", roj->pojemnosc_bazy, roj->aktualny_limit_dronow);
                    } else {
                        loguj(ZIELONY "[SYGNAŁ 1] Dodano drona. Baza: %d, Drony: %d" RESET "\n", roj->pojemnosc_bazy, roj->aktualny_limit_dronow);
                    }
                } else {
                    loguj(ZOLTY "[SYGNAŁ 1] Osiągnięto MAX_DRONOW (2*N)" RESET "\n");
                }
            } else if (msg.mtype == TYP_USUN_PLATFORMY) {
                int stary_limit = roj->aktualny_limit_dronow;
                int nowy_limit = stary_limit / 2;
                if (nowy_limit < 4) nowy_limit = 4;

                int max_dozwolona_baza = (nowy_limit - 1) / 2;
                if(max_dozwolona_baza < 1) max_dozwolona_baza = 1;

                int obecna_baza = roj->pojemnosc_bazy;
                int do_usuniecia = obecna_baza - max_dozwolona_baza;

                if (do_usuniecia < 0) do_usuniecia = 0;

                loguj(ZOLTY "[SYGNAŁ 2] Redukcja! Drony: %d->%d. Baza: %d->%d" RESET "\n", stary_limit, nowy_limit, obecna_baza, max_dozwolona_baza);

                roj->aktualny_limit_dronow = nowy_limit;
                roj->pojemnosc_bazy = max_dozwolona_baza;

                int usuniete_natychmiast = 0;
                for (int i=0; i<do_usuniecia; i++) {
                    struct sembuf op = {SEM_BAZA, -1, IPC_NOWAIT};
                    if(semop(sem_id, &op, 1) == 0) {
                        usuniete_natychmiast++;
                    }
                }

                int reszta = do_usuniecia - usuniete_natychmiast;
                if (reszta > 0) {
                    P(sem_id, SEM_PAMIEC);
                    roj->platformy_do_usuniecia += reszta;
                    V(sem_id, SEM_PAMIEC);
                    loguj("[SYGNAŁ 2] Zdemontowano od razu: %d platform. Czeka na demontaż: %d.\n", usuniete_natychmiast, reszta);
                } else {
                    loguj("[SYGNAŁ 2] Zdemontowano od razu wszystkie %d platform.\n", usuniete_natychmiast);
                }
            }
        }

        for (int k=0; k<roj->aktualny_limit_dronow; k++) {
            int i = (start_index + k) % roj->aktualny_limit_dronow; 
            if (roj->drony[i].stan == STAN_WOLNY) {
                struct sembuf zajmij_miejsce = {SEM_BAZA, -1, IPC_NOWAIT};
                if(semop(sem_id, &zajmij_miejsce, 1) == 0) {
                    loguj("[OPERATOR] Wykryto brak drona na pozycji %d. Tworzę nowego...\n", i);

                    pid_t pid = fork();

                    if (pid == 0) {
                        char bufor_id[10];
                        sprintf(bufor_id, "%d", i);
                        execl("./dron", "dron", bufor_id, NULL);
                        perror("Błąd execl");
                        exit(1);
                    } else if (pid > 0) {
                        P(sem_id, SEM_PAMIEC);
                        roj->drony[i].pid = pid;
                        roj->drony[i].id_wewnetrzne = i;
                        roj->drony[i].bateria = 0;
                        roj->drony[i].liczba_cykli = 0;
                        roj->drony[i].stan = STAN_LADOWANIE;
                        V(sem_id, SEM_PAMIEC);

                        start_index = (i + 1) % roj->aktualny_limit_dronow;
                    }
                }
            }
        }
        sleep(2);
    }
    return 0;
}