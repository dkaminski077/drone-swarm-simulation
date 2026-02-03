/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 * * Plik: dowodca.c (Interfejs Taktyczny)
 * * Opis działania:
 * - Stanowi centrum sterowania systemem z perspektywy użytkownika.
 * - Komunikuje się z Operatorem za pomocą Kolejek Komunikatów (Message Queues).
 * - Pozwala na dynamiczne skalowanie populacji roju oraz bazy (Rozbudowa/Redukcja).
 * - Implementuje mechanizm wskazywania celów do ataków samobójczych (sygnał SIGUSR1).
*/

#include "common.h"

/* * FUNKCJA: wyczysc_stdin
 * Opis: Usuwa zbędne znaki z bufora wejściowego, zapobiegając błędnemu 
 * przetwarzaniu menu w przypadku wprowadzenia nieprawidłowych danych.
*/
static void wyczysc_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

int main() {
    // Inicjalizacja ziarna losowości unikalnego dla sesji dowódcy
    srand(time(NULL) ^ getpid());

    // Pobieranie identyfikatorów zasobów IPC (Pamięć dzielona i Kolejka)
    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_msg = ftok(FTOK_PATH, 'Q');

    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666);
    int msg_id = msgget(klucz_msg, 0666);

    if (shm_id == -1 || msg_id == -1) {
        printf("BŁĄD: Nie mogę połączyć się z Rojem.\n");
        return 1;
    }

    // Dołączenie pamięci współdzielonej (odczyt stanu roju w czasie rzeczywistym)
    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);
    if (roj == (void*) -1) {
        perror("Błąd shmat");
        return 1;
    }

    while (1) {
        printf("\n--- DOWÓDCA ---\n");
        printf(" [1] ROZBUDOWA (komunikat)\n");
        printf(" [2] REDUKCJA  (komunikat)\n");
        printf(" [a] ATAK SAMOBÓJCZY (SIGUSR1 do losowego drona)\n");
        printf(" [q] WYJŚCIE\n");
        printf(" > ");

        char wybor;
        if (scanf(" %c", &wybor) != 1) {
            wyczysc_stdin();
            continue;
        }

        // --- OPCJA 1: ROZBUDOWA ROJU ---
        if (wybor == '1') {
            struct Komunikat msg = { .mtype = TYP_DODAJ_PLATFORMY };
            if (msgsnd(msg_id, &msg, sizeof(int), IPC_NOWAIT) == -1) {
                perror("msgsnd rozbudowa");
            } else {
                printf("[DOWÓDCA] Wysłano rozkaz rozbudowy.\n");
            }
        }
        // --- OPCJA 2: REDUKCJA ROJU ---
        else if (wybor == '2') {
            struct Komunikat msg = { .mtype = TYP_USUN_PLATFORMY };
            if (msgsnd(msg_id, &msg, sizeof(int), IPC_NOWAIT) == -1) {
                perror("msgsnd redukcja");
            } else {
                printf("[DOWÓDCA] Wysłano rozkaz redukcji.\n");
            }
        }
        // --- OPCJA a: INICJACJA ATAKU ---
        else if (wybor == 'a' || wybor == 'A') {
            int kandydaci[MAX_DRONOW];
            int licznik = 0;

            /* * ANALIZA STANU ROJU:
             * Przeszukiwanie tablicy w poszukiwaniu dronów w stanach aktywnych.
             */
            for (int i = 0; i < roj->max_limit_logiczny; i++) {
                int stan = roj->drony[i].stan;
                if (stan == STAN_LOT || stan == STAN_POWROT || stan == STAN_LADOWANIE) {
                    kandydaci[licznik++] = i;
                }
            }

            if (licznik == 0) {
                printf("[DOWÓDCA] Brak dostępnych celów.\n");
            } else {
                // Losowy wybór celu spośród aktywnych jednostek
                int idx = kandydaci[rand() % licznik];

                pid_t pid = roj->drony[idx].pid;
                int id_wew = roj->drony[idx].id_wewnetrzne;

                if (pid <= 0) {
                    printf("[DOWÓDCA] Wylosowano slot %d, ale PID niepoprawny (%d).\n", id_wew, pid);
                } else {
                    printf("[DOWÓDCA] ATAK -> dron %d (PID=%d)\n", id_wew, pid);
                    // Przesłanie sygnału SIGUSR1 bezpośrednio do procesu drona
                    if (kill(pid, SIGUSR1) == -1) {
                        perror("kill(SIGUSR1)");
                    }
                }
            }
        }
        // --- OPCJA q: WYJŚCIE ---
        else if (wybor == 'q' || wybor == 'Q') {
            break;
        }
    }

    // Odłączenie pamięci współdzielonej przed zakończeniem pracy interfejsu
    shmdt(roj);
    return 0;
}