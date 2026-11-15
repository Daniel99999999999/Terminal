# Krótkie transakcje XPT2046 — v0.3.4

v0.3.1 przyjęła rzeczywiste punkty kalibracji. W v0.3.3 podczas trzech przytrzymań GPIO36 reagowało (896 odpytań LOW), lecz wszystkie obserwowane X/Y/Z1/Z2 były zerowe, valid0. [Próby porównawcze](TFT_TOUCH_TEST_2026-11-15.md). Nie potwierdzono przyczyny; wycofujemy zmianę formatu, która nie dała potwierdzonej poprawy na tym sprzęcie.

Sterownik ponownie wysyła osobne24-taktowe ramki z `SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA`, jak w v0.3.1. Kolejność B0,C0,D0,D0,D0,D0,90,90,90,90: Z1/Z2, odrzucony i trzy pomiary każdej osi. Każda ramka ma PD0=0, kończy konwersję i przywraca PENIRQ. Mediana, progi,1 MHz, mapowanie osi/pinów i NVS są zachowane. Nie łączymy tego z rozstrzygnięciem przyczyny bielenia LCD, którego ponowne wgranie tego samego v0.3.3 nie odtworzyło.

Cały descriptor z buforami inline jest statyczny. Po błędzie nie jest ponownie używany; driver zatrzymuje się do resetu. Timeout20 ms dotyczy pojedynczej konwersji; pełen odczyt to najwyżej10 ramek (sumarycznie do200 ms oczekiwania w najgorszym przypadku, błąd przerywa natychmiast). Odbiór UART jest osobnym zadaniem i nie dodano pętli opóźniających komunikację.

Odczyt nadal nie wymaga IRQ przy RETRO_TOUCH_REQUIRE_IRQ=0; opcja1 zachowuje bramkowanie. TOUCH_RAW nadal podaje obie elektrody nacisku, mediany, stan IRQ i powody odrzucenia. Filtr UI z tolerancją krótkich braków próbki pozostaje.

Kontrole: oba warianty IRQ, kompilacja internal/touch, Cppcheck, stabilny start i surowe ADC na sprzęcie. Najpierw próba bez nacisku, potem rzeczywisty nacisk i sterowanie/calibracja; sam build/API nie dowodzi odpowiedzi kontrolera. Kolejność publikacji: udana kompilacja → commit/push → testy/upload → osobny zapis wyników.

## Wyniki po publikacji 541035d

Oba buildy SUCCESS; testy protokołu z REQUIRE_IRQ0/1 PASS; Cppcheck11 C w każdym profilu bez zgłoszeń. Wgrano profil touch, start stabilny15 s, mapa NVS odtworzona. Trzy przytrzymania użytkownika dały356 zaakceptowanych próbek przy620 odpytywaniach LOW; nadal są słabe/odrzucone próbki. UART przy aktywnym dotyku:17894 B z dokładnym CRC40cad4e7, bez strat/błędów. [Wyniki](logs/2026-11-15_v0.3.4_validation.json), [nacisk](logs/2026-11-15_v0.3.4_touch_press_watch.txt), [UART](logs/2026-11-15_v0.3.4_touch_uart_result.json).

Próba CAL→pięć punktów→walidacja→GOTOWE została zlecona użytkownikowi, wynik oczekuje. Nie deklarujemy jeszcze stabilnej obsługi gestów, dokładnej kalibracji ani potwierdzenia obrazu v0.3.4; odpowiedź użytkownika na próbę nacisku brzmiała wyłącznie „zrobione”.
