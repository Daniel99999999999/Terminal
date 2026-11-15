# Nowa implementacja v0.2.0 — 15.11.2026

Polecenie użytkownika: napisać runtime od nowa po nieskutecznych próbach naprawy startu pamięci.

Aktualny tor: **UART badanego urządzenia → HC-05 master ⇄ HC-05 slave → UART1 ESP32 TX32/RX34 → TFT**. Moduły są już sparowane i połączone według użytkownika. Telefonu nie ma. UART2 nie jest inicjalizowany; nie ma mostu UART1–UART2. COM14 prowadzi przez HC-05 do UART1 i jest właściwym wejściem testowego strumienia dla TFT.

## Kod

- `main.c`: jawne kroki inicjalizacji, dodatkowa kontrola 16 KiB PSRAM, heartbeat i stan co 5 s.
- `app_health.c`: GPIO2 i timer; światło stałe po błędzie lub braku heartbeat przez 2 s, oficjalny callback TWDT. Nie gwarantuje sygnalizacji przed startem aplikacji ani po zatrzymaniu wszystkich przerwań/CPU.
- `uart_input.c`: odbiornik UART1, bufor 4096 B, zdarzenia i ograniczony budżet odczytu. Kopiuje dane do kolejki ekranu, nie czeka na LCD. STATE35 opcjonalne i domyślnie OFF; KEY12 nietknięty. Bez STATE LED miga co 500 ms, bez potwierdzania stanu BT.
- `screen.c`: jedyny właściciel modelu i renderera. Kolejka 64×128 B z numerami ciągłości; główny/alternatywny model i poprzednie komórki w PSRAM. Powitanie 3 s od ostatniego zakończonego DMA; w tym czasie odbiór i parser nadal pracują. Tekst biały/czarny, automatyczny podzbiór ANSI/VT100.
- `lcd_panel.c`: standardowe esp_lcd i vendorowany sterownik Espressif ILI9341, profile SPI3/I80/NONE. Bufor RGB565 320×8 w INTERNAL DMA RAM; po timeoutcie nie jest zwalniany ani ponownie używany. Błąd LCD nie zatrzymuje parsera i odbioru.

Parser ANSI, model terminala, raster/font oraz opcjonalny XPT2046 zachowano z regresjami. Warstwa startu, monitorowania, komunikacji i wyświetlania jest nowa. Usunięto stare zadania mostu/adaptera/wyświetlania i eksperymentalne wrappery PSRAM, linkera, cache i czasów Flash. Poprzedni opublikowany kod pozostaje w Git: `98e327f`. Kopia robocza sprzed przebudowy jest lokalnie `.pio/audit/pre_rewrite`. Niewgrana lokalna próba v0.1.21 nie stała się wydaniem.

## Konfiguracja

PlatformIO espressif32@7.0.1, ESP-IDF6.0.1, C, board esp32dev. Nowy generowany `sdkconfig.terminal` pochodzi z defaults, niezależnie od historycznego sdkconfig.esp32dev. Rewizja minimum 3.0 odpowiada logom użytkownika, optymalizacja -Os, Flash DIO40 MHz, PSRAM40 MHz z testem SDK. PSRAM używana przez jawne alokacje CAPS_ALLOC; stosy i sterowniki pozostają wewnętrzne. Bez modyfikowania SDK i rejestrów pamięci. To opis konfiguracji, nie dowód usunięcia bootloopu.

Domyślny profil SPI i dotykOFF zachowane. LCD: SCK18/MOSI23/MISO19/CS21/DC22/RESET27, zasilanie VCC/LED3,3 V według użytkownika. GPIO16/17 PSRAM i Flash6–11 są zarezerwowane. UART2 GPIO25/26 pozostają niewykorzystane. Natywne BT i Wi-Fi wyłączone. EN/AT i zapis SD pozostają przyszłymi zadaniami.

`esp32dev_core` ma ten sam standardowy start PSRAM, ale uruchamia tylko sprawdzenie pamięci, LED i heartbeat. Nie uruchamia LCD ani UART. Pozostałe środowiska wybierają pełny SPI, I80 albo odbiór bez wyświetlacza.

## Weryfikacja

Kolejność użytkownika: kompilacja, commit+push przed testami, potem oddzielny commit wyników. Najpierw rdzeń/PSRAM, potem pełny firmware, regresje i strumień COM14. CRC32 i liczniki odbioru/parsera pomagają sprawdzić kolejność i kompletność danych; fizyczny obraz wymaga obserwacji LCD. Powodzenie samego SPI nie dowodzi działania odłączonego panelu. Bieżące wyniki: STATUS.md.

Użytkownik ostatecznie potwierdził fizyczne odłączenie LCD podczas wcześniejszej próby, także GND i LED z Arduino. Krótki napis przy przełączaniu USB Arduino mógł być zbiegiem okoliczności; nie jest podstawą diagnozy zasilania ani potwierdzenia stabilnego app_main. Stan obecnego okablowania nie wynika z opisu dawnej próby.
