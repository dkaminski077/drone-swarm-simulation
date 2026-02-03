/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 * * Plik: operator.c (Zarządca Systemu)
 * * Opis działania:
 * - Inicjalizuje kluczowe zasoby IPC: Pamięć Wspólną, Semafory oraz Kolejki Komunikatów.
 * - Nadzoruje cykl życia procesów potomnych (Dronów) poprzez mechanizmy fork() i execl().
 * - Gwarantuje bezpieczeństwo struktur danych dzięki obsłudze sygnałów (sprzątanie zasobów).
 * - Przetwarza rozkazy taktyczne od Dowódcy (dynamiczne skalowanie populacji i bazy).
 * - Implementuje zaawansowany mechanizm długu (Deferred Release) przy redukcji zasobów.
*/

#include "common.h"

/*
 * ZMIENNE GLOBALNE
 * Przechowują identyfikatory IPC niezbędne dla procedury kończącej system.
*/
int g_shm_id = -1;
int g_sem_id = -1;
int g_msg_id = -1;

/* --- FUNKCJE SYNCHRONIZACJI (MUTEXY Z FLAGĄ SEM_UNDO) --- */

static void P_mutex(int sem_id, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, -1, SEM_UNDO };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue; // Ponów w przypadku przerwania sygnałem
        perror("semop P_mutex");
        exit(1);
    }
}

static void V_mutex(int sem_id, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num,  1, SEM_UNDO };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        perror("semop V_mutex");
        exit(1);
    }
}

/* --- FUNKCJE LICZNIKOWE (ZASOBY BAZY - BEZ SEM_UNDO) --- */

static void P_cnt_nowait(int sem_id, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, -1, IPC_NOWAIT };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        break; // Obsługa niepowodzenia realizowana w logice wywołującej
    }
}

static void V_cnt(int sem_id, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, 1, 0 };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        perror("semop V_cnt");
        exit(1);
    }
}

/*
 * FUNKCJA: odzyskaj_slot
 * Opis: Mechanizm Self-Healing. Przywraca spójność pamięci dzielonej po wykryciu 
 * zakończenia procesu drona (np. awaria, zestrzelenie). Obsługuje spłatę długu bazy.
*/
static void odzyskaj_slot(pid_t dead_pid, struct StanRoju *roj, int sem_id, int limit) {
    int idx = -1;

    // Lokalizacja slotu skojarzonego z danym PID
    P_mutex(sem_id, SEM_PAMIEC);
    for (int i = 0; i < limit; i++) {
        if (roj->drony[i].pid == dead_pid) { idx = i; break; }
    }

    if (idx == -1) { V_mutex(sem_id, SEM_PAMIEC); return; }

    int had_slot = roj->drony[idx].ma_slot_bazy;

    // Resetowanie danych w pamięci współdzielonej (SHM)
    roj->drony[idx].pid = 0;
    roj->drony[idx].stan = STAN_WOLNY;
    roj->drony[idx].ma_slot_bazy = 0;

    int splacono = 0;
    // Sprawdzenie długu platform przy odzyskiwaniu miejsca
    if (had_slot && roj->platformy_do_usuniecia > 0) {
        roj->platformy_do_usuniecia--;
        splacono = 1;
    }
    V_mutex(sem_id, SEM_PAMIEC);

    // Aktualizacja semafora bazy (tylko jeśli slot nie został przeznaczony na redukcję)
    if (had_slot) {
        if (!splacono) {
            struct sembuf op = {SEM_BAZA, 1, 0};
            semop(sem_id, &op, 1); // V(SEM_BAZA)
        }
    }
}


/*
 * HANDLER SYGNAŁU SIGINT (Ctrl+C)
 * Gwarantuje atomowe i bezpieczne wyłączenie symulacji.
 * Usuwa obiekty IPC i terminuje procesy potomne, zapobiegając powstawaniu procesów osieroconych.
*/
void sprzatanie(int sig) {
    // 1. Destrukcja segmentu pamięci współdzielonej
    if (g_shm_id != -1) {
        if(shmctl(g_shm_id, IPC_RMID, NULL) != -1) loguj("\n[OPERATOR] Pamięć dzielona usunięta.\n");
    } 

    // 2. Destrukcja zestawu semaforów
    if (g_sem_id != -1) {
        if(semctl(g_sem_id, 0, IPC_RMID) != -1) loguj("[OPERATOR] Semafory usunięte.\n");
    } 

    // 3. Destrukcja kolejki komunikatów
    if (g_msg_id != -1) {
        if(msgctl(g_msg_id, IPC_RMID, NULL) != -1) loguj("[OPERATOR] Kolejka komunikatów usunięta.\n");
    } 

    loguj(CZERWONY "[OPERATOR] KONIEC SYMULACJI" RESET "\n");
        
    // Masowa terminacja wszystkich procesów w grupie (Drony)
    kill(0, SIGKILL);
    exit(0);
}

int main(int argc, char *argv[]) {
    int n = DEFAULT_N;
    int baza = DEFAULT_POJEMNOSC_BAZY;

    if (argc == 3) {
        n = atoi(argv[1]);
        baza = atoi(argv[2]);

        // Walidacja parametrów wejściowych
        if (n > LIMIT_TECHNICZNY) {
            printf(CZERWONY "Błąd: Przekroczono limit techniczny (%d)!\n" RESET, LIMIT_TECHNICZNY);
            return 1;
        }

        // Minimalna dopuszczalna populacja roju
        if (n < 4) {
            printf(CZERWONY "Błąd: Minimalna liczba dronów to 4!\n" RESET);
            return 1;
        }

        // Weryfikacja spójności bazy względem populacji: P <= (n - 1) / 2
        int max_mozliwa_baza = (n - 1) / 2;
        
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

    // Przechwytywanie sygnału przerwania terminala
    signal(SIGINT, sprzatanie);

    // Generowanie unikalnych kluczy dostępowych IPC
    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_sem = ftok(FTOK_PATH, 'S');
    key_t klucz_msg = ftok(FTOK_PATH, 'Q');

    if (klucz_shm == -1 || klucz_sem == -1 || klucz_msg == -1) {
        perror("Błąd ftok");
        return 1;
    }

    // --- INICJALIZACJA ZASOBÓW IPC ---

    // Konfiguracja kolejki sterującej (Dowódca -> Operator)
    int msg_id = msgget(klucz_msg, 0666 | IPC_CREAT);
    if (msg_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        return 1;
    }
    g_msg_id = msg_id;

    // Konfiguracja segmentu pamięci (Główna tablica stanów)
    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666 | IPC_CREAT);
    if (shm_id == -1) {
        perror("Błąd tworzenia pamięci (shmget)");
        return 1;
    }
    g_shm_id = shm_id;

    // Konfiguracja zestawu semaforów synchronizacyjnych
    int sem_id = semget(klucz_sem, ILOSC_SEMAFOROW, 0666 | IPC_CREAT);
    if (sem_id == -1) {
        perror("Błąd tworzenia semaforów");
        return 1;
    }
    g_sem_id = sem_id;

    // Definiowanie stanów początkowych semaforów
    if (semctl(sem_id, SEM_BAZA, SETVAL, baza) == -1) perror("Błąd SEM_BAZA");
    if (semctl(sem_id, SEM_WEJSCIE_1, SETVAL, 1) == -1) perror("Błąd SEM_WEJSCIE_1");
    if (semctl(sem_id, SEM_WEJSCIE_2, SETVAL, 1) == -1) perror("Błąd SEM_WEJSCIE_2");
    if (semctl(sem_id, SEM_PAMIEC, SETVAL, 1) == -1) perror("Błąd SEM_PAMIEC");

    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);

    if (roj == (void*) -1) {
        perror("Błąd shmat");
        return 1;
    }

    // Alokacja logiki biznesowej w pamięci współdzielonej
    roj->pojemnosc_bazy = baza;
    roj->aktualny_limit_dronow = n;
    roj->max_limit_logiczny = n * 2;
    roj->platformy_do_usuniecia = 0;

    for (int i=0; i< n*2; i++) {
        roj->drony[i].stan = STAN_WOLNY;
    }

    loguj(ZIELONY "[OPERATOR] START SYSTEMU. Baza: %d | Drony %d/%d (Ctrl+C aby zakończyć)" RESET "\n", baza, n, n*2);

    int start_index = 0;

    // GŁÓWNA PĘTLA STERUJĄCA OPERATORA
    while(1) {  

        // Monitorowanie procesów potomnych (czyszczenie tablicy procesów zombie)
        int status;
        pid_t dead;
        while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
            odzyskaj_slot(dead, roj, sem_id, roj->aktualny_limit_dronow);
        }

        struct Komunikat msg;

        // Odbiór asynchronicznych komunikatów sterujących od Dowódcy
        while (msgrcv(msg_id, &msg, sizeof(int), 0, IPC_NOWAIT) != -1) {

            // --- ROZKAZ 1: ROZBUDOWA ROJU ---
            if (msg.mtype == TYP_DODAJ_PLATFORMY) {
                int stary_limit = roj->aktualny_limit_dronow;
                int nowy_limit = stary_limit * 2;

                if (nowy_limit > roj->max_limit_logiczny)
                    nowy_limit = roj->max_limit_logiczny;

                if (nowy_limit == stary_limit) {
                    loguj(ZOLTY "[SYGNAŁ 1] Rozbudowa: osiągnięto sufit %d (2*N_start)\n" RESET,
                        roj->max_limit_logiczny);
                    continue;
                }

                // Dynamiczne przeliczanie parametrów bazy
                int stara_pojemnosc = roj->pojemnosc_bazy;
                int nowa_pojemnosc  = (nowy_limit - 1) / 2;
                if (nowa_pojemnosc < 1) nowa_pojemnosc = 1;

                // Synchronizacja stanu semafora z aktualnym obłożeniem bazy
                int wolne_sem = semctl(sem_id, SEM_BAZA, GETVAL);
                if (wolne_sem < 0) wolne_sem = 0;
                if (wolne_sem > stara_pojemnosc) wolne_sem = stara_pojemnosc;

                int zajete = stara_pojemnosc - wolne_sem;
                if (zajete < 0) zajete = 0;

                // Wyznaczanie nowej puli wolnych miejsc
                int nowe_wolne = nowa_pojemnosc - zajete;
                if (nowe_wolne < 0) nowe_wolne = 0;

                roj->aktualny_limit_dronow = nowy_limit;
                roj->pojemnosc_bazy = nowa_pojemnosc;

                // Atomowa aktualizacja limitów semaforowych
                union semun arg;
                arg.val = nowe_wolne;
                semctl(sem_id, SEM_BAZA, SETVAL, arg);

                loguj(ZIELONY "[SYGNAŁ 1] Rozbudowa x2: Drony %d->%d | Baza %d->%d | wolne(sem)=%d zajete=%d\n" RESET,
                    stary_limit, nowy_limit, stara_pojemnosc, nowa_pojemnosc, nowe_wolne, zajete);
            }
            // --- ROZKAZ 2: REDUKCJA ROJU ---
            else if (msg.mtype == TYP_USUN_PLATFORMY) {
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

                // Terminacja nadmiarowych jednostek
                for (int i = nowy_limit; i < stary_limit; i++) {
                    if (roj->drony[i].stan != STAN_WOLNY) {
                        pid_t pid = roj->drony[i].pid;
                        if (pid > 0) {
                            loguj(CZERWONY "[SYGNAŁ 2] Zabijam nadmiarowego drona slot %d (PID=%d)\n" RESET, i, pid);
                            kill(pid, SIGTERM); 
                        }
                    }
                }

                /*
                 * Mechanizm redukcji platform:
                 * Próba natychmiastowego obniżenia wartości semafora (IPC_NOWAIT).
                 * Nadmiar zostaje zapisany jako dług, który dron spłaci przy wylocie.
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
                    P_mutex(sem_id, SEM_PAMIEC);
                    roj->platformy_do_usuniecia += reszta; // Inkrementacja licznika długu
                    V_mutex(sem_id, SEM_PAMIEC);
                    loguj("[SYGNAŁ 2] Zdemontowano od razu: %d platform. Czeka na demontaż: %d.\n", usuniete_natychmiast, reszta);
                } else {
                    loguj("[SYGNAŁ 2] Zdemontowano od razu wszystkie %d platform.\n", usuniete_natychmiast);
                }
            }
        }

        // --- ZARZĄDZANIE LICZEBNOŚCIĄ DRONÓW ---
        for (int k=0; k<roj->aktualny_limit_dronow; k++) {
            int i = (start_index + k) % roj->aktualny_limit_dronow; 

            // Uzupełnianie braków w populacji (forkowanie nowych jednostek)
            if (roj->drony[i].stan == STAN_WOLNY) {
                struct sembuf zajmij_miejsce = {SEM_BAZA, -1, IPC_NOWAIT};
                if(semop(sem_id, &zajmij_miejsce, 1) == 0) {
                    loguj("[OPERATOR] Wykryto brak drona na pozycji %d. Tworzę nowego...\n", i);

                    pid_t pid = fork();

                    if (pid == 0) {
                        // INICJACJA LOGIKI DRONA (Exec)
                        char bufor_id[10];
                        sprintf(bufor_id, "%d", i);
                        execl("./dron", "dron", bufor_id, NULL);
                        perror("Błąd execl");
                        exit(1);
                    } else if (pid > 0) {
                        // Rezerwacja slotu w SHM dla nowej jednostki
                        P_mutex(sem_id, SEM_PAMIEC);
                        roj->drony[i].pid = pid;
                        roj->drony[i].id_wewnetrzne = i;
                        roj->drony[i].bateria = 0;
                        roj->drony[i].liczba_cykli = 0;
                        roj->drony[i].stan = STAN_LADOWANIE;
                        roj->drony[i].ma_slot_bazy = 1;
                        V_mutex(sem_id, SEM_PAMIEC);

                        start_index = (i + 1) % roj->aktualny_limit_dronow;
                    }
                }
            }
        }
        usleep(50000); // Interwał cyklu pracy operatora
    }
    return 0;
}