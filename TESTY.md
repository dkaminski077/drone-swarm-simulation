# Dokumentacja Scenariuszy Testowych - Projekt: Rój Dronów

**Autor:** Dawid Kamiński (155272)   
**Data:** 2026-01-12  

---

## 1. Wstęp
Niniejszy dokument opisuje procedury weryfikacyjne dla systemu symulacji wieloprocesowej. Testy obejmują sprawdzanie synchronizacji (semafory), komunikacji IPC (pamięć dzielona, kolejki) oraz zarządzania cyklem życia procesów.

**Środowisko testowe:**
- System: Linux (obsługa System V IPC)
- Kompilator: GCC
- Narzędzia pomocnicze: `ipcs`, `ps`, `htop`

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

**Status:** ✅ ZALICZONY

---

### TC-05: Integralność Danych i Logów
**Cel:** Sprawdzenie poprawności zapisu do pliku (wymóg 5.2.a - `open/write`).
**Typ:** Weryfikacja danych.

| Krok | Akcja Operatora/Użytkownika | Oczekiwany Rezultat Systemowy | Weryfikacja (Logi/System) |
|------|-----------------------------|-------------------------------|---------------------------|
| 1. | Analiza pliku `logi.txt` | Każdy wpis posiada pełny znacznik czasu ISO 8601 oraz PID. | Format: `[RRRR-MM-DD GG:MM:SS][PID] Treść`. |
| 2. | Weryfikacja wielodostępowa | Wpisy od różnych procesów nie nakładają się na siebie (atomowość zapisu). | Zastosowanie flagi `O_APPEND` działa poprawnie. |

**Status:** ✅ ZALICZONY
