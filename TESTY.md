# Dokumentacja Scenariuszy Testowych - Projekt: Rój Dronów

**Autor:** Dawid Kamiński (155272)   
**Data:** 2026-01-27  

---

## 1. Wstęp
Niniejszy dokument opisuje procedury weryfikacyjne dla systemu symulacji wieloprocesowej. Testy obejmują sprawdzanie synchronizacji (semafory), komunikacji IPC (pamięć dzielona, kolejki) oraz zarządzania cyklem życia procesów.

**Środowisko testowe:**
- System: Linux (obsługa System V IPC)
- Kompilator: GCC
- Narzędzia pomocnicze: `ipcs`, `ps`

---

## 2. Scenariusze Testowe (Test Cases)

### TC-01: Weryfikacja współbieżności i kolejek (Stress Test)
**Cel:** Sprawdzenie zachowania systemu, gdy liczba dronów ($N=12$) przekracza pojemność bazy ($P=4$).
**Typ:** Test obciążeniowy.

| Krok | Akcja Operatora/Użytkownika | Oczekiwany Rezultat Systemowy | Weryfikacja (Logi/System) |
|------|-----------------------------|-------------------------------|---------------------------|
| 1. | Uruchomienie `./operator` | Utworzenie pamięci dzielonej, semaforów i 12 procesów potomnych. | `ipcs -s` pokazuje aktywne semafory. |
| 2. | Brak interakcji (obserwacja) | Drony, które nie zmieściły się w bazie, oczekują na semaforze `SEM_BAZA`. | Log: `[ZOLTY] Baza pełna. Oczekiwanie...` |
| 3. | Długie oczekiwanie | Drony tracą energię w kolejce. Po wyczerpaniu baterii proces jest usuwany. | Log: `[CZERWONY] Bateria 0%. Rozbity w kolejce.` |

**Dowód realizacji:**
```
[2026-01-27 08:18:32][10871]     [DRON 17] PID: 10871. Uruchomiono. Bateria 0%.
[2026-01-27 08:18:34][10871]     [DRON 17] Bateria naładowana (100%). Czekam na wylot.
[2026-01-27 08:18:36][10871]     [DRON 17] Wylot bramką 1...
[2026-01-27 08:18:37][10871]     [DRON 17] Lot w strefie operacyjnej...
[2026-01-27 08:18:43][10871]     [DRON 17] Bateria słaba (10%). Powrót do bazy.
[2026-01-27 08:18:43][10871]     [DRON 17] Zbliżam się do bazy...
[2026-01-27 08:18:43][10871]     [DRON 17] Baza pełna. Oczekiwanie...
[2026-01-27 08:18:43][10871]     [DRON 17] Bateria 0%. Rozbity w kolejce.
```

**Status:** ✅ ZALICZONY

---

### TC-02: Skalowanie Systemu i "Lazy Release"
**Cel:** Weryfikacja algorytmu zapobiegającego zakleszczeniom (Deadlock) przy redukcji zasobów.
**Typ:** Test funkcjonalny.

| Krok | Akcja Operatora/Użytkownika | Oczekiwany Rezultat Systemowy | Weryfikacja (Logi/System) |
|------|-----------------------------|-------------------------------|---------------------------|
| 1. | Dowódca: `[1]` (x3) | Zwiększenie limitu dronów i miejsc w bazie. | Log: `[SYGNAŁ 1] Rozbudowa! Baza: X`. |
| 2. | Dowódca: `[2]` (Redukcja) | Operator zleca redukcję, ale **nie blokuje się** czekając na wolne miejsce. Zapisuje "dług" w pamięci. | Log Operatora: `Czeka na demontaż: X`. |
| 3. | Obserwacja wylotów | Drony przy wylocie z bazy sprawdzają dług i niszczą platformę zamiast ją zwolnić. | Log Drona: `[CZERWONY] Demontaż platformy (Redukcja).` |

**Dowód realizacji:**
```
[2026-01-27 08:25:16][10864] [SYGNAŁ 2] Redukcja! Drony: 20->10. Baza: 5->4
[2026-01-27 08:25:16][10864] [SYGNAŁ 2] Zdemontowano od razu wszystkie 1 platform.
...
[2026-01-27 08:25:18][10864] [SYGNAŁ 2] Redukcja! Drony: 10->5. Baza: 4->2
[2026-01-27 08:25:18][10864] [SYGNAŁ 2] Zdemontowano od razu: 0 platform. Czeka na demontaż: 2.
...
[2026-01-27 08:25:19][10906]     [DRON 9] Demontaż platformy (Redukcja).
[2026-01-27 08:25:21][10899]     [DRON 14] Demontaż platformy (Redukcja).
```

**Status:** ✅ ZALICZONY

---

### TC-03: Priorytetyzacja Rozkazów (Sygnały Asynchroniczne)
**Cel:** Sprawdzenie logiki decyzyjnej drona w odpowiedzi na sygnał `SIGUSR1`.
**Typ:** Test logiczny.

| Krok | Akcja Operatora/Użytkownika | Oczekiwany Rezultat Systemowy | Logika Drona |
|------|-----------------------------|-------------------------------|--------------|
| 1. | Dowódca: `[a]` (Atak) | Dowódca losuje cel i wysyła sygnał `kill(pid, SIGUSR1)`. | - |
| 2. | Przypadek A: Dron Naładowany | Dron przerywa zadanie, zwalnia zasoby i kończy proces. | Log: `!!! ATAK WYKONANY !!!` |
| 3. | Przypadek B: Dron Słaby (<20%) | Dron ignoruje sygnał, aby nie spaść na strefę cywilną. Wraca do bazy. | Log: `[ZOLTY] Atak anulowany - zbyt słaba bateria.` |

**Dowód realizacji:**
```
[2026-01-27 08:29:55][10934]     [DRON 4] !!! OTRZYMAŁEM ROZKAZ ATAKU SAMOBÓJCZEGO !!! (Bateria: 85%).
[2026-01-27 08:29:55][10934]     [DRON 4] !!! ATAK WYKONANY !!!
...
[2026-01-27 08:30:07][10936]     [DRON 3] !!! OTRZYMAŁEM ROZKAZ ATAKU SAMOBÓJCZEGO !!! (Bateria: 10%).
[2026-01-27 08:30:07][10936]     [DRON 3] Atak anulowany - zbyt słaba bateria (<20%).
```

**Status:** ✅ ZALICZONY

---

### TC-04: Graceful Shutdown i Procesy Zombie
**Cel:** Weryfikacja poprawności czyszczenia zasobów i tablicy procesów.
**Typ:** Test bezpieczeństwa.

| Krok | Akcja Operatora/Użytkownika | Oczekiwany Rezultat Systemowy | Weryfikacja (Logi/System) |
|------|-----------------------------|-------------------------------|---------------------------|
| 1. | Praca systemu | Drony cyklicznie kończą żywotność (`MAX_CYKLI`). | Operator w pętli usuwa martwe procesy (`waitpid`). |
| 2. | Operator: `Ctrl+C` | Przechwycenie `SIGINT`. Usunięcie IPC i zabicie wszystkich procesów potomnych. | Wyświetlenie: `KONIEC SYMULACJI`. |
| 3. | Weryfikacja po zamknięciu | System operacyjny jest czysty. | Komenda `pgrep dron` zwraca pusty wynik (brak zombie). |

**Dowód realizacji:**
```
[2026-01-27 08:33:02][10864] [OPERATOR] Pamięć dzielona usunięta.
[2026-01-27 08:33:02][10864] [OPERATOR] Semafory usunięte.
[2026-01-27 08:33:02][10864] [OPERATOR] Kolejka komunikatów usunięta.
[2026-01-27 08:33:02][10864] [OPERATOR] KONIEC SYMULACJI
```

**Status:** ✅ ZALICZONY

---

### TC-05: Integralność Danych i Logów
**Cel:** Sprawdzenie poprawności zapisu do pliku (wymóg 5.2.a - `open/write`).
**Typ:** Weryfikacja danych.

| Krok | Akcja Operatora/Użytkownika | Oczekiwany Rezultat Systemowy | Weryfikacja (Logi/System) |
|------|-----------------------------|-------------------------------|---------------------------|
| 1. | Analiza pliku `logi.txt` | Każdy wpis posiada pełny znacznik czasu ISO 8601 oraz PID. | Format: `[RRRR-MM-DD GG:MM:SS][PID] Treść`. |
| 2. | Weryfikacja wielodostępowa | Wpisy od różnych procesów nie nakładają się na siebie (atomowość zapisu). | Zastosowanie flagi `O_APPEND` działa poprawnie. |

**Dowód realizacji:**
```
[2026-01-27 08:30:10][10864] [OPERATOR] Wykryto brak drona na pozycji 1. Tworzę nowego...
[2026-01-27 08:30:10][10941]     [DRON 1] PID: 10941. Uruchomiono. Bateria 0%.
[2026-01-27 08:30:10][10936]     [DRON 3] Lot w strefie operacyjnej...
[2026-01-27 08:30:10][10938]     [DRON 4] Bateria słaba (10%). Powrót do bazy.
[2026-01-27 08:30:10][10938]     [DRON 4] Zbliżam się do bazy...
[2026-01-27 08:30:10][10938]     [DRON 4] Ląduję bramką 1...
```

**Status:** ✅ ZALICZONY
