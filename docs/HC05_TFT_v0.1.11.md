# HC-05 i TFT — v0.1.11

## Bieżący kod

- HC-05 ZS-040: UART1 TX32/RX34, **115200 8N1**, zgodnie z potwierdzeniem użytkownika. UART2 TX25/RX26 pozostaje 115200 8N1. UART0 przez USB i debug LED GPIO2 zachowane, GPIO16/17 zajęte przez PSRAM.
- Dwa zadania RX i dwa TX, osobne kolejki po 32×128 B i bufory RX po 4096 B. RX nie czeka na wolne miejsce w kolejce. TX używa `uart_tx_chars()` bez bufora TX sterownika, a na opróżnienie FIFO czeka z kontrolowanym timeoutem. Parametry FIFO i bajty są raportowane co 5 s. Nie wstrzykujemy echa ani komunikatów do mostu.
- UART2 RX niezależnie przekazuje kopię do parsera/modelu terminala. Komendy ANSI i UTF-8 nie są modyfikowane w strumieniu mostu. Autobaud pozostaje niewłączony.
- Natywne BT/SPP, GAP/HCI i inicjalizacja NVS zostały usunięte z bieżącego `src/`. Usunięto wrappery HCI i zależność od kontrolera BT. `MINIMAL_BUILD` w ESP-IDF wybiera tylko zależności aplikacji; docelowy firmware nie zawiera komponentów BT/Wi-Fi. `main.c` odrzuca konfigurację z włączonymi radiami.
- Archiwalne źródła i konfiguracja są w `baseline/v0.1.10/`, starszych bazach i tagu Git v0.1.10. Historyczna biblioteka BT i adapter CMake pozostały jako zasoby powrotu; bieżący projekt ich nie używa. Osobny projekt `diagnostics/` również jest archiwalny i nie jest częścią nowego firmware. Nie wykonujemy nowych prób natywnego BT.

## Pierwsze cztery przewody HC-05

RXD → GPIO32, TXD → GPIO34, GND → GND, VCC nośnika ZS-040 → właściwe 5 V. Poziomy sygnałów 3,3 V; nie łączyć 5 V z wejściami ESP32. [Pełna tabela i warunki elektryczne](PIN_ASSIGNMENT_HC05_TFT.md).

Domyślnie **STATE i EN są wyłączone**, więc GPIO35 i GPIO12 pozostają niekonfigurowane. Brak STATE oznacza **nieznany stan BT**, nie rozłączenie rozpoznane z braku ruchu. Most nadal przekazuje dane do UART HC-05. Licznik `TX_fifo` potwierdza wpis do lokalnego UART, nie dostarczenie radiowe do telefonu. Bez RTS/CTS i informacji STATE nie można zagwarantować bezstratnej transmisji podczas rozłączenia lub przeciążenia HC-05.

## Opcjonalny STATE i EN

Opcje są dostępne przez **PIO: Menuconfig → RetroTerminal** (`pio run -e esp32dev -t menuconfig`).

| Opcja | Domyślnie | Działanie |
|---|---|---|
| `RETRO_HC05_BAUD` | 115200 | Baud transmisji UART1; UART2 pozostaje 115200 |
| `RETRO_HC05_STATE_ENABLE` | OFF | Po sprawdzeniu STATE/GPIO35 włącza informację o połączeniu |
| `RETRO_HC05_STATE_ACTIVE_HIGH` | ON po włączeniu STATE | HIGH oznacza aktywne połączenie |
| `RETRO_HC05_KEY_ENABLE` | OFF | Po zabezpieczeniu strapa GPIO12 pozwala sterować EN/KEY |
| `RETRO_HC05_AT_BAUD` | 38400 | Konfigurowalny baud AT, wymaga potwierdzenia na module |

STATE: połączenie uznajemy po HIGH utrzymanym 50 ms, LOW blokuje przekazywanie natychmiast po odczycie w zadaniu nadzorczym (odczyt nominalnie co 50 ms). Każda zmiana sesji unieważnia stare pakiety. Bajty zastane w RX przy nowej sesji nie trafiają do nowego odbiorcy; kopia UART2 nadal trafia do ekranu. Dlatego pierwszy test transmisji należy rozpocząć po komunikacie `STATE status=2`. Bajtów już umieszczonych w FIFO nadajnika nie można wycofać.

LED GPIO2 miga co 500 ms bez potwierdzonego połączenia lub przy stanie nieznanym, co 1000 ms przy STATE=connected. Błędy i wykryty brak heartbeat zatrzaskują ON. Dodatkowa niebieska LED może korzystać ze STATE przez bufor/tranzystor, bez kolejnego GPIO.

Użytkownik sprawdził AT przez EN swojego ZS-040. **Przed połączeniem EN z GPIO12 trzeba zachować fabryczny strap VDD_SDIO i odseparować ewentualny wpływ przycisku/nośnika.** Samo ustawienie programu nie zabezpiecza fazy resetu. Kod nie steruje zasilaniem HC-05 i nie zapewnia automatycznego cyklu zasilania wymaganego przez niektóre tryby AT.

API w `hc05_bridge.h`:

- `hc05_bridge_request_at(true/false)` — kopiuje żądanie do kolejki. Zadanie zatrzymuje most, czeka do 200 ms na opróżnienie UART1, ustawia KEY i zmienia baud **tylko UART1**. Nie potwierdza odpowiedzi modułu. W razie błędu pozostaje bezpiecznie w trybie wstrzymania, LED ON.
- `hc05_bridge_at_send()` — kopiuje do 128 B, bez czekania. Komenda musi zawierać wymagane CR/LF. Podczas przełączania zwraca `ESP_ERR_INVALID_STATE`.
- `hc05_bridge_at_receive()` — nieblokujący odczyt porcji odpowiedzi, bufor odbiorcy co najmniej 128 B. Wywołujący odpowiada za złożenie odpowiedzi z porcji i limit czasu.

To API dla przyszłego UI/konfiguratora, nie zaimplementowany jeszcze ekran konfiguracji ani konsola komend. Nie zmieniamy automatycznie nazwy, PIN ani nastaw HC-05.

## TFT: ILI9341-compatible, 8-bit I80

Sterownik ILI9341 wybrano według opisu użytkownika; nie odczytano ID kontrolera. Używamy oficjalnego sterownika Espressif **2.1.0**, przypiętego do commita w `vendor/esp_lcd_ili9341/UPSTREAM.json`, wraz z licencją Apache-2.0. Źródło i nagłówek upstream pozostają niezmienione. Lokalny CMake dostarcza jego trzy definicje wersji bez pobierania komponentów podczas builda.

Magistrala: `esp_lcd` I80, na klasycznym ESP32 backend I2S. D0–D7=13/14/18/19/21/22/23/27, WR33, DC15, CS5, RD0, RST4. **RD pozostaje HIGH; nie wykonujemy automatycznego odczytu przez niezweryfikowane HC245.** Włączenie TFT zależy od pomiarów zasilania, poziomów i kierunku buforów. CS musi odłączać LCD także przy BOOT=LOW/RD=LOW.

| Opcja | Domyślnie | Działanie |
|---|---|---|
| `RETRO_TFT_ENABLE` | **OFF** | ON dopiero po pomiarach; przy OFF żaden GPIO TFT nie jest konfigurowany |
| `RETRO_TFT_ROTATION` | 1 | 0/2: 240×320; 1/3: 320×240 |
| `RETRO_TFT_CLOCK_HZ` | 1000000 | Początkowy zegar WR 1 MHz |
| `RETRO_TFT_BGR` | ON | Kolejność elementów koloru, do sprawdzenia wzorem |
| `RETRO_TFT_INVERT` | OFF | Korekta dla wariantów panelu wymagających inwersji |
| `RETRO_TFT_COLOR_TEST` | OFF | Po włączeniu wzór RGB/biały/czarny przez 2 s przed terminalem |

Renderer ma osobne zadanie. Kopiuje wiersz modelu bez czekania na zajęty mutex, rysuje po zwolnieniu blokady. Model i kopia ostatniego ekranu są w PSRAM. Pasek 8 pikseli wysokości jest w **INTERNAL DMA RAM**: 5120 B dla 320×240. Bufor jest ponownie używany dopiero po callbacku DMA, z limitem 250 ms. Błąd zatrzymuje renderer i zachowuje bufory, aby nie zwolnić pamięci nadal używanej przez DMA; most działa dalej, LED sygnalizuje błąd.

SDK 6.0.1 używa krótkich operacji polling dla komend I80 i wewnętrznie ma oczekiwania bez limitu; nie deklarujemy pełnej odporności sterownika SDK na awarię DMA. Operacje są odseparowane od zadań UART, a zadanie TFT ma heartbeat. Opóźnienia resetu/inicjalizacji LCD i zatrzymanie wzoru testowego nie są pętlami oczekiwania komunikacji.

Font 8×8: ASCII, Latin-1, linie/ramki DEC, podstawowe symbole i polskie litery (część jako małe kompozycje). Pozostałe glify: `?`. Kolory RGB565, bold, underline, inverse, blink i kursor; brak dotyku, SD, regulacji jasności i scrollback. Zmieniają się wiersze różniące się od ostatniej kopii oraz wiersze kursora/blink. Widok poziomy: **40×29 znaków + dolny pasek statusu z ikoną BT**; pionowy: 30×39. Przy wyłączonym TFT model zachowuje 80×24.

## FabGL

FabGL ma emulator terminala i obsługę ESP-IDF, ale jego gotowy ILI9341 jest **SPI**, a nasz shield ma **równoległe I80 8-bit**. Biblioteka jest w C++; obecny projekt jest w C i ma już testowany parser/model. Nie dodajemy FabGL ani Arduino. Użycie FabGL wymagałoby portu magistrali/wyświetlania i sprawdzenia zgodności z ESP-IDF 6.0.1; nie wykonywano takiego testu.

Źródła autora: [TFTControllerGeneric](https://github.com/fdivitto/FabGL/blob/master/src/dispdrivers/TFTControllerGeneric.h), [przykład ILI9341 SimpleTerminalOut](https://github.com/fdivitto/FabGL/blob/master/examples/ILI9341_TFT/240x320/SimpleTerminalOut/SimpleTerminalOut.ino), [CMake ESP-IDF](https://github.com/fdivitto/FabGL/blob/master/CMakeLists.txt).

## Weryfikacja i sprzęt

Zgodnie z poleceniem użytkownika źródła v0.1.11 wysłano w [46ea0fa](https://github.com/Daniel99999999999/Terminal/commit/46ea0fae02ec845b2c58fccabec2426289e6fb40) **przed końcowymi testami**. Następnie przeszły wszystkie zestawy hosta, build domyślny i osobny build z TFT/STATE/KEY/wzorem/inwersją, a także Cppcheck 10 C w obu konfiguracjach. Komponenty, mapy linkowania i ELF potwierdzają brak natywnego radia. Wyniki zapisano osobnym commitem w [STATUS](STATUS.md) i [TESTS](TESTS.md). Domyślne TFT/STATE/KEY pozostają OFF.

Nie wykonano uploadu, próby na TFT ani fizycznego testu UART1↔HC-05. Następny test sprzętu: UART1↔UART2 krótkie/długie dane i błędy; po pomiarach LCD wzór kolorów, obrót, ANSI/UTF-8, start i upload USB z LCD; potem STATE, granice sesji oraz AT/EN.
