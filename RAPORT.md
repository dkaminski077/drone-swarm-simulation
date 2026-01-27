# RAPORT PROJEKTOWY: System Roju Dronów

**Autor:** Dawid Kamiński (155272)  
**Przedmiot:** Systemy Operacyjne  
**Temat:** "Rój Dronów" – symulacja dostępu do ograniczonych zasobów.  
**Data:** 2026-01-27 

---

## 1. Cel i zakres projektu

Celem projektu było stworzenie wieloprocesowej symulacji systemu sterowania rojem dronów w środowisku Linux. Projekt modeluje problem rywalizacji o zasoby (miejsca w bazie, bramki wejściowe) oraz synchronizacji procesów niezależnych w czasie rzeczywistym.

System składa się z trzech modułów komunikujących się za pomocą mechanizmów IPC Systemu V:
1.  **Operator (Zarządca):** Proces macierzysty odpowiedzialny za cykl życia dronów (`fork/exec`), dynamiczne skalowanie systemu i usuwanie martwych procesów.
2.  **Dron (Agent):** Proces potomny symulujący lot, zużycie energii i procedurę ładowania. Posiada wewnętrzną logikę decyzyjną.
3.  **Dowódca (Interfejs):** Niezależny proces umożliwiający użytkownikowi sterowanie parametrami symulacji (zmiana limitów, wydawanie rozkazów).

---

## 2. Opis implementacji i mechanizmów

W projekcie wykorzystano następujące mechanizmy systemu operacyjnego:

### A. Zarządzanie procesami
* **Tworzenie:** Funkcja `fork()` i `execl()` w procesie Operatora służy do dynamicznego powoływania nowych dronów w odpowiedzi na zwolnienie zasobów w bazie.
* **Usuwanie (Zombie Cleanup):** Wdrożono mechanizm zapobiegający powstawaniu procesów zombie. Operator w głównej pętli wywołuje funkcję `waitpid(-1, &status, WNOHANG)`, co pozwala na natychmiastowe usunięcie drona, który zakończył działanie, bez blokowania pętli głównej.

### B. Synchronizacja i IPC (Inter-Process Communication)
* **Pamięć Dzielona (Shared Memory):** Przechowuje tablicę struktur `StanRoju` (stan baterii, ID, statusy dronów), umożliwiając natychmiastowy dostęp do danych dla wszystkich procesów.
* **Semafory (System V):**
    * `SEM_BAZA`: Semafor licznikowy kontrolujący liczbę wolnych miejsc w bazie.
    * `SEM_WEJSCIE_1/2`: Mutexy binarne chroniące dostęp do bramek (wąskie gardła).
    * `SEM_PAMIEC`: Mutex chroniący sekcję krytyczną zapisu do pamięci dzielonej (zapobiega wyścigom).
* **Kolejki Komunikatów (Message Queues):** Służą do asynchronicznego przesyłania rozkazów (Rozbudowa/Redukcja) od Dowódcy do Operatora.

### C. Obsługa Sygnałów i Plików
* **Sygnały:**
    * `SIGINT`: Przechwytywany przez Operatora w celu bezpiecznego zamknięcia systemu (usunięcie IPC, zabicie procesów).
    * `SIGUSR1`: Przesyłany przez Dowódcę do konkretnego drona w celu wymuszenia ataku samobójczego.
* **Logowanie:** Zgodnie z wymogiem, system używa niskopoziomowych funkcji wejścia-wyjścia (`open`, `write`, `close`) z flagą `O_APPEND`. Logi zawierają precyzyjny znacznik czasu ISO 8601 (`RRRR-MM-DD GG:MM:SS`) oraz PID procesu.

---

## 3. Elementy wyróżniające (Zaawansowane mechanizmy)

W projekcie wykazano się inwencją twórczą, implementując rozwiązania wykraczające poza standardową obsługę IPC, zwiększające stabilność i realizm symulacji:

### 3.1. Algorytm "Lazy Release" (Asynchroniczna redukcja zasobów)
Zaimplementowano mechanizm obsługi redukcji bazy w warunkach pełnego obciążenia.
* **Problem:** Podczas redukcji bazy (zmniejszanie limitu miejsc), zajęte sloty nie mogą być natychmiast zwolnione, co w standardowym podejściu prowadziłoby do zakleszczenia (deadlock) Operatora czekającego na semafor.
* **Rozwiązanie:** Zastosowano system "długu zasobów". Operator nie blokuje się w oczekiwaniu na zwolnienie miejsca, lecz inkrementuje licznik `platformy_do_usuniecia` w pamięci dzielonej. Drony, opuszczając bazę, sprawdzają ten licznik i **atomowo "niszczą"** swoje miejsce (nie podnoszą semafora), spłacając dług systemowy. Pozwala to na płynne skalowanie w dół bez ryzyka zakleszczenia (*deadlock*).

### 3.2. Architektura Non-Blocking (Nieblokująca)
Operator został zaprojektowany jako proces czasu rzeczywistego, który nigdy nie zawiesza się w oczekiwaniu na zdarzenia. Wykorzystano flagi `IPC_NOWAIT` przy obsłudze kolejek komunikatów (`msgrcv`) oraz przy sprawdzaniu dostępności miejsc (`semop`). Dzięki temu Operator może jednocześnie zarządzać cyklem życia dronów, odbierać rozkazy i monitorować stan roju.

### 3.3. Atomowość Logowania
Zaimplementowano moduł logowania oparty na wywołaniach systemowych `open` / `write` z flagą `O_APPEND`. Gwarantuje to **atomowość dopisywania danych** na poziomie jądra systemu operacyjnego, zapobiegając uszkodzeniu struktury pliku logów (*race condition*) przy jednoczesnym zapisie przez wiele procesów.

### 3.4. Logika Agentowa i Priorytety
Drony posiadają zaimplementowaną logikę decyzyjną. Proces potrafi ocenić swój stan wewnętrzny (poziom baterii) i na tej podstawie **odrzucić sygnał sterujący** (`SIGUSR1` - Atak), jeśli jego wykonanie zagrażałoby stabilności systemu (ryzyko rozbicia poza strefą walki).

### 3.5. Bezpieczeństwo Sygnałów (Async-Signal-Safety)
W celu wyeliminowania ryzyka zakleszczenia (*deadlock*) podczas obsługi sygnału `SIGUSR1` (Atak), zrezygnowano z używania niebezpiecznych funkcji (jak `printf` czy `semop`) wewnątrz handlera.
Zastosowano flagę typu `volatile sig_atomic_t`, która jest jedynie ustawiana w handlerze. Właściwa logika biznesowa (zwolnienie zasobów, operacje wejścia-wyjścia) wykonywana jest w bezpiecznym momencie w głównej pętli procesu, co gwarantuje stabilność pamięci i semaforów.

---

## 4. Scenariusze Testowe

Poprawność działania systemu zweryfikowano zgodnie z dokumentacją testową (`TESTY.md`). Poniżej przedstawiono wyniki kluczowych testów obciążeniowych i funkcjonalnych.

### TC-01: Weryfikacja współbieżności i kolejek (Stress Test)
**Cel:** Sprawdzenie zachowania systemu, gdy liczba dronów ($N=12$) przekracza pojemność bazy ($P=4$).
* **Akcja:** Uruchomienie Operatora bez ingerencji użytkownika.
* **Rezultat:** Drony, dla których zabrakło miejsca w bazie, oczekują na semaforze `SEM_BAZA`. Logi potwierdzają kolejkowanie: `[ZOLTY] Baza pełna. Oczekiwanie...`. Po wyczerpaniu baterii w kolejce następuje usunięcie procesu (`Rozbity w kolejce`).
* **Status:** ✅ ZALICZONY

### TC-02: Skalowanie Systemu i "Lazy Release"
**Cel:** Weryfikacja algorytmu zapobiegającego zakleszczeniom (Deadlock) przy dynamicznej redukcji zasobów.
* **Akcja:** Rozbudowa bazy do maksimum, a następnie nagła redukcja (rozkaz `[2]` od Dowódcy).
* **Rezultat:** Operator nie blokuje się w oczekiwaniu na wolne miejsca. Zleca redukcję "na kredyt" (log: `Czeka na demontaż`). Drony fizycznie niszczą platformy przy wylocie, raportując: `[CZERWONY] Demontaż platformy (Redukcja)`.
* **Status:** ✅ ZALICZONY

### TC-03: Priorytetyzacja Rozkazów (Sygnały Asynchroniczne)
**Cel:** Sprawdzenie logiki decyzyjnej drona w odpowiedzi na sygnał ataku `SIGUSR1`.
* **Akcja:** Wysłanie sygnału ataku do losowego drona.
* **Rezultat:**
    * **Przypadek A (Dron naładowany):** Natychmiastowe wykonanie misji (`!!! ATAK WYKONANY !!!`).
    * **Przypadek B (Dron słaby, <20%):** Ignorowanie rozkazu w celu bezpiecznego powrotu (`[ZOLTY] Atak anulowany - zbyt słaba bateria`).
* **Status:** ✅ ZALICZONY

### TC-04: Graceful Shutdown i Procesy Zombie
**Cel:** Weryfikacja poprawności czyszczenia zasobów i tablicy procesów (zgodność z wymogiem 5.2.b - `waitpid`).
* **Akcja:** Nagłe przerwanie pracy Operatora kombinacją `Ctrl+C` (sygnał `SIGINT`).
* **Rezultat:** Operator przechwytuje sygnał, usuwa zasoby IPC (`shm`, `sem`, `msg`) i kończy procesy potomne. Weryfikacja poleceniem `pgrep dron` po zamknięciu zwraca wynik pusty (brak procesów zombie).
* **Status:** ✅ ZALICZONY

### TC-05: Integralność Danych i Logów
**Cel:** Sprawdzenie poprawności zapisu do pliku (zgodność z wymogiem 5.2.a - `open/write` z flagą `O_APPEND`).
* **Akcja:** Analiza pliku `logi.txt` po długotrwałym działaniu wielu procesów.
* **Rezultat:** Każdy wpis posiada poprawny format ISO 8601 (`[RRRR-MM-DD GG:MM:SS][PID]`). Wpisy są atomowe i nie nakładają się na siebie, co potwierdza poprawność synchronizacji dostępu do pliku.
* **Status:** ✅ ZALICZONY

---

## 5. Linki do kodu

Poniższe odnośniki prowadzą do fragmentów kodu w repozytorium, obrazujących wykorzystanie wymaganych funkcji systemowych:

### a. Tworzenie i obsługa plików
* **Funkcje:** `open()`, `write()`, `close()`
* **Implementacja:** [common.h - funkcja zapisz_do_pliku](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/common.h#L107-L125)
    * *Szczegóły:* Użycie flagi `O_APPEND` do atomowego zapisu logów.

### b. Tworzenie procesów
* **Funkcje:** `fork()`, `execl()`, `exit()`, `waitpid()`
* **Tworzenie (`fork`):** [operator.c - wywołanie fork](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L258)
* **Uruchamianie (`execl`):** [operator.c - wywołanie execl](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L264)
* **Kończenie (`exit`):** [dron.c - wywołanie exit](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/dron.c#L129)
* **Sprzątanie (`waitpid`):** [operator.c - pętla usuwająca zombie](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L178)

### c. Tworzenie i obsługa wątków
* *Nie dotyczy:* Projekt zrealizowano w oparciu o procesy, a nie wątki.

### d. Obsługa sygnałów
* **Funkcje:** `signal()`, `kill()`
* **Rejestracja handlera:** [operator.c - signal dla SIGINT](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L108)
* **Wysyłanie sygnału:** [dowodca.c - kill wysyłający SIGUSR1](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/dowodca.c#L116)

### e. Synchronizacja procesów
* **Funkcje:** `semget()`, `semctl()`, `semop()`
* **Inicjalizacja:** [operator.c - semget i semctl SETVAL](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L139-L150)
* **Operacje atomowe (P/V):** [operator.c - funkcje pomocnicze P/V](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L25-L35)

### f. Łącza nazwane i nienazwane
* *Nie dotyczy:* Komunikacja oparta o Pamięć Dzieloną i Kolejki Komunikatów.

### g. Segmenty pamięci dzielonej
* **Funkcje:** `shmget()`, `shmat()`, `shmctl()`
* **Tworzenie:** [operator.c - shmget](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L130-L136)
* **Dołączanie:** [dron.c - shmat](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/dron.c#L109-L113)
* **Usuwanie:** [operator.c - shmctl IPC_RMID](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L43-L46)

### h. Kolejki komunikatów
* **Funkcje:** `msgget()`, `msgsnd()`, `msgrcv()`
* **Tworzenie:** [operator.c - msgget](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L122-L128)
* **Odbieranie:** [operator.c - msgrcv](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/operator.c#L183)
* **Wysyłanie:** [dowodca.c - msgsnd](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/dowodca.c#L64)

### i. Kluczowe algorytmy i logika
* **Algorytm "Lazy Release" (Operator):** [operator.c - obsługa długu zasobów](https://github.com/dkaminski077/drone-swarm-simulation/blob/2160df3bb455bb7c0d299945fc2cfd89a654d725/operator.c#L236-L244)
    * *Szczegóły:* Operator nie czeka na zwolnienie semafora, lecz zapisuje "dług" w zmiennej `platformy_do_usuniecia`.
* **Algorytm "Lazy Release" (Dron):** [dron.c - spłacanie długu](https://github.com/dkaminski077/drone-swarm-simulation/blob/2160df3bb455bb7c0d299945fc2cfd89a654d725/dron.c#L163-L178)
    * *Szczegóły:* Dron przy wylocie sprawdza dług i zamiast oddać zasób (`V`), niszczy go (nie podnosi semafora).
* **Bezpieczeństwo Sygnałów (Volatile):** [dron.c - deklaracja flagi](https://github.com/dkaminski077/drone-swarm-simulation/blob/2160df3bb455bb7c0d299945fc2cfd89a654d725/dron.c#L25)
    * *Szczegóły:* Użycie `volatile sig_atomic_t` zapobiegające wyścigom i optymalizacjom kompilatora w handlerze.
* **Bezpieczna pętla Sleep:** [dron.c - obsługa przerwań](https://github.com/dkaminski077/drone-swarm-simulation/blob/2160df3bb455bb7c0d299945fc2cfd89a654d725/dron.c#L125-L132)
    * *Szczegóły:* Pętla `while` wznawiająca `sleep` w przypadku przerwania przez sygnał, gwarantująca pełny czas ładowania.
* **Logika Agentowa (Odrzucenie rozkazu):** [dron.c - funkcja obsluz_atak](https://github.com/dkaminski077/drone-swarm-simulation/blob/421ba4ba5bed91f97ed1ac823bb071b769bc3412/dron.c#L53-L81)
    * *Opis:* Dron autonomicznie decyduje o ignorowaniu rozkazu ataku, jeśli poziom baterii jest krytyczny (<20%).

---

## 6. Wnioski

Zrealizowany projekt spełnia wszystkie założenia funkcjonalne. Zastosowanie pamięci dzielonej zapewniło wysoką wydajność komunikacji, a semafory skutecznie chronią zasoby przed wyścigami. Wprowadzenie mechanizmu "Lazy Release" oraz obsługi "Zombie Cleanup" czyni system odpornym na błędy i stabilnym nawet przy długotrwałym działaniu. Kod został zabezpieczony przed wyciekami pamięci i jest zgodny z dobrymi praktykami programowania systemowego w języku C.
