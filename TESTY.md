# Raport z Testów Obciążeniowych i Skalowalności Systemu

**Autor:** Dawid Kamiński (155272)
**Data:** 28.01.2026
**Typ Testów:** Stress Testing & High Performance Computing (HPC)
**Cel:** Weryfikacja stabilności mechanizmów IPC (System V) w warunkach ekstremalnego nasycenia procesami.

---

## 1. Wstęp
Niniejszy dokument przedstawia wyniki testów weryfikujących zachowanie systemu w warunkach brzegowych. Testy przeprowadzono w środowisku Linux.

---

## 2. Scenariusze Testowe

### SCENARIUSZ 1: "Burza Sygnałów" (Signal Storm & Self-Healing)
**Cel:** Sprawdzenie, czy semafory (Mutexy) zachowują spójność, gdy procesy są masowo zabijane sygnałami asynchronicznymi, oraz czy Operator potrafi automatycznie odbudować rój.

#### Przebieg Testu
1.  **Start:** Uruchomienie systemu z parametrem **5000 dronów**. Oczekiwanie na pełną stabilizację.
2.  **Atak:** Wysłanie sygnału do wszystkich procesów potomnych jednocześnie:
    ```bash
    killall -SIGUSR1 dron
    ```
3.  **Weryfikacja:** Sprawdzenie stanu semaforów poleceniem `ipcs -s` (czy nie ma martwych blokad).

#### Dowód (Logi i Wyniki)
* **Logi Operatora:** System wykrył masową śmierć procesów i natychmiast rozpoczął procedurę odtwarzania (`fork`).
    ```text
    [2026-01-28 23:04:23][22359]     [DRON 4262] PID: 22359. Uruchomiono. Bateria 0%.
    [2026-01-28 23:04:24][22359]     [DRON 4262] Bateria naładowana (100%). Czekam na wylot.
    [2026-01-28 23:04:46][22359]     [DRON 4262] Wylot bramką 1...
    [2026-01-28 23:04:46][22359]     [DRON 4262] Lot w strefie operacyjnej...
    [2026-01-28 23:04:46][22359]     [DRON 4262] !!! OTRZYMAŁEM ROZKAZ ATAKU SAMOBÓJCZEGO !!! (Bateria: 100%).
    [2026-01-28 23:04:46][22359]     [DRON 4262] !!! ATAK WYKONANY !!!
    [2026-01-28 23:05:06][26957]     [DRON 4262] PID: 26957. Uruchomiono. Bateria 0%.
    [2026-01-28 23:05:07][26957]     [DRON 4262] Bateria naładowana (100%). Czekam na wylot.
    [2026-01-28 23:05:16][26957]     [DRON 4262] Wylot bramką 1...
    [2026-01-28 23:05:16][26957]     [DRON 4262] Lot w strefie operacyjnej...
    ```
* **Stan IPC:**
    ```text
    ------ Semaphore Arrays --------
    key        semid      owner      perms      nsems     
    0x53306d09 35         dawidkamin 666        4
    ```
    *(Tablica semaforów pozostała aktywna, zestaw 4 semaforów jest gotowy do dalszej pracy).*

**Wniosek:** Mechanizm synchronizacji jest odporny na przerwania systemowe.  Funkcja `semop` działa bezpiecznie, a system posiada zdolność samoleczenia.

---

### SCENARIUSZ 2: "Korek Uliczny" (Semaphore Contention)
**Cel:** Weryfikacja kolejkowania procesów przez Scheduler systemu operacyjnego w sytuacji ekstremalnego braku zasobów (*Resource Starvation*).

#### Przebieg Testu
1.  **Konfiguracja:** Ustawienie `CZAS_LADOWANIA = 1s` oraz `BRAMKI = 2` (wąskie gardło).
2.  **Obciążenie:** Uruchomienie **5000 dronów** walczących o te 2 bramki.
3.  **Analiza:** Obserwacja kolejki oczekujących na semaforze.

#### Dowód (Logi i Wyniki)
* **Szczegółowy Stan Semaforów (`ipcs -s -i 35`):**
    ```text
    semnum     value      ncount     zcount     pid       
    0          16         0          0          23066     (Baza - są wolne miejsca)
    1          0          1189       0          28711     (Bramka 1 - KOREK: 1189 czekających)
    2          0          1218       0          22423     (Bramka 2 - KOREK: 1218 czekających)
    3          1          0          0          27431     (Mutex - wolny)
    ```
* **Analiza:**
    Kolumna `value = 0` dla semaforów 1 i 2 oznacza, że bramki są zajęte.
    Kolumna `ncount > 1000` dowodzi, że system operacyjny utrzymuje stabilną kolejkę ponad 2300 procesów oczekujących na dostęp (FIFO). Nie wystąpił *Deadlock* (system nie zamarzł, procesy wymieniają się miejscami).

**Wniosek:** System operacyjny poprawnie kolejkuje procesy w strukturze semafora System V. Architektura jest odporna na zagłodzenie zasobów. 

---

### SCENARIUSZ 3: "Wielkie Wygaszanie" (Dynamiczna Redukcja)
**Cel:** Sprawdzenie, czy system potrafi płynnie zredukować liczbę procesów z 5000 do 4 bez pozostawiania procesów Zombie (sierot) i wycieków pamięci.

#### Konfiguracja Testowa (Symulacja Przyspieszona)
W celu zweryfikowania poprawności zwalniania zasobów w rozsądnym czasie, parametry czasowe symulacji zostały zmodyfikowane dla tego konkretnego testu:
* `MAX_CYKLI`: **5** (zamiast 50) – wymuszenie szybkiej rotacji pokoleń.
* `CZAS_LOTU / LADOWANIA`: **1s** – skrócenie cyklu pracy przy zachowaniu realnego obciążenia schedulera (funkcja `sleep(1)`).
* `KOSZT_LOTU`: **20%** – przyspieszone zużycie baterii.

#### Przebieg Testu
1.  **Stan Wysoki:** System pracuje z 5000 procesami.
2.  **Rozkaz:** Seria komend redukcji wysyłana pętlą w bashu, aż do osiągnięcia limitu 4.
3.  **Obserwacja:** Monitorowanie tabeli procesów (`ps`, `htop`) oraz logów zamykania.

#### Dowód (Logi i Wyniki)
* **Logika Redukcji (Graceful Shutdown):**
    ```text
    [2026-01-28 23:38:14][785]     [DRON 2004] Bateria naładowana (100%). Czekam na wylot.
    [2026-01-28 23:38:25][785]     [DRON 2004] Wylot bramką 2...
    [2026-01-28 23:38:25][785]     [DRON 2004] Demontaż platformy (Redukcja).
    [2026-01-28 23:38:25][785]     [DRON 2004] Lot w strefie operacyjnej...
    ...
    [2026-01-28 23:38:45][785]     [DRON 2004] Baza pełna. Oczekiwanie...
    [2026-01-28 23:38:46][785]     [DRON 2004] Ląduję bramką 2...
    [2026-01-28 23:38:47][785]     [DRON 2004] Limit cykli osiągnięty (cykle: 6). Złomowanie.
    [2026-01-28 23:38:47][785]     [DRON 2004] Złomowanie zakończone.
    ```
* **Stan Końcowy (Brak procesów Zombie):**
    ```text
      polecenie: ps -C dron
        PID TTY          TIME CMD
        4765 pts/1    00:00:00 dron
        4793 pts/1    00:00:00 dron
        4803 pts/1    00:00:00 dron
        4816 pts/1    00:00:00 dron
    ```
    *(Tabela procesów czysta. Wszystkie nadmiarowe procesy zostały poprawnie odebrane przez waitpid operatora).*

**Wniosek:** Operator prawidłowo wstrzymał tworzenie nowych procesów. Nadmiarowe jednostki dokonały bezpiecznego zamknięcia (*Graceful Shutdown*) po zakończeniu skróconego cyklu życia.

---

### SCENARIUSZ 4: "Test Konfliktu Sterowania" (Race Conditions)
**Cel:** Weryfikacja atomowości operacji na Pamięci Dzielonej przy sprzecznych rozkazach wysyłanych w milisekundach.

#### Przebieg Testu
1.  **Atak Rozkazami:** Wysłanie naprzemiennych komend:
    `(Redukcja)` i `(Rozbudowa)`
    w bardzo krótkim czasie (szybciej niż czas reakcji Operatora).

#### Dowód (Logi i Wyniki)
* **Logi Operatora:**
    ```text
    [2026-01-28 23:52:58][32747] [SYGNAŁ 2] Redukcja! Drony: 10000->5000. Baza: 4999->2499
    [2026-01-28 23:52:58][32747] [SYGNAŁ 2] Zdemontowano od razu: 2 platform. Czeka na demontaż: 2498.
    [2026-01-28 23:53:00][32747] [SYGNAŁ 1] Anulowano usuwanie 2324 platform z powodu rozbudowy.
    [2026-01-28 23:53:00][32747] [SYGNAŁ 1] ROZBUDOWA (x2)! Baza: 2499->4999. Drony: 5000->10000
    [2026-01-28 23:53:02][32747] [SYGNAŁ 2] Redukcja! Drony: 10000->5000. Baza: 4999->2499
    [2026-01-28 23:53:02][32747] [SYGNAŁ 2] Zdemontowano od razu: 0 platform. Czeka na demontaż: 2500.
    [2026-01-28 23:53:04][32747] [SYGNAŁ 1] Anulowano usuwanie 2319 platform z powodu rozbudowy.
    [2026-01-28 23:53:04][32747] [SYGNAŁ 1] ROZBUDOWA (x2)! Baza: 2499->4999. Drony: 5000->10000
    ```

**Wniosek:** Algorytm kompensacji długu w pamięci dzielonej zapobiegł uszkodzeniu liczników logicznych. Stan bazy pozostał spójny mimo wyścigu rozkazów.

---

### SCENARIUSZ 5: "Szklany Sufit" (Resource Exhaustion / Obsługa błędu fork)
**Cel:** Weryfikacja stabilności Operatora w sytuacji wyczerpania limitu procesów użytkownika (test odporności na błędy systemowe).

#### Konfiguracja Testowa
* **Ograniczenie:** `ulimit -u 300` (Sztuczna blokada na poziomie OS).
* **Zadanie:** Uruchomienie `500` dronów (próba przekroczenia blokady).

#### Przebieg Testu
1.  System wystartował i tworzył procesy do momentu osiągnięcia limitu (~280-300 procesów).
2.  Po nasyceniu tablicy procesów, funkcja systemowa `fork()` zaczęła zwracać błąd.

#### Dowód (Logi i Wyniki)
* **Logi z momentu nasycenia:**
    ```text
    [2026-01-29 00:26:19][28495] [OPERATOR] Wykryto brak drona na pozycji 60. Tworzę nowego...
    [2026-01-29 00:26:19][28495] [OPERATOR] Wykryto brak drona na pozycji 63. Tworzę nowego...
    [2026-01-29 00:26:19][28495] [OPERATOR] Wykryto brak drona na pozycji 67. Tworzę nowego...
    ...
    [2026-01-29 00:26:47][28586]     [DRON 67] Bateria 0%. Rozbity w kolejce.
    [2026-01-29 00:26:47][28584]     [DRON 60] Baza pełna. Oczekiwanie...
    [2026-01-29 00:26:47][28584]     [DRON 60] Bateria 0%. Rozbity w kolejce.
    [2026-01-29 00:26:47][28585]     [DRON 63] Baza pełna. Oczekiwanie...
    [2026-01-29 00:26:47][28585]     [DRON 63] Bateria 0%. Rozbity w kolejce.
    ```
* **Zachowanie Systemu:**
    System **nie uległ awarii** (brak `Segmentation Fault`). Operator kontynuował pracę w trybie ograniczonym, zarządzając tylko tą grupą dronów, którą udało się utworzyć. Nadmiarowe żądania utworzenia procesów były odrzucane przez system, co nie wpłynęło na stabilność procesu głównego.

**Wnioski Końcowe:**
Aplikacja poprawnie znosi brak zasobów systemowych. Operator jest odporny na błędy funkcji `fork()`, co spełnia wymaganie niezawodności w warunkach braku pamięci/PID-ów.

---

## 3. Podsumowanie Projektu
Przeprowadzone testy potwierdziły, że projekt spełnia wszystkie założenia specyfikacji, a dodatkowo wykazuje odporność na błędy systemowe i ekstremalne obciążenie.
