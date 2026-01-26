/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 * 
 * Plik: dowodca.c (Interfejs Użytkownika)
 * 
 * Opis działania:
 * - Umożliwia interakcję użytkownika z systemem.
 * - Wysyła rozkazy (Rozbudowa/Redukcja) do Operatora za pomocą Kolejki Komuniktaów.
 * - Monitoruje stan roju czytając z Pamięci Dzielonej.
 * - Wysyła sygnał ataku (SIGUSR1) bezpośrednio do Dronów.
*/

#include "common.h"

int main() {
    // Generator liczb losowych dla losowania celu ataku
    srand(time(NULL) ^ getpid());

    // Generowanie kluczy IPC
    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_sem = ftok(FTOK_PATH, 'S');
    key_t klucz_msg = ftok(FTOK_PATH, 'Q');

    // Uzyskanie dostępu do zasobów
    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666);
    int sem_id = semget(klucz_sem, ILOSC_SEMAFOROW, 0666);
    int msg_id = msgget(klucz_msg, 0666);

    if (shm_id == -1 || sem_id == -1 || msg_id == -1) {
        printf("BŁĄD: Nie mogę połączyć się z Rojem.\n");
        return 1;
    }

    // Dołączenie pamięci dzielonej (tylko do odczytu)
    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);

    if (roj == (void*) -1) {
        perror("Błąd shmat");
        return 1;
    }

    char wybor;

    // GŁOWNA PĘTLA INTERFEJSU
    while(1) {
        // Wyświetlanie menu i aktualnego stanu roju
        printf("\n-----------------------------------\n");
        printf("STATUS: Baza: %d miejsc | Limit dronów: %d/%d\n", roj->pojemnosc_bazy, 
        roj->aktualny_limit_dronow, roj->max_limit_logiczny);
        printf("\n-----------------------------------\n");
        printf(" [1] ROZBUDOWA: Dodaj platformy.\n");
        printf(" [2] REDUKCJA: Zdemontuj 50%% platfrom.\n");
        printf(" [a] ATAK SAMOBÓJCZY: Wyślij sygnał zniszczenia.\n");
        printf(" [q] WYJŚCIE\n");
        printf(" >");

        scanf(" %c", &wybor);

        // --- OPCJA 1: ROZBUDOWA (Komunikat do Operatora) ---
        if (wybor == '1') {
            struct Komunikat msg;
            msg.mtype = TYP_DODAJ_PLATFORMY;
            if(msgsnd(msg_id, &msg, sizeof(int), 0) == -1) {
                perror("Błąd wysyłania komunikatu.\n");
            } else {
                printf("[DOWÓDCA] Wysłano rozkaz rozbudowy.\n");
                sleep(2);
            }
        } 
        // --- OPCJA 2: REDUKCJA (Komunikat do Operatora) ---
        else if (wybor == '2') {
            struct Komunikat msg;
            msg.mtype = TYP_USUN_PLATFORMY;
            if(msgsnd(msg_id, &msg, sizeof(int), 0) == -1) {
                perror("Błąd wysyłania komunikatu.\n");
            } else {
                printf("[DOWÓDCA] Wysłano rozkaz redukcji.\n");
                sleep(2);
            }
        } 
        // --- OPCJA A: ATAK SAMOBÓJCZY (SYGNAŁ DO DRONA) ---
        else if (wybor == 'a' || wybor == 'A') {
            printf("[DOWÓDCA] Szukam celu do ataku samobójczego...\n");

            /*
             * Algorytm wyboru celu:
             * 1. Skanowanie tablicy dronów w pamięci dzielonej.
             * 2. Wyszukiwanie wszystkich aktywnych dronów (Lot/Powrót/Ładowanie).
             * 3. Wylosowanie jednego drona z listy kandydatów.
            */
            int kadnydaci[MAX_DRONOW];
            int licznik_kandydatow = 0;
            for (int i=0; i<roj->max_limit_logiczny; i++) {
                int stan = roj->drony[i].stan;

                if(stan == STAN_LOT || stan == STAN_POWROT || stan == STAN_LADOWANIE) {
                    kadnydaci[licznik_kandydatow] = i;
                    licznik_kandydatow++;
                }
            }

            if (licznik_kandydatow > 0) {
                int wylosowany_index = rand() % licznik_kandydatow;
                int cel_index = kadnydaci[wylosowany_index];

                // Pobieranie danych celu
                pid_t cel_pid = roj->drony[cel_index].pid;
                int cel_id_wew = roj->drony[cel_index].id_wewnetrzne;
                int cel_bateria = roj->drony[cel_index].bateria;
                int cel_stan = roj->drony[cel_index].stan;

                printf("[DOWÓDCA] Namierzono %d celów. Wylosowano drona nr %d.\n ATAK!\n", licznik_kandydatow, cel_id_wew);

                // Wysłanie sygnału SIGUSR1 do procesu wylosowanwego drona
                kill(cel_pid, SIGUSR1);

                sleep(2);
            } else {
                printf("[DOWÓDCA] Brak dostępnych celów,\n");
            }
        } 
        // --- OPCJA Q: WYJŚCIE ---
        else if (wybor == 'q' || wybor == 'Q') {
            break;
        }
    }

    // Odłączenie pamięci dzielonej
    shmdt(roj);
    return 0;
}