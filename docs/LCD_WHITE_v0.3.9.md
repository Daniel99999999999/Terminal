# Biały LCD po restarcie v0.3.9 — 16.11.2026

**Objaw nadal występuje.** Po zgłoszeniu użytkownika odczytano COM5 bez resetu przez12 s, a następnie wykonano jeden kontrolowany EN przez RTS przy aktywnym zapisie. Użytkownik potwierdził, że po tym resecie ekran nadal jest biały. Nie wgrywano firmware, nie otwierano COM14 i nie wysyłano danych HC-05.

COM5 potwierdził, że na płytce znajduje się już v0.3.9 `esp32dev_touch`, ELF06590579c… — zgodny z przygotowanym wcześniej artefaktem. Poprzedni zapis „płytka nadal v0.3.8” jest od tej obserwacji nieaktualny. Nie ustalamy, kto ani kiedy wykonał upload; nie był on częścią tego odczytu.

## Co rzeczywiście potwierdza log

- Start aplikacji, profil2/touch1, RAM wewnętrzny; PSRAM wyłączony i piny16/17 zarezerwowane. Brak wyjątku lub ponownego restartu w obu przechwyconych odcinkach poza jednym zleconym resetem.
- READY, praca UART1, parsera i zadania ekranu; brak odebranych danych, strat i błędów UART. Wolny RAM144656 B, kalibracja wczytana z NVS.
- API resetu zakończone, następnie149 ms oczekiwania aplikacji (oddzielnie wcześniejsze10 ms upstream po RESX HIGH). Nominalny odstęp po RESX nadal przekracza120 ms. Nie jest to pomiar elektrycznego impulsu RESET na module.
- Zapis SLPOUT w478 ms, dalsza konfiguracja i odczyty od632 ms. Zadana częstotliwość1 MHz podczas inicjalizacji/odczytów i10 MHz podczas rysowania. API transferów nie zgłasza błędu ani timeoutu.
- Log DISPON w978 ms, uruchomienie zegara powitania1031 ms i jego koniec4032 ms. Te wpisy opisują program, **nie potwierdzają obrazu na panelu**.

## Odczyty kontrolera

Poniżej wyłącznie rzeczywiście odebrane bajty (`n`), bez dodatkowych wyzerowanych pól logu:

| Komenda | Po konfiguracji / OFF | Po DISPON / ON |
|---|---|---|
| D3, n4 |00 00 00 00 |54 54 2A 2A |
|09, n5 |FA 29 80 00 00 |54 54 54 2A 2A |
|0A, n2 |00 00 |54 54 |
|0B, n2 |00 00 |2A 2A |
|0C, n2 |00 00 |54 54 |

Wszystkie statusy API są ESP_OK. Nie uzyskano wiarygodnego ID ani spójnego odczytu ustawień; nie można na tej podstawie uznać, że LCD przyjął inicjalizację. Same powtarzające się54/2A nie dowodzą uszkodzenia panelu, przerwanego MISO ani błędu konkretnej linii — mogą wynikać z toru elektrycznego lub sposobu odczytu/niezgodności kontrolera.

**Korekta wcześniejszego audytu diagnostyki:** oznaczenie „dummy included” dla wszystkich odczytów było zbyt ogólne. Tabela poleceń dla interfejsu równoległego nie jest pełnym opisem SPI. [ILI9341 DS §7.1.10, s.38–39](https://files.waveshare.com/upload/e/e3/ILI9341_DS.pdf#page=38) pokazuje dla SPI jednobajtowe0A/0B/0C bez dodatkowego bajtu pustego, natomiast wielobajtowe04/09 z dodatkowym taktem zegara. v0.3.9 loguje surowe dane i niczego automatycznie nie dekoduje, ale dodatkowe bajty nie powinny być interpretowane jako dowód poprawności odczytu. To ograniczenie diagnostyki, nie ustalona przyczyna białego ekranu. Rozszerzone D3 wymaga oddzielnego traktowania; indeksowane odczyty przezD9 stosują [Adafruit](https://github.com/adafruit/Adafruit_ILI9341/blob/master/Adafruit_ILI9341.cpp) i [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI/blob/master/TFT_eSPI.cpp).

Sprawdzony lokalny transport ESP-IDF6.0.1 utrzymuje CS pomiędzy komendą odczytu a fazą danych (`SPI_TRANS_CS_KEEP_ACTIVE`); nie stwierdzono w nim dawnego błędu z odpuszczaniem CS opisanego w [zgłoszeniu Espressif8652](https://github.com/espressif/esp-idf/issues/8652). Sam czasowy model testowy nie weryfikuje rzeczywistych przebiegów SPI ani znaczenia odpowiedzi klona.

## Co pozostaje do rozdzielenia

Nowe odstępy w v0.3.9 nie wystarczyły w tej próbie. Brak podstaw do dalszego zwiększania opóźnień bez sprawdzenia sygnałów. Nie powracamy do diagnozy PSRAM ani wbudowanego BT; aplikacja działa w wariancie internal.

Użytkownik został poproszony o napięcia **VCC, RESET i CS na złączu samego TFT względem jego GND, przy białym ekranie, bez kolejnego resetu**. Wyniki są potrzebne do rozdzielenia zasilania/stale aktywnego resetu od problemu transmisji; nie zgadujemy wartości pomiarów. Sam wysoki poziom RESET/CS nie potwierdzi impulsów ani poprawnych pakietów — przy prawidłowych napięciach potrzebne będą wiarygodne odczyty SPI lub przebiegi RESET/CS/DC/SCK/MOSI.

Sprawdzono również [potwierdzony przez autora M5GFX problem inicjalizacji padów w IDF6](https://github.com/m5stack/M5GFX/issues/192): tam SPI zachowywało wcześniejszy tryb open-drain po detekcji płytki. Nasz kod nie wykonuje tej detekcji i nie ustawia OD na SCK/MOSI, więc nie przenosimy tamtej diagnozy na tę płytkę. Zrzut rzeczywistej konfiguracji GPIO przed/po SPI byłby użyteczną diagnostyką ewentualnej kolejnej wersji.

Surowe lokalne zapisy: `.pio/audit/2026-11-16_white_screen_passive_com5.txt` oraz `.pio/audit/2026-11-16_white_screen_reset_com5.txt`. COM5 zamknięty po obu odczytach. Brak nowej zmiany kodu i brak deklaracji naprawy.
