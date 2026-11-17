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

Użytkownik podał pomiary przy białym ekranie, bez kolejnego resetu:

| Miejsce pomiaru | Wynik użytkownika |
|---|---:|
| TFT VCC względem TFT GND |3,243 V |
| TFT RESET względem TFT GND |3,258 V |
| TFT CS względem TFT GND |−24,8 mV |
| GPIO21 na ESP32 względem ESP32 GND |−24 mV |

VCC i zwolniony RESET mają prawidłowy nominalny poziom3,3 V. Pomiar DC nie wyklucza krótkich spadków napięcia ani błędnego impulsu resetu. CS i GPIO21 są natomiast bliskie0 V. W naszym transporcie aktywne LOW jest zachowywane tylko pomiędzy komendą a parametrami/danymi; ostatnia część transferu nie ma KEEP_ACTIVE. Przy nieruchomym terminalu w spoczynku oczekujemy HIGH. Mruganie kursora nie powinno dawać średniej bliskiej0 V: pojedynczy pasek jest wysyłany tylko przez małą część czasu. To konkretna niezgodność stanu linii z oczekiwanym zachowaniem, lecz jeszcze nie dowód zwarcia albo błędu peryferium.

Po wyłączeniu zasilania i odłączeniu wyłącznie przewodu GPIO21↔CS LCD użytkownik zmierzył nieobciążone GPIO21 po ponownym starcie/5 s: **0 V**. Zgłoszony niski poziom pozostaje więc również bez tego przewodu. Nie przesądza to o zwarciu, uszkodzeniu GPIO ani błędzie konfiguracji SPI.

### Próba GPIO przez bootloader ROM

Bez zapisu Flash i bez ładowania stuba przełączono ESP32 przez COM5 do bootloadera ROM. Zmieniono wyłącznie rejestry dotyczące GPIO21: zwykłe wyjście push-pull, HIGH, wejście padu włączone, bez pull-up/pull-down. Przewód LCD CS pozostaje odłączony według ostatniej procedury użytkownika. Readback: `GPIO_OUT[21]=1`, `GPIO_ENABLE[21]=1`, **`GPIO_IN[21]=1`**, matrix `0x500`, mux `0x2a00`, pin `0x0`. To odczyt cyfrowy wewnątrz ESP32 podczas próby, nie pomiar napięcia na złączu płytki.

Pierwsza próba miała limit180 s i automatycznie zakończyła się resetem do aplikacji. Użytkownik odpowiedział „jakieś mili Volty”, ale nie zapisano momentu przyłożenia sondy względem końca próby. **Nie wolno zestawiać tych wyników jako pewnej równoczesnej sprzeczności.** Nie potwierdzają ani sprawności wyprowadzenia na PCB, ani jego uszkodzenia.

Powtórka rozpoczęta16.11.2026 o20:35:00 UTC (21:35:00 CET) miała limit900 s, zapis czasu ustawienia i kontrolny odczyt wejścia co15 s. W trakcie aktywnego wymuszenia użytkownik podał **GPIO21=5 mV**, a następnie kontrolnie **3V3=3,257 V** z tą samą masą i zakresem napięcia stałego. Odczyt wejścia pozostawał HIGH przed i po zgłoszeniu5 mV. Tym razem nie zachodzi wcześniejsza niepewność, czy próba już wygasła. Nie jest to analogowy pomiar napięcia padu przez ESP32: zestawiamy cyfrowy readback z pomiarem użytkownika na wyprowadzeniu.

O20:43:08 UTC wykonano dodatkowo LOW→HIGH. W obu stanach wejście podążyło za wyjściem: `requested/input/latch/enable=0/0/0/1`, następnie `1/1/1/1`. Miernik nie obserwował osobno tej krótkiej zmiany; potwierdza ona wyłącznie cyfrowe odczyty. Następnie poleceniem `reset` przywrócono start aplikacji i zamknięto COM5; zakończenie20:43:08,628 UTC potwierdzone przez helper. Flash nie był zapisywany.

**Wniosek ograniczony:** zasilanie i odniesienie pomiaru zostały sprawdzone, ale pozostaje rozbieżność pomiędzy wejściem GPIO21 w układzie a napięciem zgłoszonym na wyprowadzeniu PCB. Kolejny krok to identyfikacja dokładnego punktu pomiarowego i sprawdzenie jego połączenia z GPIO21. Możliwe przyczyny obejmują inny punkt pomiarowy, kontakt sondy/złącza lub przerwę połączenia; nie wykluczono usterki sprzętowej. Nie rozpoznano uszkodzenia GPIO, błędu SPI ani przyczyny białego LCD. Nie zmieniać jeszcze pinoutu i nie wgrywać kolejnej poprawki na podstawie samej tej rozbieżności. Przewód LCD CS nadal odłączony według ostatniego potwierdzonego stanu.

Lokalne artefakty: `.pio/audit/gpio21_rom_result.json` (pierwsza próba), `.pio/audit/gpio21_rom_result_v2.json` (powtórka). Firmware nadal v0.3.9; brak nowego uploadu.

### Doprecyzowanie17.11.2026 — bezpośrednio goldpin P21

Użytkownik potwierdził pomiar napięcia **bezpośrednio na goldpinie P21**. Nie chodzi o odłączony koniec przewodu CS. Ponownie sprawdzono stałe helpera z rzeczywistymi nagłówkami SDK:10 adresów OUT/SET/CLEAR/ENABLE/EN_SET/EN_CLEAR/IN/PIN21/MATRIX21/MUX21, obie bazy, funkcja GPIO21=2 i FUN_IE bit9 są zgodne. Sprawdzono również sygnatury `esptool.read_reg(addr)` oraz `write_reg(addr,value,mask)`; helper używa ich w prawidłowej kolejności. Nie jest to niezależne potwierdzenie napięcia na PCB ani kompletnej sprawności GPIO. Nie otwierano COM5/COM14.

Zlecono dwa pomiary rezystancji przy całkowicie odłączonym zasilaniu ESP32 i LCD, także USB, z przewodem CS nadal odłączonym: goldpin P21↔jego własne pole lutownicze na spodzie PCB oraz P21↔GND ESP32. Wyniki oczekiwane. Połączenie goldpinu z własnym polem powinno mieć rezystancję zbliżoną do zwarcia sond. Sam taki wynik nie sprawdzi ciągłości dalszej ścieżki do modułu. Nie zalecano lutowania ani wymiany GPIO.

Sprawdzono również [potwierdzony przez autora M5GFX problem inicjalizacji padów w IDF6](https://github.com/m5stack/M5GFX/issues/192): tam SPI zachowywało wcześniejszy tryb open-drain po detekcji płytki. Nasz kod nie wykonuje tej detekcji i nie ustawia OD na SCK/MOSI, więc nie przenosimy tamtej diagnozy na tę płytkę. Zrzut rzeczywistej konfiguracji GPIO przed/po SPI byłby użyteczną diagnostyką ewentualnej kolejnej wersji.

Surowe lokalne zapisy: `.pio/audit/2026-11-16_white_screen_passive_com5.txt` oraz `.pio/audit/2026-11-16_white_screen_reset_com5.txt`. COM5 zamknięty po obu odczytach. Brak nowej zmiany kodu i brak deklaracji naprawy.
