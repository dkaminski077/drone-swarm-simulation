/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 * 
 * Plik: operator.c (Zarządca Systemu)
 * 
 * Opis działania:
 * - Inicjalizuje zasoby IPC: Pamięć Dzieloną, Semafory, Kolejki Komunikatów.
 * - Zarządza cyklem życia procesorów potomnych (Dronów) używjąc funkcji fork() i exec().
 * - Implementuje obsługę bezpiecznego zamknięcia systemu.
 * - Obsługuje komunikaty od Dowódcy (zmiana limitów bazy/roju).
 * - Realizuje mechanizm odroczonego zwlaniania zasobów.
*/

#include "common.h"

/*
 * ZMIENNE GLOBALNE
 * Potrzebne do procedury sprzątania
*/
int g_shm_id = -1;
int g_sem_id = -1;
int g_msg_id = -1;

// Operacja P (Opuszczanie/Czekanie)
void P(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

// Operacja V (Podniesienie/Sygnalizowanie)
void V(int sem_id, int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(sem_id, &op, 1);
}

/*
 * HANDLER SYGNAŁU SIGINT (Ctrl+C)
 * Realizuje bezpieczne zamknięcie systemu.
 * Usuwa zaoby IPC i zabija procesy potomne, aby nie zostały tzw. "sieroty".
*/
void sprzatanie(int sig) {
    // 1. Usuwanie pamięci dzielonej
    if (g_shm_id != -1) {
        if(shmctl(g_shm_id, IPC_RMID, NULL) != -1) loguj("\n[OPERATOR] Pamięć dzielona usunięta.\n");
    } 

    // 2. Usuwanie tablicy semaforów
    if (g_sem_id != -1) {
        if(semctl(g_sem_id, 0, IPC_RMID) != -1) loguj("[OPERATOR] Semafory usunięte.\n");
    } 

    // 3. Usuwanie kolejki komunikatów
    if (g_msg_id != -1) {
        if(msgctl(g_msg_id, IPC_RMID, NULL) != -1) loguj("[OPERATOR] Kolejka komunikatów usunięta.\n");
    } 

    loguj(CZERWONY "[OPERATOR] KONIEC SYMULACJI" RESET "\n");
        
    // SIGKILL do dgrupy prpcesów (zabija wszystkie drony)
    kill(0, SIGKILL);
    exit(0);

}

int main(int argc, char *argv[]) {
    int n = DEFAULT_N;
    int baza = DEFAULT_POJEMNOSC_BAZY;

    if (argc == 3) {
        n = atoi(argv[1]);
        baza = atoi(argv[2]);

        // Walidacja
        if (n > LIMIT_TECHNICZNY) {
            printf(CZERWONY "Błąd: Przekroczono limit techniczny (%d)!\n" RESET, LIMIT_TECHNICZNY);
            return 1;
        }

        // WARUNEK 1: Spójność z redukcją (nowy_limit < 4)
        if (n < 4) {
            printf(CZERWONY "Błąd: Minimalna liczba dronów to 4!\n" RESET);
            return 1;
        }

        // WARUNEK 2: Spójność ze wzorem: max_dozwolona_baza = (n - 1) / 2
        int max_mozliwa_baza = (n - 1) / 2;
        
        // Zabezpieczenie, żeby baza była przynajmniej 1
        if (max_mozliwa_baza < 1) max_mozliwa_baza = 1;

        if (baza > max_mozliwa_baza) {
            printf(CZERWONY "Błąd: Zbyt duża baza dla %d dronów!\n" RESET, n);
            printf("Max baza to: %d (podałeś: %d).\n", max_mozliwa_baza, baza);
            return 1;
        }
        
        if (baza <= 0) {
             printf(CZERWONY "Błąd: Baza musi mieć minimum 1 miejsce.\n" RESET);
             return 1;
        }
    } else if (argc != 1) {
        printf("Użycie: ./operator [Liczba Dronów] [Pojemność Bazy]\n");
        return 1;
    }

    // Rejestracja handlera do przechwytywania Ctrl+C
    signal(SIGINT, sprzatanie);

    // Generowanie kluczy IPC
    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_sem = ftok(FTOK_PATH, 'S');
    key_t klucz_msg = ftok(FTOK_PATH, 'Q');

    if (klucz_shm == -1 || klucz_sem == -1 || klucz_msg == -1) {
        perror("Błąd ftok");
        return 1;
    }

    // --- INICJALIZACJA ZASOBÓW IPC ---

    // Kolejka komunikatów (komunikacja Dowódca -> Operator)
    int msg_id = msgget(klucz_msg, 0666 | IPC_CREAT);
    if (msg_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        return 1;
    }
    g_msg_id = msg_id;

    // Pamięć dzielona (stan roju)
    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666 | IPC_CREAT);
    if (shm_id == -1) {
        perror("Błąd tworzenia pamięci (shmget)");
        return 1;
    }
    g_shm_id = shm_id;

    // Semafory (synchronizacja)
    int sem_id = semget(klucz_sem, ILOSC_SEMAFOROW, 0666 | IPC_CREAT);
    if (sem_id == -1) {
        perror("Błąd tworzenia semaforów");
        return 1;
    }
    g_sem_id = sem_id;

    // Ustawienie wartości począrkowych semaforów
    if (semctl(sem_id, SEM_BAZA, SETVAL, baza) == -1) perror("Błąd SEM_BAZA");
    if (semctl(sem_id, SEM_WEJSCIE_1, SETVAL, 1) == -1) perror("Błąd SEM_WEJSCIE_1");
    if (semctl(sem_id, SEM_WEJSCIE_2, SETVAL, 1) == -1) perror("Błąd SEM_WEJSCIE_2");
    if (semctl(sem_id, SEM_PAMIEC, SETVAL, 1) == -1) perror("Błąd SEM_PAMIEC");

    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);

    if (roj == (void*) -1) {
        perror("Błąd shmat");
        return 1;
    }

    // Inicjalizacja stanu początkowego w pamięci dzielonej
    roj->pojemnosc_bazy = baza;
    roj->aktualny_limit_dronow = n;
    roj->max_limit_logiczny = n * 2;
    roj->platformy_do_usuniecia = 0;    // Licznik długu

    for (int i=0; i< n*2; i++) {
        roj->drony[i].stan = STAN_WOLNY;
    }

    loguj(ZIELONY "[OPERATOR] START SYSTEMU. Baza: %d | Drony %d/%d (Ctrl+C aby zakończyć)" RESET "\n", baza, n, n*2);

    int start_index = 0;

    // GŁÓWNA PĘTLA OPERATORA
    while(1) {  

        // waitpid w pętli, aby posprzątać martwe procesy
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) {}

        struct Komunikat msg;

        // Obsługa komunikatów od Dowódcy (IPC_NOWAIT - nie blokuj, jeśli nie ma waidomośći)
        while (msgrcv(msg_id, &msg, sizeof(int), 0, IPC_NOWAIT) != -1) {

            // --- SYGNAŁ 1: ROZBUDOWA ---
            if (msg.mtype == TYP_DODAJ_PLATFORMY) {
                int limit_logiczny = n*2;
                if(roj->aktualny_limit_dronow < limit_logiczny && roj->aktualny_limit_dronow < LIMIT_TECHNICZNY) {
                    roj->aktualny_limit_dronow++;
                    // Warunek: Baza w danym momencie może pomieścić co najwyżej P (P<N/2) dronów
                    if((roj->pojemnosc_bazy + 1) * 2 < roj->aktualny_limit_dronow) {
                        V(sem_id, SEM_BAZA);    // Zwiększenie semafora
                        roj->pojemnosc_bazy++;
                        loguj(ZIELONY "[SYGNAŁ 1] Rozbudowa! Baza: %d, Drony: %d" RESET "\n", roj->pojemnosc_bazy, roj->aktualny_limit_dronow);
                    } else {
                        loguj(ZIELONY "[SYGNAŁ 1] Dodano drona. Baza: %d, Drony: %d" RESET "\n", roj->pojemnosc_bazy, roj->aktualny_limit_dronow);
                    }
                } else {
                    loguj(ZOLTY "[SYGNAŁ 1] Osiągnięto MAX_DRONOW (2*N)" RESET "\n");
                }
            }
            // --- SYGNAŁ 2: REDUKCJA ---
            else if (msg.mtype == TYP_USUN_PLATFORMY) {
                int stary_limit = roj->aktualny_limit_dronow;
                int nowy_limit = stary_limit / 2;
                if (nowy_limit < 4) nowy_limit = 4;    // Minimalny limit

                int max_dozwolona_baza = (nowy_limit - 1) / 2;
                if(max_dozwolona_baza < 1) max_dozwolona_baza = 1;

                int obecna_baza = roj->pojemnosc_bazy;
                int do_usuniecia = obecna_baza - max_dozwolona_baza;

                if (do_usuniecia < 0) do_usuniecia = 0;

                loguj(ZOLTY "[SYGNAŁ 2] Redukcja! Drony: %d->%d. Baza: %d->%d" RESET "\n", stary_limit, nowy_limit, obecna_baza, max_dozwolona_baza);

                // Aktualizacja stanu logicznego
                roj->aktualny_limit_dronow = nowy_limit;
                roj->pojemnosc_bazy = max_dozwolona_baza;

                /*
                 * Próbujemy zająć semafor bazy (usunąć platformy).
                 * Jeśli semafor jest zajęty (baza zajęta), nie czekamy.
                 * Zapisujemy "dług" w zmiennej plataformy_do_usuniecia.
                 * Drony "spłacą" ten dług (usuną platformy) przy wylocie z bazy.
                */
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
                    roj->platformy_do_usuniecia += reszta;  // Zapis długu
                    V(sem_id, SEM_PAMIEC);
                    loguj("[SYGNAŁ 2] Zdemontowano od razu: %d platform. Czeka na demontaż: %d.\n", usuniete_natychmiast, reszta);
                } else {
                    loguj("[SYGNAŁ 2] Zdemontowano od razu wszystkie %d platform.\n", usuniete_natychmiast);
                }
            }
        }

        // --- ZARZĄDZANIE DRONAMI ---
        for (int k=0; k<roj->aktualny_limit_dronow; k++) {
            int i = (start_index + k) % roj->aktualny_limit_dronow; 

            // Jeśli slot jest wolny, tworzymy nowy proces drona
            if (roj->drony[i].stan == STAN_WOLNY) {
                struct sembuf zajmij_miejsce = {SEM_BAZA, -1, IPC_NOWAIT};
                if(semop(sem_id, &zajmij_miejsce, 1) == 0) {
                    loguj("[OPERATOR] Wykryto brak drona na pozycji %d. Tworzę nowego...\n", i);

                    pid_t pid = fork();

                    if (pid == 0) {
                        // KOD PROCESU POTOMNEGO (DRONA)
                        char bufor_id[10];
                        sprintf(bufor_id, "%d", i);
                        execl("./dron", "dron", bufor_id, NULL);
                        perror("Błąd execl");
                        exit(1);
                    } else if (pid > 0) {
                        // Inicjalizacja struktur w pamięci dzielonej
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
        sleep(2);   // Cykl pracy operatora
    }
    return 0;
}