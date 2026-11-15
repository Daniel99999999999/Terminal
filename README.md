# ESP32 RetroTerminal — VS Code / Codex

Projekt terminala szeregowego DIY opartego na **ESP32-WROVER (ESP32-D0WD-V3)**. Natywny **ESP-IDF 6.0.1 w PlatformIO**, język **C**, bez Arduino.

Repozytorium: [Daniel99999999999/Terminal](https://github.com/Daniel99999999999/Terminal). Bieżąca wersja: **v0.1.15**, diagnostyka startu. [Opis wersji](CHANGELOG.md), [praca z Git/GitHub](docs/GITHUB.md).

**v0.1.14:** [powitanie TFT przez 3 sekundy, test COM14 i rozszerzona ścieżka noflash](docs/WELCOME_UART_v0.1.14.md). COM14 prowadzi do HC-05 na UART1; model TFT nadal otrzymuje UART2 RX. Test sprzętowy v0.1.13 wykazał dalszy bootloop; aktualny wynik w STATUS.

**v0.1.13:** [bootloop podczas inicjalizacji PSRAM i lokalna poprawka linkowania](docs/PSRAM_BOOT_FIX_v0.1.13.md). Zachowano wybrany przez użytkownika domyślny **ILI9341 SPI**, dotyk domyślnie **OFF** (niepodłączony). `esp32dev` i `esp32dev_spi` korzystają z SPI bez dotyku; `esp32dev_headless` wymusza NONE, `esp32dev_i80` pozostaje alternatywą. Wczesny log pokazuje profil i stan dotyku. Stan fizycznej weryfikacji opisuje STATUS.

**v0.1.12 (historia):** [profile TFT przez `#define`: NONE / ILI9341 I80 / ILI9341 SPI + XPT2046](docs/TFT_PROFILES_v0.1.12.md). Nowy moduł SPI ma VCC i LED **3,3 V** według użytkownika; osobny przydział GPIO i osobne magistrale LCD/dotyku. Wstępne surowe odczyty dotyku bez kalibracji/UI. [Zapis na SD zachowano jako przyszłe zadanie](docs/FUTURE_SD.md).

**v0.1.11:** dodano most HC-05 UART1 TX32/RX34 przy potwierdzonych 115200, opcjonalne STATE35 i EN12/AT oraz renderer ILI9341 przez I80/DMA. [Konfiguracja i ograniczenia](docs/HC05_TFT_v0.1.11.md), [połączenia](docs/PIN_ASSIGNMENT_HC05_TFT.md). Natywne BT/Wi-Fi usunięto z bieżącego firmware. TFT, STATE i EN są domyślnie wyłączone do potwierdzenia warunków elektrycznych. Kod wymaga testu na rzeczywistym sprzęcie; końcowe kontrole po commicie źródeł są opisane w STATUS.

> **Stan sprzętu:** próby wbudowanego BT zakończono decyzją użytkownika, bez ustalenia przyczyny. HC-05 to ZS-040, użytkownik sprawdził AT przez EN i podał baud transmisji 115200. Stary shield I80 nie świeci nawet przy samym 5V/GND; stabilizator 3,3 V według użytkownika działa. Nowy moduł SPI ma potwierdzone VCC/LED 3,3 V; fizyczny obraz i dotyk pozostają nieprzetestowane. Brak nowego logu potwierdzającego wgraną wersję; nie wykonano automatycznego uploadu. [Wyniki i historia](docs/STATUS.md).

## Szybki start — Windows / VS Code

1. Sklonuj `git clone https://github.com/Daniel99999999999/Terminal.git` do wybranego katalogu roboczego.
2. W VS Code wybierz **File → Open Folder** i otwórz **folder zawierający `platformio.ini`**.
3. Upewnij się, że jest zainstalowane rozszerzenie **PlatformIO IDE**; opcjonalnie rozszerzenie Codex.
4. Otwórz projekt w Codex i poleć przeczytać `AGENTS.md` i `docs/STATUS.md` (gotowy prompt w `CODEX_START.md`).
5. Zbuduj projekt przez PlatformIO: `pio run -e esp32dev` lub **Terminal → Run Task → PIO: Build**.
6. Wgraj: `pio run -e esp32dev -t upload`.
7. Monitor portu: `pio device monitor -b 115200` (ostatni log z 12.11.2026: COM5; port może się zmienić).

Zweryfikowane środowisko v0.1.10: **PlatformIO espressif32 7.0.1 / ESP-IDF 6.0.1**. `platformio.ini` nie przypina wersji platformy; przed pierwszą kompilacją sprawdź wersję zależności w logu. Do odtworzenia tego środowiska ustaw lokalnie `platform = espressif32 @ 7.0.1`. Porty `upload_port` i `monitor_port` dostosuj do swojego komputera. Do testów bez ESP32: `python tests/run_host_tests.py` (kompilator C11: GCC, Clang lub bcc64x; opcjonalnie `--cc ścieżka`).

**Uwaga na konfigurację:** `sdkconfig.defaults` ustala żądane opcje, ale jeśli projekt ma już wygenerowany `sdkconfig`, może on mieć pierwszeństwo. W istniejącym działającym projekcie zachowaj `sdkconfig`, porównaj z `sdkconfig.defaults` i w razie potrzeby użyj `pio run -t menuconfig`. Jeśli konfiguracja jest świeża i nie zawiera własnych zmian, można ją wygenerować od nowa. Nie usuwaj w ciemno konfiguracji projektu źródłowego.

**Uwaga na rozmiar kompilacji:** pierwszy pełny build ESP-IDF kompiluje setki plików bibliotek statycznych. To normalne i nie znaczy, że każdy zostanie dołączony do pliku wynikowego. Następne buildy korzystają z cache `.pio/build`. Nie wprowadzaj optymalizacji `COMPONENTS` kosztem działającej konfiguracji bez pomiaru skutków.

## Aktualny firmware

`src/` zawiera most HC-05, parser/model i renderer terminala, transport I80/SPI, XPT2046 oraz debug LED. Profile i opcje TFT/dotyku wybiera `src/display_profile.h` lub `build_flags`; [instrukcja v0.1.12 i pinout SPI](docs/TFT_PROFILES_v0.1.12.md). Opcje HC-05 pozostają w `src/Kconfig.projbuild` → **RetroTerminal**, a [instrukcja v0.1.11](docs/HC05_TFT_v0.1.11.md) opisuje STATE/EN i API AT. Bez STATE informacja BT pozostaje nieznana, most UART działa. Nie uruchamiamy natywnego radia.

## Pliki i historia v0.1.10

Poniższy opis SPP dotyczy zachowanej bazy **v0.1.10**, nie bieżącego firmware. Dawne moduły `src/bt_*` znajdują się obecnie w `baseline/v0.1.10/src/`. Archiwalne pliki vendor/diagnostics nie są linkowane przez bieżącą aplikację.

- `src/main.c` — firmware v0.1.10, Bluetooth SPP / UART2 / FreeRTOS / diagnostyka i kolejka modelu terminala.
- `src/ansi_parser.c/.h`, `src/terminal.c/.h`, `src/terminal_view.c/.h` — domyślnie biały tekst na czarnym tle, automatyczny podzbiór ANSI/VT100 i rozszerzeń xterm; [zakres i ograniczenia](docs/TERMINAL_EMULATION.md). Model w pamięci, fizyczny TFT nadal do uruchomienia.
- `diagnostics/esp_idf_spp_acceptor/` — niezależny projekt z oryginalnym C Espressif do porównania na Androidzie; osobne zadania VS Code.
- `cmake/bt_controller_compat.cmake`, `vendor/esp32-bt-lib/` — lokalny backport oficjalnej poprawki #18145, z kontrolą wersji/haszy, pochodzeniem i licencją.
- `src/bt_hci_diag.c/.h`, `src/bt_hci_scan.c/.h` — pasywna obserwacja HCI, filtr i dopasowanie odpowiedzi, domyślnie `BT_HCI_SCAN_DIAG=1`.
- `src/bt_discovery_diag.c/.h` — porównanie CoD/EIR, obecnie domyślnie `BT_DISCOVERY_TEST=2`.
- `src/bt_tx_state.c/.h` — stan zapisu SPP, generacje sesji i obsługa congestion/błędów.
- `src/debug_led.c/.h`, `src/debug_led_logic.c/.h` — LED, heartbeat, reakcja na TWDT / abort i testowalna logika czasu.
- `baseline/v0.1.4/` — niezmieniona baza z konfiguracją i instrukcją powrotu.
- `baseline/v0.1.6/` — kopia wersji z ostatniego testu sprzętowego użytkownika.
- `baseline/v0.1.7/` — dokładna kopia trybu 2 sprzed dodania HCI wraz z sumami plików.
- `baseline/v0.1.8/` — dokładna kopia 18 plików sprzed podmiany linkowanej biblioteki kontrolera.
- `baseline/v0.1.9/` — kopia 7 plików sprzed dodania modelu terminala, z manifestem SHA256; korzysta również ze wspólnych modułów w `src/`.
- `CHANGELOG.md` — opis v0.1.10, wyniki weryfikacji i ograniczenia pierwszego importu do Git.
- `tests/run_host_tests.py` — testy na komputerze; `--suite bt` / `--suite led` / `--suite hci` / `--suite terminal` wybiera zestaw, domyślnie wszystkie.
- `docs/LED_AND_PIN_PLAN.md` — LED zasilania/statusu, możliwości wykrywania zawieszeń, propozycja LCD i ADC1 dla dotyku.
- `docs/FIXES_v0.1.5.md` — mapowanie napraw A1–A5, zachowanie na granicy sesji i ograniczenia.
- `platformio.ini` — PlatformIO, `esp32dev`, ESP-IDF, monitor 115200.
- `sdkconfig.defaults` — Bluetooth Classic, PSRAM i 4 MB Flash.
- `AGENTS.md` — nadrzędne reguły pracy dla Codex.
- `CODEX_START.md` — gotowy prompt do rozpoczęcia sesji Codex.
- `docs/STATUS.md` — fakty potwierdzone oraz niepotwierdzone.
- `docs/HARDWARE.md` — sprzęt, ograniczenia pinów, LCD i panel dotykowy.
- `docs/NEXT_STEPS.md` — kolejność prac i kryteria odbioru.
- `docs/TESTS.md` — procedury testowania; nie wprowadzaj zmian w RF bez diagnozy.
- `docs/photos/` — wybrane fotografie modułu, LCD, taśm i oznaczeń.

## Co robiła baza v0.1.10?

- Inicjalizacja PSRAM i raportowanie wolnego heapu.
- UART2, **GPIO25 TX**, **GPIO26 RX**, 115200 8N1 TTL 3,3 V.
- Bluetooth Classic SPP (`ESP32-Terminal`), uruchomienie serwera i diagnostyka; to obecny kod główny, jeszcze bez integracji HC-05.
- Cztery zadania FreeRTOS, w tym osobne zadanie modelu terminala; bez aktywnego oczekiwania na znaki.
- RX UART odczytuje dostępne dane po zdarzeniu, z kontrolnym oczekiwaniem do 100 ms przy utracie zdarzenia. Nie czeka na dopełnienie 128 B.
- Zapis SPP ma limit 3 s. Błąd, timeout lub rozłączenie podczas zapisu wywołują restart profilu SPP; telefon musi wtedy połączyć się ponownie.
- Pakiety mają identyfikator sesji. Na jej granicy RX UART odrzuca zaległe dane; szczegóły i test oczekiwania na log granicy są w `docs/TESTS.md`.
- Debug LED GPIO2 (active HIGH według schematu NodeMCU-32 v1.3): zmiana stanu co 500 ms bez SPP, co 1000 ms z SPP; wykryty błąd oznacza ON. Heartbeat, TWDT i zapamiętana przyczyna resetu rozszerzają diagnostykę; całkowitego zatrzymania CPU nie da się niezawodnie zasygnalizować samym kodem.
- `#define SPP_TEST_ECHO 1`: w razie faktycznego połączenia Bluetooth **echo** znaków z telefonu; do pracy jako most UART ↔ Bluetooth ustawić `0` po potwierdzeniu testów.
- **Model ANSI/VT100 jest zaimplementowany i przyjmuje kopię danych UART2 także offline. Sterownik LCD i dotyk nie są jeszcze uruchomione.** Formatowane dane zmieniają komórki, kolory, kursor i przewijanie; [dokładny podzbiór](docs/TERMINAL_EMULATION.md). Zwykły tekst zaczyna od białego na czarnym tle.

**Nie zakładaj**, że SPP działa radiowo tylko dlatego, że w logu wypisano `SPP listening`. Bluetooth nie został jeszcze wykryty przez telefon.

Nowszy log potwierdza podstawowe połączenie ACL z odbiornikiem, lecz nie zakończenie uwierzytelniania ani otwarcie SPP. Nie jest to dowód całkowitej niesprawności radia ani potwierdzenie stabilnego transferu.

Wyniki aktualnych kontroli i testy wymagające sprzętu są w `docs/STATUS.md` oraz `docs/TESTS.md`. Logi v0.1.7 / tryby 1 i 2 potwierdzają lokalną inicjalizację; obie próby zakończyły się niewidocznością na Samsungu.
