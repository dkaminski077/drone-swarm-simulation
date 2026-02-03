# Raport z Testów Obciążeniowych i Stabilności (v2.0)

**Autor:** Dawid Kamiński (155272)
**Data:** 03.02.2026
**Wersja Systemu:** 2.0 (High Performance / Hybrid Reduction)
**Cel:** Weryfikacja poprawności zaimplementowanych mechanizmów IPC oraz procedur awaryjnych w kodzie źródłowym projektu "Rój Dronów".

---

## 1. Środowisko i Metodologia
Testy przeprowadzono w środowisku Linux, wykorzystując narzędzia systemowe do monitorowania zasobów oraz inżynierii chaosu.

* **Limit procesów:** 5000+
* **Kluczowe mechanizmy:** SIGUSR1 (Atak), SIGTERM (Redukcja), Semafory System V
* **Narzędzia weryfikacji:** `ipcs`, `ps`, `killall`, `logi.txt`

---

## 2. Scenariusze Testowe

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

## 3. Podsumowanie
Przeprowadzone testy obciążeniowe potwierdzają, że system „Rój Dronów” w wersji 2.0 (High Performance) jest w pełni stabilny i przygotowany do pracy w warunkach ekstremalnych.

1. Odporność na Chaos: Zastosowanie waitpid z flagą WNOHANG oraz funkcji sprzątającej sloty pozwala systemowi na odzyskanie 100% spójności po nagłym zakończeniu procesów sygnałem SIGKILL.
2. Inteligentne Skalowanie: Mechanizm Deferred Release (spłata długu platform) skutecznie rozwiązuje problem synchronizacji zasobów w momencie redukcji bazy, eliminując ryzyko wyścigu (race conditions).
3. Wydajność IPC: System bezbłędnie zarządza kolejkami semaforowymi nawet przy ekstremalnym współbieżnym dostępie (ponad 1100 procesów oczekujących w kolejce do bramki), nie wykazując tendencji do zakleszczeń (deadlocks).
