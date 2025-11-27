# Temat 5 – Rój dronów

**Autor:** Dawid Kamiński  
**Nr albumu:** 155272 
**Link do repozytorium:** https://github.com/dkaminski077/drone-swarm-simulation

---

## 1. Opis zadania

Celem projektu jest zaprojektowanie i implementacja symulacji roju autonomicznych dronów w środowisku systemu operacyjnego Linux. Projekt modeluje problem współbieżnego dostępu do ograniczonych zasobów (baza, wejścia do bazy) przez wiele niezależnych jednostek.

### Model systemu:
System zostanie zrealizowany w architekturze **wieloprocesowej**, gdzie poszczególni aktorzy (Drony, Operator, Dowódca) będą reprezentowani przez osobne procesy systemowe.

Główne założenia logiczne symulacji wynikają z treści zadania:
1.  **Zarządzanie rojem:** System inicjuje $N$ procesów dronów. Baza posiada ograniczoną pojemność $P$ (gdzie $P < N/2$) oraz dwa jednokierunkowe wejścia (wąskie gardła).
2.  **Cykl pracy drona:** Każdy proces drona realizuje pętlę zdarzeń: start, lot (symulacja zużycia energii), powrót (przy 20% baterii), ładowanie i ponowny start. Wyczerpanie baterii do 0% w trakcie oczekiwania na zasób (wejście do bazy) skutkuje terminacją procesu (zniszczeniem).
3.  **Role zarządcze:**
    * **Operator:** Monitoruje liczebność roju i w określonych odstępach czasu ($T_k$) powołuje nowe procesy w miejsce utraconych lub zutylizowanych jednostek, respektując limity pojemności bazy.
    * **Dowódca:** Interweniuje w działanie systemu poprzez asynchroniczne sygnały sterujące.

### Sygnały sterujące:
System będzie obsługiwał komunikaty wysyłane przez Dowódcę, implementujące następujące funkcjonalności:
* Dynamiczna zmiana pojemności bazy (dołożenie lub demontaż platform startowych).
* Wymuszenie ataku samobójczego na wybranym dronie (z uwzględnieniem warunku minimalnego poziomu naładowania).

---

## 2. Architektura i środowisko uruchomieniowe

Projekt zostanie wykonany w języku C z wykorzystaniem API systemu Linux.

### Planowane rozwiązania techniczne:
Aby zrealizować założenia projektowe, zostaną wykorzystane standardowe mechanizmy systemowe:

* **Zarządzanie procesami:** Tworzenie i kontrola cyklu życia procesów potomnych (reprezentujących drony) przez proces macierzysty.
* **Synchronizacja procesów:** Wykorzystanie mechanizmów synchronizacji (np. semafory) do rozwiązania problemu dostępu do sekcji krytycznych (wejścia do bazy) oraz kontroli liczby zajętych miejsc w bazie.
* **Komunikacja międzyprocesowa (IPC):** Zastosowanie mechanizmów IPC (np. pamięć dzielona, kolejki komunikatów lub potoki) do wymiany danych o stanie roju (poziom baterii, status) między dronami a procesami zarządzającymi.
* **Obsługa plików:** Raportowanie przebiegu symulacji do pliku tekstowego z zapewnieniem spójności danych przy jednoczesnym dostępie wielu procesów.

---

## 3. Scenariusze testowe

Poprawność działania systemu zostanie zweryfikowana poprzez serię testów sprawdzających stabilność synchronizacji oraz reakcję na zdarzenia losowe i sterujące.

### Test 1: Weryfikacja współbieżności i wąskich gardeł
* **Cel:** Sprawdzenie poprawności obsługi kolejki do bazy.
* **Scenariusz:** Uruchomienie symulacji z dużą liczbą dronów ($N$) przy małej pojemności bazy ($P$) i długim czasie ładowania.
* **Oczekiwany rezultat:** Obserwacja blokowania się procesów przed wejściem do bazy. System powinien poprawnie odnotować zniszczenie dronów, które nie zdążyły otrzymać dostępu do zasobu przed wyczerpaniem energii.

### Test 2: Skalowanie systemu (Sygnały Dowódcy)
* **Cel:** Weryfikacja dynamicznej zmiany parametrów symulacji.
* **Scenariusz:** Wysłanie sygnałów zwiększających oraz zmniejszających limit miejsc w bazie w trakcie pełnego obciążenia systemu.
* **Oczekiwany rezultat:** Operator powinien odpowiednio wznowić lub wstrzymać tworzenie nowych procesów, dostosowując liczebność roju do nowego limitu.

### Test 3: Priorytety i obsługa rozkazów (Atak)
* **Cel:** Sprawdzenie logiki decyzyjnej dronów.
* **Scenariusz:** Wydanie rozkazu ataku dla drona naładowanego oraz drona z baterią na wyczerpaniu.
* **Oczekiwany rezultat:** Dron naładowany powinien natychmiast zakończyć działanie (status: atak), natomiast dron z niskim poziomem baterii powinien zignorować rozkaz i kontynuować procedurę powrotu do bazy.

### Test 4: Cykl życia i regeneracja roju
* **Cel:** Długotrwała stabilność symulacji.
* **Scenariusz:** Ustawienie krótkiego czasu eksploatacji drona ($X_i$).
* **Oczekiwany rezultat:** Procesy powinny kończyć się naturalnie po osiągnięciu limitu cykli. Operator powinien wykrywać ubytek procesów i sukcesywnie uruchamiać nowe instancje, utrzymując ciągłość działania roju bez wycieków pamięci lub zasobów.

  
