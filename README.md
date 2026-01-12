# 🚁 System Roju Dronów (Symulacja Wieloprocesowa)

> **Autor:** Dawid Kamiński (155272) **Przedmiot:** Systemy Operacyjne **Technologia:** C (Linux API, System V IPC)

---

## 📖 O Projekcie

Projekt jest implementacją problemu współbieżnego symulującego zarządzanie rojem autonomicznych dronów. System modeluje rywalizację o zasoby (miejsca w bazie, bramki wejściowe) oraz komunikację między niezależnymi procesami w czasie rzeczywistym.

Głównym celem projektu było stworzenie stabilnego, odpornego na błędy (deadlocki, wycieki pamięci) środowiska z wykorzystaniem mechanizmów **IPC Systemu V**.

## 🛠 Kluczowe Funkcjonalności (Highlights)

W projekcie zaimplementowano szereg zaawansowanych mechanizmów systemowych:

* **Autorski algorytm "Lazy Release":** Rozwiązanie problemu zakleszczeń przy redukcji zasobów. Operator nie blokuje się oczekując na zwolnienie semafora, lecz zleca "dług", który drony spłacają asynchronicznie przy wylocie (atomowe niszczenie semafora).
* **Zombie Cleanup (Non-blocking):** Operator działa w trybie ciągłym, na bieżąco usuwając martwe procesy potomne (`waitpid` z flagą `WNOHANG`), co zapobiega zaśmiecaniu tablicy procesów.
* **Logika Agentowa:** Drony posiadają "instynkt samozachowawczy" – potrafią odrzucić rozkaz ataku samobójczego, jeśli poziom baterii jest krytyczny (<20%).
* **Atomowe Logowanie:** System logów oparty na funkcjach systemowych `open`/`write` z flagą `O_APPEND`, gwarantujący integralność danych przy wielu piszących procesach jednocześnie.

## 🏗 Architektura Systemu
System składa się z trzech niezależnych modułów komunikujących się przez Pamięć Dzieloną, Semafory i Kolejki Komunikatów:

1. **`operator` (Zarządca)** – Proces nadrzędny. Tworzy drony (`fork`/`exec`), inicjalizuje IPC, zarządza skalowaniem roju i sprząta po zakończonych procesach.
2. **`dron` (Agent)** – Proces potomny. Symuluje lot, zużycie energii, cykl ładowania oraz procedurę powrotu do bazy (kolejkowanie).
3. **`dowodca` (Interfejs)** – Niezależny proces sterujący. Pozwala użytkownikowi wydawać rozkazy (Rozbudowa, Redukcja, Atak) wpływające na pracę Operatora i Dronów.

## 🚀 Instrukcja Uruchomienia

### 1. Kompilacja
Wymagany jest kompilator GCC oraz system Linux.

    gcc operator.c -o operator
    gcc dron.c -o dron
    gcc dowodca.c -o dowodca

### 2. Uruchomienie
System wymaga dwóch terminali (lub uruchomienia w tle).

**Terminal 1 (Start Operatora):**

    ./operator

*Operator zainicjalizuje system i zacznie wypuszczać drony.*

**Terminal 2 (Start Dowódcy):**

    ./dowodca

*Uruchomi się interfejs tekstowy do sterowania rojem.*

### 3. Sterowanie (Dowódca)
Dostępne komendy w menu:
* `[1]` - **Rozbudowa roju:** Zwiększa limit dronów i dodaje miejsca w bazie.
* `[2]` - **Redukcja roju:** Zmniejsza zasoby (testuje algorytm Lazy Release).
* `[a]` - **Atak Samobójczy:** Wysyła sygnał `SIGUSR1` do losowego drona.
* `[q]` - **Wyjście.**

Aby bezpiecznie zamknąć symulację i posprzątać zasoby, w terminalu Operatora wciśnij `Ctrl+C`.

## 📂 Struktura Plików
* `operator.c` - Kod źródłowy zarządcy (inicjalizacja IPC, pętla główna).
* `dron.c` - Kod źródłowy procesu drona (cykl życia, logika).
* `dowodca.c` - Kod źródłowy interfejsu sterującego.
* `common.h` - Plik nagłówkowy (wspólne struktury, stałe, funkcja logowania).
* `Raport.md` - Szczegółowa dokumentacja projektowa, weryfikacja wymagań i linki do kodu.
* `TESTY.md` - Opis scenariuszy testowych (QA).

---
*Projekt wykonany w ramach zaliczenia laboratoriów Systemy Operacyjne (2026).*
