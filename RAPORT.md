# RAPORT PROJEKTOWY: System Roju Dronów (Wersja High-Performance)

**Autor:** Dawid Kamiński (155272)  
**Przedmiot:** Systemy Operacyjne  
**Temat:** "Rój Dronów" – wieloprocesowa symulacja zarządzania zasobami ograniczonymi.  
**Data:** 2026-02-03  

---

## 1. Cel i zakres projektu

Celem projektu jest implementacja zaawansowanego systemu sterowania masowym rojem dronów w środowisku Linux. System modeluje dynamiczną rywalizację tysięcy niezależnych procesów o ograniczone zasoby fizyczne, takie jak platformy startowe w bazie oraz bramki przelotowe. Projekt kładzie szczególny nacisk na wydajność (obsługa do 25 000 jednostek), odporność na awarie oraz atomowość operacji w pamięci współdzielonej.

---

## 2. Architektura Systemu

System opiera się na czterech współpracujących modułach wykorzystujących mechanizmy IPC Systemu V:

1.  **Operator (Zarządca):** Proces nadrzędny odpowiedzialny za inicjalizację zasobów IPC, monitorowanie cyklu życia dronów oraz dynamiczne skalowanie limitów bazy i populacji.
2.  **Dron (Agent):** Samodzielny proces realizujący maszynę stanów: Ładowanie -> Lot -> Powrót. Posiada własną logikę decyzyjną i reaguje na sygnały asynchroniczne.
3.  **Dowódca (Interfejs):** Moduł sterujący, który za pomocą kolejek komunikatów przesyła rozkazy taktyczne do Operatora oraz wydaje bezpośrednie polecenia ataku jednostkom.
4.  **Monitor (Wizualizacja):** Niezależny proces odczytujący stan pamięci współdzielonej z częstotliwością 10Hz, prezentujący statystyki roju bez obciążania logiki sterującej.

---

## 3. Implementacja Mechanizmów Systemowych

### 3.1. Synchronizacja i Bezpieczeństwo (SEM_UNDO)
W celu zapobiegania trwałym blokadom systemu w przypadku awarii procesu, zastosowano funkcje `P_mutex` i `V_mutex` oparte na semaforach z flagą `SEM_UNDO`. Gwarantuje to, że jeśli dron zginie w sekcji krytycznej, jądro systemu automatycznie zwolni mutex chroniący pamięć współdzieloną.

### 3.2. Mechanizm Self-Healing (Odzyskiwanie Zasobów)
Operator w pętli głównej wykorzystuje nieblokujące wywołanie `waitpid(-1, &status, WNOHANG)`. Po wykryciu zakończenia procesu drona, funkcja `odzyskaj_slot` czyści rekord w tablicy `StanRoju`, resetuje PID i sprawdza, czy proces ten nie "zadłużył" bazy, spłacając dług demontażu platform.

### 3.3. Logika "Deferred Release" (Spłata Długu)
System implementuje algorytm redukcji bazy przy pełnym obłożeniu:
* Podczas zmniejszania bazy, Operator inkrementuje licznik `platformy_do_usuniecia`.
* Drony opuszczające bazę sprawdzają ten stan i atomowo "likwidują" swoje miejsce (nie podnosząc semafora `SEM_BAZA`), co pozwala na płynne skalowanie zasobów bez ryzyka zakleszczenia.

### 3.4. Obsługa Sygnałów i Atomowość
* **SIGUSR1 (Atak):** Dron po odebraniu sygnału weryfikuje poziom baterii. Jeśli energia jest krytyczna (<20%), rozkaz jest ignorowany dla zachowania stabilności systemu.
* **SIGTERM (Redukcja):** Operator wysyła ten sygnał do nadmiarowych jednostek podczas drastycznej redukcji limitu populacji.
* **Atomowość I/O:** Logowanie zdarzeń odbywa się przez systemowe wywołania `open` i `write` z flagą `O_APPEND`. Gwarantuje to spójność pliku `logi.txt` przy równoległym zapisie z wielu procesów.

---

## 4. Specyfikacja Techniczna i Limity

* **Limit Techniczny:** 25 000 dronów (`LIMIT_TECHNICZNY`).
* **Struktura SHM:** Tablica `struct Dron` o stałym rozmiarze, co eliminuje kosztowną alokację dynamiczną w czasie pracy.
* **Zarządzanie energią:** Każdy stan drona (lot, czekanie w kolejce) ma zdefiniowany koszt energetyczny (`KOSZT_LOTU`, `KOSZT_CZEKANIA`).
* **Flaga `ma_slot_bazy`:** Precyzyjne powiązanie drona z konkretnym zasobem w semaforze bazy, zapobiegające błędnym operacjom na licznikach podczas wyścigów procesowych.

---

## 5. Scenariusze Testowe

### SCENARIUSZ 1: "Atak Zmasowany" (Signal Storm)
* **Komenda:** `killall -SIGUSR1 dron`
* **Cel:** Sprawdzenie, czy system potrafi obsłużyć jednoczesne nadejście tysięcy sygnałów asynchronicznych (interrupcje systemowe) i czy Operator poprawnie odbuduje strukturę roju.

* **Przebieg:**
    1.  Uruchomienie roju w konfiguracji domyślnej (5000 dronów).
    2.  Wysłanie sygnału `SIGUSR1` (Atak) do wszystkich procesów potomnych jednocześnie.
    3.  Obserwacja licznika `AKTYWNE DRONY` w procesie Monitora.
* **Wynik:**
    Operator natychmiast wykrył masowe zakończenie procesów (zwrócone statusy w pętli `waitpid`) i rozpoczął procedurę `fork`, przywracając stan liczebny roju do zadanego limitu w ciągu kilku sekund. Semafory pozostały spójne, nie wystąpiły zakleszczenia.
    ### Dowód działania (Logi z testu Atak Zmasowany):

**1. Stan przed atakiem (Monitor):**
```text
=== SYSTEM MONITORINGU ROJU (Odświeżanie 10Hz) ===
--------------------------------------------------
 POJEMNOŚĆ BAZY:      2499
 DRONY AKTYWNE:       5000
 LIMIT OBECNIE:       5000 / 10000
 ```
 **2. Reakcja Systemu (Logi Operatora i Dronów):**
```text
[2026-02-03 12:56:43][1468]     [DRON 3533] !!! ATAK WYKONANY !!!
[2026-02-03 12:56:43][1469]     [DRON 4139] Wylot bramką 1...
[2026-02-03 12:56:43][1478]     [DRON 4638] Lot w strefie operacyjnej...
[2026-02-03 12:56:43][1478]     [DRON 4638] !!! OTRZYMAŁEM ROZKAZ ATAKU SAMOBÓJCZEGO !!! (Bateria: 100%).
[2026-02-03 12:56:43][1478]     [DRON 4638] !!! ATAK WYKONANY !!!
[2026-02-03 12:56:43][1482]     [DRON 2108] Wylot bramką 2...
[2026-02-03 12:56:43][1469]     [DRON 4139] Lot w strefie operacyjnej...
[2026-02-03 12:56:43][1469]     [DRON 4139] !!! OTRZYMAŁEM ROZKAZ ATAKU SAMOBÓJCZEGO !!! (Bateria: 100%).
[2026-02-03 12:56:43][1469]     [DRON 4139] !!! ATAK WYKONANY !!!
 ```
 **3. Stan w trakcie regeneracji (Monitor):**
```text
=== SYSTEM MONITORINGU ROJU (Odświeżanie 10Hz) ===
--------------------------------------------------
 POJEMNOŚĆ BAZY:      2499
 DRONY AKTYWNE:       2143      <-- Spadek do około 2100 (Operator na bieżąco uzupełnia braki)
 LIMIT OBECNIE        5000 / 10000
 ```
  **4. Stan po odbudowie (Monitor):**
```text
=== SYSTEM MONITORINGU ROJU (Odświeżanie 10Hz) ===
--------------------------------------------------
 POJEMNOŚĆ BAZY:      2499
 DRONY AKTYWNE:       5000      <-- System automatycznie odtworzył rój
 LIMIT OBECNIE        5000 / 10000
 ```
    
* **Status:** ✅ **ZALICZONY**

---

### SCENARIUSZ 2: "Twardy Reset" (Crash Test)
* **Cel:** Potwierdzenie, że system zachowuje spójność liczników semaforowych (w szczególności SEM_BAZA) w przypadku nagłego przerwania pracy procesów potomnych sygnałem SIGKILL, który uniemożliwia wykonanie procedur czyszczących wewnątrz procesu drona.
* **Metodologia:** Test polega na siłowym zakończeniu wszystkich procesów potomnych komendą killall -9 dron przy pełnym obciążeniu systemu, a następnie analizie zachowania procesu Operatora i wartości semafora System V.
* **Przebieg:**
    1.  Doprowadzenie do stanu stabilnego: Baza pełna (semafor `SEM_BAZA` bliski 0).
    2.  Nagłe zabicie wszystkich dronów sygnałem `SIGKILL` (nieprzechwytywalnym).
    3.  Weryfikacja wartości semafora poleceniem `ipcs -s`.
* **Wynik:**
    1. Licznik wolnych miejsc w bazie nie został błędnie podwojony.
    2. Operator poprawnie wykrył awarię i przyrówcił liczebność roju.
    3. System zachował stabilność i spójność.
    ### Dowód działania:

**1. Stan przed atakiem (Baza pełna):**
```text
Semaphore Array semid=10
uid=1000        gid=1000        cuid=1000       cgid=1000
mode=0666, access_perms=0666
nsems = 4
otime = Tue Feb  3 13:50:29 2026
ctime = Tue Feb  3 13:50:12 2026

semnum     value      ncount     zcount     pid
0          2          0          0          23738  <-- SEM_BAZA: Stabilny (tylko 2 wolne miejsca)
1          0          1205       0          23733
2          0          1125       0          23817
3          1          0          0          23662
```
**2. Stan po ataku:**
```text
Semaphore Array semid=10
uid=1000        gid=1000        cuid=1000       cgid=1000
mode=0666, access_perms=0666
nsems = 4
otime = Tue Feb  3 13:51:15 2026
ctime = Tue Feb  3 13:50:12 2026

semnum     value      ncount     zcount     pid
0          0          0          0          29371  <-- SEM_BAZA: 0 wolnych miejsc (Idealne wypełnienie po awarii)
1          0          1102       0          27730
2          0          1227       0          27565
3          1          0          0          29663
```
* **Status:** ✅ **ZALICZONY**

---

### SCENARIUSZ 3: "Wielkie Wygaszanie" (Dynamic Reduction)
* **Mechanizm:** Zmiana limitu w `dowodca`
* **Cel:** Weryfikacja mechanizmu redukcji

* **Przebieg:**
    1.  Rój pracuje na pełnych obrotach (5000 dronów).
    2.  Dowódca wysyła serię rozkazów `[2] REDUKCJA`, zmniejszając limit do minimum technicznego.
    3.  Obserwacja szybkości zwalniania zasobów w Monitorze.
* **Wynik:**
    Operator natychmiast zmienił pojemność bazy (używając `semctl SETVAL`) i wysłał sygnały `SIGTERM` do nadmiarowych procesów. Drony zakończyły pracę w ułamku sekundy, zwalniając pamięć RAM. Nie odnotowano procesów Zombie ani blokady Operatora na zajętym semaforze.
    ### Dowód działania (Logi z testu Wielkie Wygaszanie):

**1. Stan początkowy (Pełne obciążenie - Monitor):**
```text
=== SYSTEM MONITORINGU ROJU (Odświeżanie 10Hz) ===
--------------------------------------------------
 POJEMNOŚĆ BAZY:      2499
 DRONY AKTYWNE:       5000
 LIMIT OBECNIE:       5000 / 10000
 ```
 **2. Reakcja Operatora (Logi systemowe):**
```text
[2026-02-03 13:56:07][8753] [SYGNAŁ 2] Redukcja! Drony: 5000->2500. Baza: 2499->1249
[2026-02-03 13:56:08][8753] [SYGNAŁ 2] Redukcja! Drony: 2500->1250. Baza: 1249->624
[2026-02-03 13:56:09][8753] [SYGNAŁ 2] Redukcja! Drony: 1250->625. Baza: 624->312
[2026-02-03 13:56:09][8753] [SYGNAŁ 2] Redukcja! Drony: 625->312. Baza: 312->155
[2026-02-03 13:56:09][8753] [SYGNAŁ 2] Redukcja! Drony: 312->156. Baza: 155->77
[2026-02-03 13:56:09][8753] [SYGNAŁ 2] Redukcja! Drony: 156->78. Baza: 77->38
[2026-02-03 13:56:09][8753] [SYGNAŁ 2] Redukcja! Drony: 78->39. Baza: 38->19
[2026-02-03 13:56:09][8753] [SYGNAŁ 2] Redukcja! Drony: 39->19. Baza: 19->9
[2026-02-03 13:56:10][8753] [SYGNAŁ 2] Redukcja! Drony: 19->9. Baza: 9->4
[2026-02-03 13:56:10][8753] [SYGNAŁ 2] Redukcja! Drony: 9->4. Baza: 4->1
[2026-02-03 13:56:10][8753] [SYGNAŁ 2] Redukcja! Drony: 4->4. Baza: 1->1
 ```
 **3. Stan po redukcji (Monitor):**
```text
=== SYSTEM MONITORINGU ROJU (Odświeżanie 10Hz) ===
--------------------------------------------------
 POJEMNOŚĆ BAZY:      1
 DRONY AKTYWNE:       4
 LIMIT OBECNIE:       4 / 10000
 ```
**4. Weryfikacja czystości procesów (Brak Zombie):**
```text
$ ps -ef | grep dron | grep defunct
(brak wyników)
 ```
* **Status:** ✅ **ZALICZONY**

---

### SCENARIUSZ 4: "Korek Uliczny" (Semaphore Contention)
**Konfiguracja:** 5 000 dronów, 2 bramki wejściowe.
**Cel:** Weryfikacja wydolności mechanizmu synchronizacji (muteksów) w sytuacji ekstremalnego współbieżnego dostępu (tzw. High Contention). Test sprawdza, czy system operacyjny poprawnie kolejkuje procesy (zapobiegając zagłodzeniu) przy wąskim gardle wejścia/wyjścia.

* **Przebieg:**
    1.  Ustawienie liczebności roju na 5 000 jednostek (Start systemu).
    2.  Drony masowo próbują wykonać operację startu i lądowania, rywalizując o dostęp do muteksów `SEM_WEJSCIE_1` (bramka 1) i `SEM_WEJSCIE_2` (bramka 2).
    3.  Analiza kolejki semaforów poleceniem `ipcs -s -i [SEM_ID]` w momencie szczytowego obciążenia.

* **Wynik (Zrzut stanu semaforów):**
```text
Semaphore Array semid=12
uid=1000        gid=1000        cuid=1000       cgid=1000
mode=0666, access_perms=0666
nsems = 4
otime = Tue Feb  3 14:02:41 2026
ctime = Tue Feb  3 14:02:28 2026

semnum     value      ncount     zcount     pid
0          7          0          0          18290   <-- SEM_BAZA: Stabilny (7 wolnych miejsc)
1          0          1167       0          18245   <-- BRAMKA 1: Blokada (1167 procesów w kolejce)
2          0          1134       0          18295   <-- BRAMKA 2: Blokada (1134 procesy w kolejce)
3          1          0          0          18243   <-- PAMIĘĆ: Dostępna (system zarządza stanem roju płynnie)
```
* **Status:** ✅ **ZALICZONY**

---

### SCENARIUSZ 5: "Paradoks Rozszerzenia"
* **Cel:** Weryfikacja spójności struktur IPC podczas jednoczesnej fali zgonów procesów (SIGKILL) oraz gwałtownych zmian limitów (Rozbudowa/Redukcja).

* **Metodologia:** Wykorzystanie skryptu test_chaos.sh, który w pętli wykonuje następujące kroki:
    1. Masowe usuwanie 200 losowych procesów dronów sygnałem SIGKILL (wymuszenie asynchronicznego sprzątania slotów).
    2. Jednoczesne wysyłanie sygnałów ROZBUDOWY i REDUKCJI bazy (wymuszenie wyścigu o dostęp do muteksu pamięci).
    3. Monitorowanie żywotności procesu Operatora pod krytycznym obciążeniem.

* **Dowód na odzyskiwanie slotów po ataku chaosu:** W logach Operatora odnotowano skuteczne działanie funkcji reclaim_slot_after_dead, która wyczyściła sloty po procesach zabitych sygnałem nieprzechwytywalnym:
```text
[2026-02-03 14:48:07][13101] [OPERATOR] Wykryto brak drona na pozycji 4410. Tworzę nowego... 
[2026-02-03 14:48:07][13101] [OPERATOR] Wykryto brak drona na pozycji 4411. Tworzę nowego...
```
Wniosek: Mechanizm waitpid oraz funkcja reclaim_slot_after_dead poprawnie zidentyfikowały martwe procesy i wyczyściły pamięć dzieloną, mimo braku sygnału od samych dronów.

* **Dowód na poprawną spłatę długu (Deferred Release):** Logi dronów potwierdzają, że procesy potomne poprawnie odczytują licznik długu i rezygnują z inkrementacji semafora bazy przy wylocie:
```text
[2026-02-03 14:48:07][21028] [DRON 4376] Wylot bramką 1... 
[2026-02-03 14:48:07][21028] [DRON 4376] Demontaż platformy (Redukcja).
```
Wniosek: Mechanizm odroczonego zwalniania zasobów zapobiega "puchnięciu" bazy ponad twarde limity zdefiniowane przez Dowódcę.

* **Dowód na dynamiczną synchronizację bazy:** 
```text
[2026-02-03 14:37:18][22937] [SYGNAŁ 1] Rozbudowa x2: Drony 2500->5000 | Baza 1249->2499 | wolne(sem)=1258 zajete=1241
```
Wniosek: Log dowodzi atomowości operacji przeliczania bazy. Operator poprawnie wyliczył, że przy nowej pojemności 2499 i 1241 dornach aktualnie przebywających w bazie, semafor należy ustawić na dokładnie 1258 wolnych miejsc (1241 + 1258 = 2499). Gwarantuje to brak wycieków zasobów podczas skalowania systemu.

---

## 5. Linki do kodu i wykorzystanych mechanizmów systemowych

Poniższe zestawienie prezentuje implementację kluczowych mechanizmów systemowych w projekcie (System V IPC, obsługa sygnałów, zarządzanie procesami).

### a. Tworzenie i obsługa plików (I/O)
Mechanizm logowania zdarzeń do pliku z użyciem niskopoziomowych funkcji systemowych.
* **Funkcje:** `open()`, `write()`, `close()`
* **Implementacja (O_APPEND):** [common.h - funkcja zapisz_do_pliku](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/common.h#L111-L129)

### b. Zarządzanie Procesami (Process Lifecycle)
Pełny cykl życia procesów: od tworzenia, przez podmianę obrazu, aż po terminację i sprzątanie zombie.
* **Tworzenie (`fork`):** [operator.c - wywołanie fork dla nowego drona](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L359)
* **Uruchamianie (`execl`):** [operator.c - przekazanie argumentów do nowego procesu](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L365)
* **Sprzątanie Zombie (`waitpid`):** [operator.c - pętla z flagą WNOHANG](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L240-L245)
* **Kończenie (`exit`):**
    * [dron.c - po ataku samobójczym](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L243)
    * [dron.c - naturalne wyjście (złomowanie)](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L393)

### c. Obsługa Sygnałów (Signal Handling)
Zaawansowana obsługa przerwań, w tym sygnały sterujące, kończące oraz bezpieczne flagi.
* **SIGINT (Zamykanie systemu):** [operator.c - handler sprzątania zasobów (Ctrl+C)](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L172)
* **SIGUSR1 (Rozkaz Taktyczny):**
    * [dowodca.c - wysłanie sygnału do konkretnego PID](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dowodca.c#L107)
    * [dron.c - rejestracja handlera ataku](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L200)
* **SIGTERM (Redukcja Populacji):**
    * [operator.c - masowe zabijanie nadmiarowych procesów](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L319)
    * [dron.c - handler bezpiecznego wyłączenia](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L201)
* **Bezpieczeństwo (`sig_atomic_t`):** [dron.c - atomowe flagi w handlerach](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L23-L24)

### d. Synchronizacja i Semafory (System V)
Wykorzystanie semaforów zarówno jako Mutexy (binarne) jak i Liczniki zasobów (zliczające).
* **Inicjalizacja (`semget`, `semctl`):** [operator.c - tworzenie i ustawianie wartości początkowych](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L202-L214)
* **Mutex z flagą `SEM_UNDO`:** [operator.c - funkcje P_mutex oraz V_mutex (zabezpieczenie przed zakleszczeniem)](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L25-L41)
* **Tryb nieblokujący (`IPC_NOWAIT`):** [dron.c - sprawdzanie dostępności miejsca w bazie](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L337)
* **Dynamiczna zmiana limitów (`SETVAL`):** [operator.c - skalowanie bazy w locie](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L286-L289)

### e. Pamięć Współdzielona (Shared Memory)
Główny kanał wymiany danych o stanie roju (tablica struktur).
* **Tworzenie (`shmget`):** [operator.c - alokacja segmentu pamięci](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L194-L200)
* **Dołączanie (`shmat`):** [dron.c - mapowanie pamięci do procesu](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L189-L194)
* **Struktura Danych:** [common.h - definicja struct StanRoju](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/common.h#L72-L80)
* **Usuwanie (`shmctl`):** [operator.c - destrukcja zasobu (IPC_RMID)](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L110-L112)

### f. Kolejki Komunikatów (Message Queues)
Asynchroniczny kanał sterujący na linii Dowódca -> Operator.
* **Tworzenie (`msgget`):** [operator.c - inicjalizacja kolejki](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L186-L192)
* **Wysyłanie (`msgsnd`):** [dowodca.c - wysłanie rozkazu rozbudowy/redukcji](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dowodca.c#L61-L77)
* **Odbieranie (`msgrcv`):** [operator.c - nieblokujący odbiór komunikatów](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/operator.c#L250)

### i. Kluczowe algorytmy i logika
* **Algorytm "Deferred Release" (Operator):** [operator.c - obsługa długu zasobów](https://github.com/dkaminski077/drone-swarm-simulation/blob/fdad8f635634e1eded89e42b6c7ba91aa974605c/operator.c#L337-L342)
    * *Szczegóły:* Operator zapisuje "dług" w zmiennej `platformy_do_usuniecia` zamiast blokować się na semaforze.
* **Algorytm "Deferred Release" (Dron):** [dron.c - spłacanie długu](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L106-L113)
    * *Szczegóły:* Dron przy wylocie sprawdza dług i zamiast oddać zasób (`V`), niszczy go (nie podnosi semafora).
* **Bezpieczna pętla Sleep:** [dron.c - obsługa przerwań](https://github.com/dkaminski077/drone-swarm-simulation/blob/3978e794573b7dc5b7ee292a8b412ec8f51c495a/dron.c#L212-L224)
    * *Szczegóły:* Pętla wznawiająca `sleep` w przypadku przerwania przez sygnał, gwarantująca pełny czas ładowania.
---

## 6. Wnioski

Zrealizowany projekt udowodnił, że wykorzystanie mechanizmów IPC Systemu V w środowisku Linux pozwala na efektywną synchronizację masową (do 25 000 procesów) bez utraty stabilności. Kluczowe wnioski z implementacji:

1.  **Odporność na awarie (Self-Healing):** Zastosowanie flagi `SEM_UNDO` oraz aktywnego monitorowania procesów (`waitpid WNOHANG`) całkowicie wyeliminowało problem procesów zombie oraz "wyciekania" semaforów, nawet w scenariuszach typu *Signal Storm*.
2.  **Skalowalność bez zakleszczeń:** Algorytm "Deferred Release" (Spłata Długu) umożliwił dynamiczną redukcję zasobów bazy w czasie rzeczywistym, rozwiązując klasyczny problem *deadlocka* przy zmniejszaniu puli dostępnych semaforów.
3.  **Wydajność I/O:** Zastąpienie standardowego wyjścia atomowym zapisem do pliku (`O_APPEND`) usunęło wąskie gardło terminala, pozwalając na płynną symulację przy odświeżaniu 10Hz.

System spełnia rygorystyczne wymagania wydajnościowe i zachowuje spójność danych w pamięci współdzielonej niezależnie od obciążenia, co potwierdziły testy scenariuszowe.
