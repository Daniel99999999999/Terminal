# Krótkie transakcje XPT2046 — v0.3.4

v0.3.1 przyjęła rzeczywiste punkty kalibracji. W v0.3.3 podczas trzech przytrzymań GPIO36 reagowało (896 odpytań LOW), lecz wszystkie obserwowane X/Y/Z1/Z2 były zerowe, valid0. [Próby porównawcze](TFT_TOUCH_TEST_2026-11-15.md). Nie potwierdzono przyczyny; wycofujemy zmianę formatu, która nie dała potwierdzonej poprawy na tym sprzęcie.

Sterownik ponownie wysyła osobne24-taktowe ramki z `SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA`, jak w v0.3.1. Kolejność B0,C0,D0,D0,D0,D0,90,90,90,90: Z1/Z2, odrzucony i trzy pomiary każdej osi. Każda ramka ma PD0=0, kończy konwersję i przywraca PENIRQ. Mediana, progi,1 MHz, mapowanie osi/pinów i NVS są zachowane. Nie łączymy tego z rozstrzygnięciem przyczyny bielenia LCD, którego ponowne wgranie tego samego v0.3.3 nie odtworzyło.

Cały descriptor z buforami inline jest statyczny. Po błędzie nie jest ponownie używany; driver zatrzymuje się do resetu. Timeout20 ms dotyczy pojedynczej konwersji; pełen odczyt to najwyżej10 ramek (sumarycznie do200 ms oczekiwania w najgorszym przypadku, błąd przerywa natychmiast). Odbiór UART jest osobnym zadaniem i nie dodano pętli opóźniających komunikację.

Odczyt nadal nie wymaga IRQ przy RETRO_TOUCH_REQUIRE_IRQ=0; opcja1 zachowuje bramkowanie. TOUCH_RAW nadal podaje obie elektrody nacisku, mediany, stan IRQ i powody odrzucenia. Filtr UI z tolerancją krótkich braków próbki pozostaje.

Kontrole: oba warianty IRQ, kompilacja internal/touch, Cppcheck, stabilny start i surowe ADC na sprzęcie. Najpierw próba bez nacisku, potem rzeczywisty nacisk i sterowanie/calibracja; sam build/API nie dowodzi odpowiedzi kontrolera. Kolejność publikacji: udana kompilacja → commit/push → testy/upload → osobny zapis wyników.
