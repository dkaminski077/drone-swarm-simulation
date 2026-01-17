/*
 * Temat: Rój Dronów
 * Autor: Dawid Kamiński (155272)
 * 
 * Plik: common.h (Konfiguracja i Biblioteki)
 * 
 * Opis działania:
 * - Zawiera definicje struktur IPC (StanRoju, Dron, Komunikat).
 * - Definiuje stałe systemowe.
 * - Implementuje funkcję 'zapisz_do_pliku'.
*/

#ifndef COMMON_H
#define COMMON_H

// Biblioteki systemowe wymagane do IPC, obsługi plików i procesów
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

// --- KODY KOLORÓw ---
#define RESET   "\033[0m"
#define CZERWONY "\033[1;31m"
#define ZIELONY  "\033[1;32m"
#define ZOLTY    "\033[1;33m"

// --- PARAMETRY SYMULACJI ---
#define N 12                // Początkowa liczba dronów
#define MAX_DRONOW (N*2)    // Maksymalna liczba dronów
#define POJEMNOSC_BAZY 4    // Początkowa liczba miejsc w bazie
#define BAT_CRITICAL 20     // Poziom baterii wymuszający powrót do bazy
#define MAX_CYKLI 5         // Czas życia drona (liczba cykli ładowania przed złomowaniem)
#define CZAS_LADOWANIA 2    // Czas trwania ładowania (sekundy)
#define CZAS_LOTU 1         // Czas trwania jednego cyklu lotu (sekundy)
#define KOSZT_LOTU 15       // Zużycie baterii w locie (%)
#define KOSZT_CZEKANIA 10   // Zużycie baterii w kolejce do bazy (%)
#define BATERIA_PELNA 100   // Stan po naładowaniu (%)

#define FTOK_PATH "common.h"
#define FTOK_ID 'R'

// --- STANY PROCESU DRONA ---
#define STAN_WOLNY 0        // Slot w tablicy jest pusty (dron nie istnieje)
#define STAN_LADOWANIE 1    // Dron jest w bazie i ładuje baterię
#define STAN_LOT 2          // Dron lata w strefie operacyjnej
#define STAN_POWROT 3       // Dron wraca i czeka przed wejśceim do bazy
#define STAN_ATAK 4         // Dron wykonuje procedurę ataku samobójczego
#define STAN_ZNISZCZONY 5   // Stan końcowy (bateria 0% lub po ataku samobójczym)

// Struktura pojedynczego drona w pamięci dzielonej
struct Dron {
    pid_t pid;              // ID procesu systemowego
    int bateria;            // Poziom baterii (0%-100%)
    int liczba_cykli;       // Liczba odbytych ładowań
    int stan;               // Aktualny stan
    int id_wewnetrzne;      // ID w tablicy (0-MAX_DRONÓW)
};

// --- GŁOWNA STRUKTURA PAMIĘCI DZIELONEJ ---
struct StanRoju {
    struct Dron drony[MAX_DRONOW];      // Tablica wszystkich dronów
    int pojemnosc_bazy;                 // Aktualna liczba miejsc w bazie
    int aktualny_limit_dronow;          // Aktualny limit liczebności roju
    int platformy_do_usuniecia;         // Zmienna przechowująca liczbę platform, które muszą zostać zniszczone przez drony przy wylocie
};

// Typy komunikatów dla Kolejki
#define TYP_DODAJ_PLATFORMY 1
#define TYP_USUN_PLATFORMY 2

// Struktura komunikatu (Dowódca -> Operator)
struct Komunikat {
    long mtype;
};

// Unia wymagana przez funkcję semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// --- INDEKSY SEMAFORÓW ---
#define SEM_BAZA 0              // Semafor licznikowy (wolne miejsca w bazie)
#define SEM_WEJSCIE_1 1         // Mutex dla bramki nr 1
#define SEM_WEJSCIE_2 2         // Mutex dla bramki nr 2
#define SEM_PAMIEC 3            // Mutex chroniący zapis do pamięci dzielonej
#define ILOSC_SEMAFOROW 4

/*
 * Funkcja logująca
 * Zapisuje logi do pliku "logi.txt".
 * Wykorzystuje funkcje: open(), write(), close().
*/
static void zapisz_do_pliku(const char *tekst) {
    // O_WRONLY: tylko do zapisu, O_CREAT: towrzy jeśli nie istnieje, O_APEND: dopisuje na końcu
    int fd = open("logi.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd != -1) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char linia[1024];
        
        // Formatowanie znacznika czasu i PID procesu
        sprintf(linia, "[%04d-%02d-%02d %02d:%02d:%02d][%d] %s", 
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec, 
                getpid(), tekst);
        
        write(fd, linia, strlen(linia));
        
        close(fd);
    }
}

// Makro pomocniczne: wypisuje na ekranie oraz zapisuje do pliku
#define loguj(...) { \
    char bufor[512]; \
    sprintf(bufor, __VA_ARGS__); \
    printf("%s", bufor); \
    fflush(stdout); \
    zapisz_do_pliku(bufor); \
}

#endif