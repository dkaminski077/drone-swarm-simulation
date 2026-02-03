#!/bin/bash
# test_chaos.sh

echo "--- START EKSTREMALNEGO TESTU ---"

for i in {1..3}
do
    echo "Cykl $i: Generowanie kolizji..."

    # 1. Zabijanie procesów w tle, aby operator zaczął sprzątać (SIGKILL)
    pgrep dron | shuf -n 200 | xargs kill -9 2>/dev/null &

    # 2. Wysłanie rozkazu ROZBUDOWY (Opcja 1 + wyjście q)
    echo -e "1\nq" | ./dowodca > /dev/null &
    
    # 3. Wysłanie rozkazu REDUKCJI (Opcja 2 + wyjście q)
    echo -e "2\nq" | ./dowodca > /dev/null &

    # Krótka pauza na przetworzenie sygnałów
    sleep 1
    
    # Sprawdzenie czy żyje operator
    if ! pgrep -x "operator" > /dev/null; then
        echo "BŁĄD: Operator padł pod obciążeniem!"
        exit 1
    fi
done

echo "--- TEST ZAKOŃCZONY. SPRAWDŹ logi.txt ---"