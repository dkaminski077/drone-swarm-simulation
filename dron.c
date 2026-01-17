/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 * 
 * Plik: dron.c (Proces Potomny)
 * 
 * Opis działania:
 * - Symuluje cykl życia drona: Ładowanie -> Start -> Lot -> Powrót -> Ładowanie.
 * - Synchronizuje dostęp do zasobów za pomocą Semaforów.
 * - Obsługuje sygnał SIGUSR1 (Atak Samobójczy) wysłany przez Dowódcę.
 * - Fizycznie realizuje niszczenie platform (spłacanie długu) przy wylocie z bazy.
 * - Loguje stany i błedy do pliku. 
*/

#include "common.h"

struct StanRoju *g_roj = NULL;
int g_sem_id = -1;
int g_id_drona = -1;

/* FLAGA SYGNAŁOWA (Zabezpieczenie przed Deadlockiem)
 * Używamy typu volatile sig_atomic_t, aby zapewnić atomowość zapisu w handlerze.
 * Dzięki temu nie musimy używać semaforów wewnątrz funkcji obsługi sygnału.
 */
volatile sig_atomic_t g_atak_otrzymany = 0;

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
 * HANDLER SYGNAŁU ATAKU (SIGUSR1)
 * Funkcja wywoływana asynchronicznie przez system.
 * Jedynie ustawia flagę - cała logika wykonywana jest bezpiecznie w pętli głównej.
*/
void atak(int sig) {
    g_atak_otrzymany = 1;
}

/*
 * FUNKCJA REALIZUJĄCA ATAK
 * Wywoływana w pętli głównej w bezpiecznych momentach.
 * Zwraca 1, jeśli dron zginął (należy zakończyć proces), 0 w przeciwnym razie.
 */
int obsluz_atak(int id_wew, int sem_id, struct StanRoju *roj) {
    if (!g_atak_otrzymany) return 0;
    
    int bateria = roj->drony[id_wew].bateria;
    int stan = roj->drony[id_wew].stan;
    
    loguj(CZERWONY "    [DRON %d] !!! OTRZYMAŁEM ROZKAZ ATAKU SAMOBÓJCZEGO !!! (Bateria: %d%%)." RESET "\n", id_wew, bateria);

    // Dron ignoruje atak, jeśli ma za mało energii
    if (bateria < 20) {
        loguj(ZOLTY "    [DRON %d] Atak anulowany - zbyt słaba bateria (<20%%)." RESET "\n", id_wew);
        g_atak_otrzymany = 0;  // Reset flagi
        return 0;  // Kontynuuj misję
    }

    loguj(CZERWONY "    [DRON %d] !!! ATAK WYKONANY !!!" RESET "\n", id_wew);

    // Aktualizacja stanu i zwolnienie zasobów
    P(sem_id, SEM_PAMIEC);
    roj->drony[id_wew].stan = STAN_WOLNY;
    V(sem_id, SEM_PAMIEC);

    // Jeśli był w bazie, musi zwolnić miejsce
    if(stan == STAN_LADOWANIE) {
        V(sem_id, SEM_BAZA);
    }

    return 1;  // Atak wykonany - zakończ proces
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        loguj("Błąd: Dron musi być uruchomiony przez Operatora (brak ID)!\n");
        return 1;
    }
    int id_wew = atoi(argv[1]);

    g_id_drona = id_wew;
    g_atak_otrzymany = 0;


    // Inicjalizacja generatora liczb losowych (nikalna dla procesu)
    srand(time(NULL) ^ getpid());

    // Pobranie kluczy i dołączenie do zasobów
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

    // Rejestracja sygnału ataku
    signal(SIGUSR1, atak);

    loguj(ZIELONY "    [DRON %d] PID: %d. Uruchomiono. Bateria 0%%." RESET "\n", id_wew, getpid());

    // GŁOWNA PĘTLA ŻYCIA DRONA
    while(1) {
        unsigned int ladowanie = 2;
        while (ladowanie > 0) {
            if (obsluz_atak(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }
            ladowanie = sleep(ladowanie);
        }

        // Reset baterii
        P(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].bateria = 100;
        roj->drony[id_wew].liczba_cykli++;
        int cykle = roj->drony[id_wew].liczba_cykli;
        V(sem_id, SEM_PAMIEC);

        // Sprawdzenie zużycia
        if (cykle > MAX_CYKLI) {
            loguj(ZOLTY "    [DRON %d] Limit cykli osiągnięty (cykle: %d). Złomowanie." RESET "\n", id_wew, cykle);
            break;
        }

        loguj(ZIELONY "    [DRON %d] Bateria naładowana (100%%). Czekam na wylot." RESET "\n", id_wew);

        if (obsluz_atak(id_wew, sem_id, roj)) {
            shmdt(roj);
            exit(0);
        }

        // Losowanie bramki wylotowej
        int bramka = (rand()%2) + SEM_WEJSCIE_1;

        // Wylot
        P(sem_id, bramka);
        loguj("    [DRON %d] Wylot bramką %d...\n", id_wew, bramka - SEM_WEJSCIE_1 +1);
        sleep(1);
        V(sem_id, bramka);

        // OBSŁUGA SPŁACANIA DŁUGU
        P(sem_id, SEM_PAMIEC);
        int zniszcz_platforme = 0;
        if (roj->platformy_do_usuniecia > 0) {
            roj->platformy_do_usuniecia--;  // Zmniejszenie długu
            zniszcz_platforme = 1;
        }
        V(sem_id, SEM_PAMIEC);

        if (zniszcz_platforme) {
            // Zamiast oddać semafor (V), niszczymy go (nie wywołujemy V)
            loguj(CZERWONY "    [DRON %d] Demontaż platformy (Redukcja)." RESET "\n", id_wew);
        } else {
            // Standardowe zwolnienie miejsca
            V(sem_id, SEM_BAZA);
        }
        // -----------------------------------------
        
        // Zmiana stanu na LOT
        P(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_LOT;
        V(sem_id, SEM_PAMIEC);

        loguj("    [DRON %d] Lot w strefie operacyjnej...\n", id_wew);

        // Symulacja lotu i zużycia baterii
        while(1) {
            if (obsluz_atak(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }

            sleep(1);

            P(sem_id, SEM_PAMIEC);
            roj->drony[id_wew].bateria -= 15;
            int poziom = roj->drony[id_wew].bateria;
            V(sem_id, SEM_PAMIEC);

            // Powrót do bazy przy niskim stanie baterii
            if (poziom <= BAT_CRITICAL) {
                loguj(ZOLTY "    [DRON %d] Bateria słaba (%d%%). Powrót do bazy." RESET "\n", id_wew, poziom);
                break;
            }

            // Awaria (rozbicie)
            if (poziom <= 0) {
                loguj(CZERWONY "    [DRON %d] Bateria 0%%. Rozbity w locie." RESET "\n", id_wew);
                P(sem_id, SEM_PAMIEC);
                roj->drony[id_wew].stan = STAN_WOLNY;
                V(sem_id, SEM_PAMIEC);
                exit(0);
            }
        }

        // Procedura powrotu
        P(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_POWROT;
        V(sem_id, SEM_PAMIEC);

        loguj("    [DRON %d] Zbliżam się do bazy...\n", id_wew);

        // Oczekiwanie na wolne miejsce w bazie
        while(1) {
            if (obsluz_atak(id_wew, sem_id, roj)) {
                shmdt(roj);
                exit(0);
            }

            struct sembuf wejdz = {SEM_BAZA, -1, IPC_NOWAIT};

            if (semop(sem_id, &wejdz, 1) == 0) {
                break;  // Udało się wejść
            } else {
                // Czekanie w kolejce (zużywa baterię)
                P(sem_id, SEM_PAMIEC);
                roj->drony[id_wew].bateria -= 10;
                int poziom = roj->drony[id_wew].bateria;
                V(sem_id, SEM_PAMIEC);

                loguj(ZOLTY "    [DRON %d] Baza pełna. Oczekiwanie..." RESET "\n", id_wew);

                if (poziom <=0 ) {
                    loguj(CZERWONY "    [DRON %d] Bateria 0%%. Rozbity w kolejce." RESET "\n", id_wew);
                    P(sem_id, SEM_PAMIEC);
                    roj->drony[id_wew].stan = STAN_WOLNY;
                    V(sem_id, SEM_PAMIEC);
                    exit(0);
                }
                sleep(1);
            }
        }

        // Lądowanie
        bramka = (rand()%2) + SEM_WEJSCIE_1;
        P(sem_id, bramka);
        loguj("    [DRON %d] Ląduję bramką %d...\n", id_wew, bramka - SEM_WEJSCIE_1 + 1);
        sleep(1);
        V(sem_id, bramka);

        // Zmiana stanu na ładowanie
        P(sem_id, SEM_PAMIEC);
        roj->drony[id_wew].stan = STAN_LADOWANIE;
        V(sem_id, SEM_PAMIEC);
    }

    // Zakończenie pracy (naturalne zużycie)
    P(sem_id, SEM_PAMIEC);
    roj->drony[id_wew].stan = STAN_WOLNY;
    V(sem_id, SEM_PAMIEC);

    V(sem_id, SEM_BAZA);    // Zwalnianie miejsca

    loguj(CZERWONY "    [DRON %d] Złomowanie zakończone." RESET "\n", id_wew);

    shmdt(roj);
    return 0;
}