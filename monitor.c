/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 *
 * Plik: monitor.c (Moduł Wizualizacji - High Performance)
 *
 * Opis działania:
 * - Działa jako niezależny proces "obserwatora".
 * - Podłącza się do istniejących zasobów IPC (Pamięć Dzielona) w trybie odczytu.
 * - Nie modyfikuje stanu roju, jedynie prezentuje dane.
 * - Odświeża widok z częstotliwością 10Hz, czyszcząc ekran kodami ANSI.
 * - Pozwala odciążyć proces Operatora od operacji wejścia/wyjścia (I/O).
 */

#include "common.h"

int main() {
    // 1. GENEROWANIE KLUCZY (Mszą być identyczne jak w Operatorze)
    key_t klucz_shm = ftok(FTOK_PATH, FTOK_ID);
    key_t klucz_sem = ftok(FTOK_PATH, 'S');
    
    // 2. POBRANIE ID ZASOBÓW
    // Zwróć uwagę na brak flagi IPC_CREAT - monitor nie tworzy zasobów, tylko się podłącza.
    int shm_id = shmget(klucz_shm, sizeof(struct StanRoju), 0666);
    int sem_id = semget(klucz_sem, ILOSC_SEMAFOROW, 0666);

    if (shm_id == -1 || sem_id == -1) {
        printf(CZERWONY "BŁĄD: Nie mogę znaleźć Operatora! Uruchom go najpierw.\n" RESET);
        return 1;
    }

    // 3. DOŁĄCZENIE PAMIĘCI DZIELONEJ
    struct StanRoju *roj = (struct StanRoju*) shmat(shm_id, NULL, 0);
    if (roj == (void*)-1) {
        perror("Błąd shmat");
        return 1;
    }

    // 4. GŁÓWNA PĘTLA WIZUALIZACJI
    while(1) {
        // Czyszczenie ekranu za pomocą kodów ucieczki ANSI (działa w Linux/Unix)
        // \033[H - ustaw kursor w lewym górnym rogu
        // \033[J - wyczyść ekran od kursora do końca
        printf("\033[H\033[J");

        printf(ZIELONY "=== SYSTEM MONITORINGU ROJU (Odświeżanie 10Hz) ===\n" RESET);
        printf("--------------------------------------------------\n");
        
        // Wyświetlanie kluczowych statystyk pobranych bezpośrednio z pamięci współdzielonej
        printf(" POJEMNOŚĆ BAZY:      %d\n", roj->pojemnosc_bazy);
        printf(" AKTYWNE DRONY:       %d\n", roj->aktywne_drony); // Nowe pole z common.h
        printf(" LIMIT LOGICZNY:      %d / %d\n", roj->aktualny_limit_dronow, roj->max_limit_logiczny);
        
        printf("--------------------------------------------------\n");
        printf(" [Aby wysłać rozkaz, użyj okna DOWÓDCY]\n");
        printf(" [Ctrl+C aby zamknąć podgląd]\n");

        // Oczekiwanie 100ms (100 000 mikrosekund) -> 10 klatek na sekundę
        usleep(100000); 
    }

    // Odłączenie pamięci (teoretycznie nieosiągalne przy pętli while(1), ale dobra praktyka)
    shmdt(roj);
    return 0;
}