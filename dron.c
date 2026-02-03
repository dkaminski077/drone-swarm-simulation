/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 * * Plik: dron.c (Proces Jednostki Wykonawczej)
 * * Opis działania:
 * - Implementuje pełną maszynę stanów drona: Ładowanie -> Start -> Lot -> Powrót.
 * - Wykorzystuje mechanizmy IPC System V (Semafory, Pamięć Współdzielona) do synchronizacji.
 * - Obsługuje asynchroniczne przerwania: SIGUSR1 (Atak) oraz SIGTERM (Redukcja/Koniec).
 * - Realizuje procedurę "spłaty długu" (platformy_do_usuniecia) zapewniając spójność 
 * liczników bazy podczas dynamicznego skalowania roju przez Operatora.
 */

#include "common.h"

struct StanRoju *g_roj = NULL;
int g_sem_id = -1;
int g_id_drona = -1;

/* FLAGI SYGNAŁOWE (Bezpieczeństwo wielowątkowe)
 * volatile sig_atomic_t gwarantuje atomowy dostęp do zmiennych modyfikowanych 
 * wewnątrz programowych procedur obsługi sygnałów (handlerów).
 */
volatile sig_atomic_t g_atak_otrzymany = 0;
volatile sig_atomic_t g_zabij = 0;

/* --- OPERACJE MUTEXOWE (Z FLAGĄ SEM_UNDO) --- */
static void P_mutex(int sem_id, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, -1, SEM_UNDO };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue; // Ponów próbę jeśli przerwano sygnałem
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

/* --- OPERACJE LICZNIKOWE (BEZ SEM_UNDO) --- 
 * Stosowane dla semafora SEM_BAZA, aby stan zasobów pozostał trwały 
 * nawet po zakończeniu procesu drona.
 */
static void P_cnt_nowait(int sem_id, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, -1, IPC_NOWAIT };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        break;
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

/* --- HANDLERY SYGNAŁÓW --- */
void atak(int sig) {
    g_atak_otrzymany = 1; // Rejestracja rozkazu ataku
}

void koniec(int sig) {
    g_zabij = 1; // Rejestracja prośby o wygaszenie procesu
}

/*
 * FUNKCJA: obsluz_atak
 * Realizuje procedurę ataku samobójczego. Zwalnia sloty bazy z uwzględnieniem
 * ewentualnego długu demontażu platform.
 */
int obsluz_atak(int id_wew, int sem_id, struct StanRoju *roj) {
    if (!g_atak_otrzymany) return 0;
    
    int bateria = roj->drony[id_wew].bateria;
    int stan = roj->drony[id_wew].stan;
    
    loguj(CZERWONY "    [DRON %d] !!! OTRZYMAŁEM ROZKAZ ATAKU SAMOBÓJCZEGO !!! (Bateria: %d%%)." RESET "\n", id_wew, bateria);

    // Weryfikacja zdolności energetycznej do wykonania ataku
    if (bateria < 20) {
        loguj(ZOLTY "    [DRON %d] Atak anulowany - zbyt słaba bateria (<20%%)." RESET "\n", id_wew);
        g_atak_otrzymany = 0;  
        return 0;  
    }

    loguj(CZERWONY "    [DRON %d] !!! ATAK WYKONANY !!!" RESET "\n", id_wew);

    // Atomowa aktualizacja stanu w pamięci współdzielonej
    P_mutex(sem_id, SEM_PAMIEC);
    int ma_slot = roj->drony[id_wew].ma_slot_bazy;
    roj->drony[id_wew].stan = STAN_WOLNY;
    V_mutex(sem_id, SEM_PAMIEC);

    if (ma_slot) {
        int splacono = 0;

        P_mutex(sem_id, SEM_PAMIEC);
        // Sprawdzenie czy należy fizycznie zredukować platformę w bazie
        if (roj->platformy_do_usuniecia > 0) {
            roj->platformy_do_usuniecia--;
            splacono = 1;
        }
        roj->drony[id_wew].ma_slot_bazy = 0;
        V_mutex(sem_id, SEM_PAMIEC);

        if (splacono) {
            loguj(CZERWONY "    [DRON %d] ATAK: spłacam dług demontażu (bez V na SEM_BAZA).\n" RESET, id_wew);
        } else {
            V_cnt(sem_id, SEM_BAZA); // Tradycyjne zwolnienie miejsca
            loguj(ZOLTY "    [DRON %d] ATAK: zwalniam miejsce w bazie (V na SEM_BAZA).\n" RESET, id_wew);
        }
    }

    return 1;
}

/*
 * FUNKCJA: obsluz_koniec
 * Czyści zasoby i kończy proces w odpowiedzi na sygnał SIGTERM.
 */
int obsluz_koniec(int id_wew, int sem_id, struct StanRoju *roj) {
    if (!g_zabij) return 0;

    loguj(CZERWONY "    [DRON %d] SIGTERM (redukcja). Sprzątam i kończę.\n" RESET, id_wew);

    P_mutex(sem_id, SEM_PAMIEC);
    int ma_slot = roj->drony[id_wew].ma_slot_bazy;
    roj->drony[id_wew].stan = STAN_WOLNY;
    V_mutex(sem_id, SEM_PAMIEC);

    if (ma_slot) {
        int splacono = 0;

        P_mutex(sem_id, SEM_PAMIEC);
        if (roj->platformy_do_usuniecia > 0) {
            roj->platformy_do_usuniecia--;
            splacono = 1;
        }
        roj->drony[id_wew].ma_slot_bazy = 0;
        V_mutex(sem_id, SEM_PAMIEC);

        if (splacono) {
            loguj(CZERWONY "    [DRON %d] SIGTERM: spłacam dług demontażu (bez V na SEM_BAZA).\n" RESET, id_wew);
        } else {
            V_cnt(sem_id, SEM_BAZA);
            loguj(ZOLTY "    [DRON %d] SIGTERM: zwalniam miejsce w bazie (V na SEM_BAZA).\n" RESET, id_wew);
        }
    }

    return 1;
}



int main(int argc, char *argv[]) {
    if (argc < 2) {
        loguj("Błąd: Dron musi być uruchomiony przez Operatora (brak ID)!\n");
        return 1;
    }
    int id_wew = atoi(argv[1]);

    g_id_drona = id_wew;
    g_atak_otrzymany = 0;

    // Inicjalizacja ziarna losowości unikalnego dla każdego procesu
    srand(time(NULL) ^ getpid());

    // Pobieranie identyfikatorów zasobów IPC
    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_sem = ftok(FTOK_PATH, 'S');

    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666);
    int sem_id = semget(klucz_sem, ILOSC_SEMAFOROW, 0666);

    if (shm_id == -1 || sem_id == -1) {
        perror("Błąd podłączenia drona do zasobów");
        return 1;
    }

    // Dołączenie pamięci współdzielonej do przestrzeni adresowej procesu
    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);
    if (roj == (void*) -1) {
        perror("Błąd shmat w dronie");
        return 1;
    }

    g_roj = roj;
    g_sem_id = sem_id;

    // Rejestracja procedur obsługi sygnałów
    signal(SIGUSR1, atak);
    signal(SIGTERM, koniec);

    loguj(ZIELONY "    [DRON %d] PID: %d. Uruchomiono. Bateria 0%%." RESET "\n", id_wew, getpid());

    // Oznaczenie slotu bazy jako zajętego podczas startu
    P_mutex(sem_id, SEM_PAMIEC);
    roj->drony[id_wew].ma_slot_bazy = 1;
    V_mutex(sem_id, SEM_PAMIEC);

    /* --- GŁÓWNA PĘTLA MASZYNY STANÓW --- */
    while(1) {
        // ETAP: ŁADOWANIE
        unsigned int ladowanie = CZAS_LADOWANIA;
        while (ladowanie > 0) {
            if (obsluz_atak(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }
            if (obsluz_koniec(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }
            ladowanie = sleep(ladowanie);
        }

        // Aktualizacja statystyk i reset poziomu energii
        P_mutex(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].bateria = BATERIA_PELNA;
        roj->drony[id_wew].liczba_cykli++;
        int cykle = roj->drony[id_wew].liczba_cykli;
        V_mutex(sem_id, SEM_PAMIEC);

        // Kontrola naturalnego zużycia eksploatacyjnego
        if (cykle > MAX_CYKLI) {
            loguj(ZOLTY "    [DRON %d] Limit cykli osiągnięty (cykle: %d). Złomowanie." RESET "\n", id_wew, cykle);
            break;
        }

        loguj(ZIELONY "    [DRON %d] Bateria naładowana (100%%). Czekam na wylot." RESET "\n", id_wew);

        if (obsluz_atak(id_wew, sem_id, roj)) {
            shmdt(roj);
            exit(0);
        }
        if (obsluz_koniec(id_wew, sem_id, roj)) {
            shmdt(roj);
            exit(0);
        }

        // Procedura wylotu przez bramki (synchronizacja mutexem)
        int bramka = (rand()%2) + SEM_WEJSCIE_1;

        P_mutex(sem_id, bramka);
        loguj("    [DRON %d] Wylot bramką %d...\n", id_wew, bramka - SEM_WEJSCIE_1 +1);
        usleep(10000);
        V_mutex(sem_id, bramka);

        // OBSŁUGA REDUKCJI BAZY (Mechanizm spłaty długu przy opuszczaniu slotu)
        P_mutex(sem_id, SEM_PAMIEC);
        int zniszcz_platforme = 0;
        if (roj->platformy_do_usuniecia > 0) {
            roj->platformy_do_usuniecia--; 
            zniszcz_platforme = 1;
        }
        V_mutex(sem_id, SEM_PAMIEC);

        if (zniszcz_platforme) {
            loguj(CZERWONY "    [DRON %d] Demontaż platformy (Redukcja)." RESET "\n", id_wew);
        } else {
            V_cnt(sem_id, SEM_BAZA); // Tradycyjne zwolnienie miejsca
        }

        // Aktualizacja stanów po opuszczeniu bazy
        P_mutex(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].ma_slot_bazy = 0;
        V_mutex(sem_id, SEM_PAMIEC);
        
        P_mutex(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_LOT;
        V_mutex(sem_id, SEM_PAMIEC);

        loguj("    [DRON %d] Lot w strefie operacyjnej...\n", id_wew);

        // ETAP: SYMULACJA LOTU
        while(1) {
            if (obsluz_atak(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }
            if (obsluz_koniec(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }

            sleep(CZAS_LOTU);

            // Zużycie energii w trakcie misji
            P_mutex(sem_id, SEM_PAMIEC);
            roj->drony[id_wew].bateria -= KOSZT_LOTU;
            int poziom = roj->drony[id_wew].bateria;
            V_mutex(sem_id, SEM_PAMIEC);

            // Powrót przy stanie krytycznym baterii
            if (poziom <= BAT_CRITICAL) {
                loguj(ZOLTY "    [DRON %d] Bateria słaba (%d%%). Powrót do bazy." RESET "\n", id_wew, poziom);
                break;
            }

            // Obsługa awaryjnego rozbicia (brak energii)
            if (poziom <= 0) {
                loguj(CZERWONY "    [DRON %d] Bateria 0%%. Rozbity w locie." RESET "\n", id_wew);
                P_mutex(sem_id, SEM_PAMIEC);
                roj->drony[id_wew].stan = STAN_WOLNY;
                V_mutex(sem_id, SEM_PAMIEC);
                exit(0);
            }
        }

        // ETAP: POWRÓT I KOLEJKOWANIE
        P_mutex(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_POWROT;
        V_mutex(sem_id, SEM_PAMIEC);

        loguj("    [DRON %d] Zbliżam się do bazy...\n", id_wew);

        // Próba rezerwacji miejsca w bazie
        while(1) {
            if (obsluz_atak(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }
            if (obsluz_koniec(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }

            struct sembuf wejdz = {SEM_BAZA, -1, IPC_NOWAIT};

            if (semop(sem_id, &wejdz, 1) == 0) {
                P_mutex(sem_id, SEM_PAMIEC);
                roj->drony[id_wew].ma_slot_bazy = 1;
                V_mutex(sem_id, SEM_PAMIEC);
                break;
            } else {
                // Zawis i oczekiwanie na wolny slot (zużycie energii)
                P_mutex(sem_id, SEM_PAMIEC);
                roj->drony[id_wew].bateria -= KOSZT_CZEKANIA;
                int poziom = roj->drony[id_wew].bateria;
                V_mutex(sem_id, SEM_PAMIEC);

                loguj(ZOLTY "    [DRON %d] Baza pełna. Oczekiwanie..." RESET "\n", id_wew);

                if (poziom <=0 ) {
                    loguj(CZERWONY "    [DRON %d] Bateria 0%%. Rozbity w kolejce." RESET "\n", id_wew);
                    P_mutex(sem_id, SEM_PAMIEC);
                    roj->drony[id_wew].stan = STAN_WOLNY;
                    V_mutex(sem_id, SEM_PAMIEC);
                    exit(0);
                }
                sleep(1);
            }
        }

        // ETAP: LĄDOWANIE PRZEZ BRAMKĘ
        bramka = (rand()%2) + SEM_WEJSCIE_1;
        P_mutex(sem_id, bramka);
        loguj("    [DRON %d] Ląduję bramką %d...\n", id_wew, bramka - SEM_WEJSCIE_1 + 1);
        usleep(10000);
        V_mutex(sem_id, bramka);

        P_mutex(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_LADOWANIE;
        V_mutex(sem_id, SEM_PAMIEC);
    }

    /* PROCEDURA WYCOFANIA JEDNOSTKI (Naturalne zakończenie pracy) */
    P_mutex(sem_id, SEM_PAMIEC);
    roj->drony[id_wew].pid = 0;
    roj->drony[id_wew].stan = STAN_WOLNY;
    V_mutex(sem_id, SEM_PAMIEC);

    P_mutex(sem_id, SEM_PAMIEC);
    int ma_slot = roj->drony[id_wew].ma_slot_bazy;
    roj->drony[id_wew].ma_slot_bazy = 0;
    V_mutex(sem_id, SEM_PAMIEC);

    // Zwolnienie zasobów bazy przy zakończeniu procesu
    if (ma_slot) V_cnt(sem_id, SEM_BAZA);

    loguj(CZERWONY "    [DRON %d] Złomowanie zakończone." RESET "\n", id_wew);

    shmdt(roj); // Odłączenie pamięci współdzielonej
    return 0;
}