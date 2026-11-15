# ESP32 RetroTerminal — VS Code / Codex

Projekt terminala szeregowego DIY opartego na **ESP32-WROVER (ESP32-D0WD-V3)**. Natywny **ESP-IDF 6.0.1 w PlatformIO**, język **C**, bez Arduino.

Repozytorium: [Daniel99999999999/Terminal](https://github.com/Daniel99999999999/Terminal). Bieżąca wersja: **v0.3.1**. [Nowa implementacja i konfiguracja](docs/REWRITE_v0.2.0.md), [opis wersji](CHANGELOG.md), [praca z Git/GitHub](docs/GITHUB.md).

**v0.3.1:** [CR jako nowa linia, wybierany ścisły VT, pauza/wznowienie obrazu dotykiem](docs/DISPLAY_CONTROLS_v0.3.1.md). CR+LF nadal daje pojedyncze przejście. Odbiór działa podczas pauzy; historia scrollback jeszcze nie jest zaimplementowana. Dotyk wymaga podłączenia i kalibracji.

**v0.3.0:** [XPT2046: kalibracja pięciu punktów, niezależna walidacja, test dotyku i zapis ustawień](docs/TOUCH_CALIBRATION_v0.3.0.md). Osobny profil `esp32dev_touch` używa działającego RAM wewnętrznego. Obrót LCD 3 zachowany. Wariant bez dotyku nadal dostępny; użytkownik dopiero podłączy dotyk według opisanego planu.

**v0.2.3:** po zdjęciu wzoru z opisami narożników zmieniono domyślny obrót z1 na3 (o180°), zachowując320×240. W v0.2.2 litery i narożniki były obrócone względem fizycznego ustawienia LCD. Stan weryfikacji w STATUS.

**v0.2.2:** poprawiono tabelę obrotów ILI9341 po zdjęciu pokazującym odbicie lewo–prawo. Domyślny poziomy obraz używa MADCTL 0x28 zamiast 0x68. [Porównanie orientacji](https://github.com/adafruit/Adafruit_ILI9341/blob/master/Adafruit_ILI9341.cpp#L209). Stan próby sprzętowej w STATUS; działający wariant pamięci nadal `esp32dev_internal`.

**Wynik orientacji:** [v0.2.3 wgrana, stabilny start; obrót 3 zaakceptowany przez użytkownika](docs/RESULTS_v0.2.3.md). [Szczegółowy raport bootloopu](docs/BOOTLOOP_REPORT_2026-11-15.md) oddziela dowody od hipotez; pierwotna przyczyna błędu startu PSRAM pozostaje nieustalona.

**Aktualny tor:** badane urządzenie → HC-05 master ⇄ HC-05 slave → **UART1 TX32/RX34, 115200 → TFT**. Telefonu nie ma; UART2 i most nie są uruchamiane. Nowy runtime, standardowy start SDK bez eksperymentalnych wrapperów pamięci, powitanie 3 s i parser ANSI/VT100. Dawne opisy mostu poniżej są historyczne; rzeczywiste wyniki w STATUS.

**v0.2.1:** jawny wariant `esp32dev_internal` używa wyłącznie RAM wewnętrznego do sprawdzenia terminala bez wadliwego obecnie startu PSRAM. Domyślny `esp32dev` nadal włącza PSRAM; v0.2.0 core potwierdził błąd przed app_main. Wyniki prób w STATUS; nie ogłaszać naprawy PSRAM.

**Sprawdzono i wgrano `esp32dev_internal`:** stabilny start, powitanie w logu 3001 ms, COM14/HC-05 → UART1 → parser: 17894 B, CRC32 zgodne, bez strat i błędów. **Użytkownik potwierdził podłączony LCD, widoczne powitanie i migającą LED GPIO2.** Treść i kolory testu COM14 wymagają jeszcze wizualnego sprawdzenia. W VS Code wybierz `PIO: Upload SPI TFT (internal RAM diagnostic)`; zwykłe `PIO: Upload` nadal wybiera profil PSRAM.

**v0.1.19:** standardowy start PSRAM SDK, Flash QIO/40 MHz; usunięte nieskuteczne eksperymentalne wrappery i noflash z v0.1.13–18. Powitanie TFT 3 s zachowane. Wcześniejsze wpisy poniżej opisują historię; rzeczywiste wyniki w STATUS.

**v0.1.14:** [powitanie TFT przez 3 sekundy, test COM14 i rozszerzona ścieżka noflash](docs/WELCOME_UART_v0.1.14.md). COM14 prowadzi do HC-05 na UART1; model TFT nadal otrzymuje UART2 RX. Test sprzętowy v0.1.13 wykazał dalszy bootloop; aktualny wynik w STATUS.

**v0.1.13:** [bootloop podczas inicjalizacji PSRAM i lokalna poprawka linkowania](docs/PSRAM_BOOT_FIX_v0.1.13.md). Zachowano wybrany przez użytkownika domyślny **ILI9341 SPI**, dotyk domyślnie **OFF** (niepodłączony). `esp32dev` i `esp32dev_spi` korzystają z SPI bez dotyku; `esp32dev_headless` wymusza NONE, `esp32dev_i80` pozostaje alternatywą. Wczesny log pokazuje profil i stan dotyku. Stan fizycznej weryfikacji opisuje STATUS.

**v0.1.12 (historia):** [profile TFT przez `#define`: NONE / ILI9341 I80 / ILI9341 SPI + XPT2046](docs/TFT_PROFILES_v0.1.12.md). Nowy moduł SPI ma VCC i LED **3,3 V** według użytkownika; osobny przydział GPIO i osobne magistrale LCD/dotyku. Wstępne surowe odczyty dotyku bez kalibracji/UI. [Zapis na SD zachowano jako przyszłe zadanie](docs/FUTURE_SD.md).

**v0.1.11:** dodano most HC-05 UART1 TX32/RX34 przy potwierdzonych 115200, opcjonalne STATE35 i EN12/AT oraz renderer ILI9341 przez I80/DMA. [Konfiguracja i ograniczenia](docs/HC05_TFT_v0.1.11.md), [połączenia](docs/PIN_ASSIGNMENT_HC05_TFT.md). Natywne BT/Wi-Fi usunięto z bieżącego firmware. TFT, STATE i EN są domyślnie wyłączone do potwierdzenia warunków elektrycznych. Kod wymaga testu na rzeczywistym sprzęcie; końcowe kontrole po commicie źródeł są opisane w STATUS.

> **Stan sprzętu:** nowy LCD SPI wyświetla obraz, obrót 3 został zaakceptowany. HC-05 → UART1 → parser sprawdzono przez COM14, GPIO2 miga. ESP32 używa RAM wewnętrznego; dotyk nie jest jeszcze podłączony. Próby natywnego BT zakończono. Dawny shield I80 bez podświetlenia pozostaje osobnym, nierozwiązanym przypadkiem. [Wyniki i historia](docs/STATUS.md).

## Szybki start — Windows / VS Code

1. Sklonuj `git clone https://github.com/Daniel99999999999/Terminal.git` do wybranego katalogu roboczego.
2. W VS Code wybierz **File → Open Folder** i otwórz **folder zawierający `platformio.ini`**.
3. Upewnij się, że jest zainstalowane rozszerzenie **PlatformIO IDE**; opcjonalnie rozszerzenie Codex.
4. Otwórz projekt w Codex i poleć przeczytać `AGENTS.md` i `docs/STATUS.md` (gotowy prompt w `CODEX_START.md`).
5. Zbuduj sprawdzony wariant bez dotyku: `pio run -e esp32dev_internal`; z podłączonym XPT2046 wybierz `esp32dev_touch`.
6. Wgraj wybrany wariant, np. `pio run -e esp32dev_internal -t upload --upload-port COM5`. Domyślny `esp32dev` nadal włącza PSRAM i obecnie wpada w bootloop.
7. Monitor portu: `pio device monitor -b 115200` (ostatni log z 12.11.2026: COM5; port może się zmienić).

Środowisko: **PlatformIO espressif32 7.0.1 / ESP-IDF 6.0.1**, platforma przypięta w `platformio.ini`. Porty `upload_port` i `monitor_port` dostosuj do swojego komputera. Do testów bez ESP32: `python tests/run_host_tests.py` (kompilator C11: GCC, Clang lub bcc64x; opcjonalnie `--cc ścieżka`).

**Uwaga na konfigurację:** `sdkconfig.defaults` ustala żądane opcje, ale jeśli projekt ma już wygenerowany `sdkconfig`, może on mieć pierwszeństwo. W istniejącym działającym projekcie zachowaj `sdkconfig`, porównaj z `sdkconfig.defaults` i w razie potrzeby użyj `pio run -t menuconfig`. Jeśli konfiguracja jest świeża i nie zawiera własnych zmian, można ją wygenerować od nowa. Nie usuwaj w ciemno konfiguracji projektu źródłowego.

**Uwaga na rozmiar kompilacji:** pierwszy pełny build ESP-IDF kompiluje setki plików bibliotek statycznych. To normalne i nie znaczy, że każdy zostanie dołączony do pliku wynikowego. Następne buildy korzystają z cache `.pio/build`. Nie wprowadzaj optymalizacji `COMPONENTS` kosztem działającej konfiguracji bez pomiaru skutków.

## Aktualny firmware

`src/` zawiera odbiornik HC-05 na UART1, parser/model ANSI/VT100, renderer TFT I80/SPI, XPT2046 z kalibracją i debug LED. UART2 i most nie są uruchamiane. Profile wybiera `src/display_profile.h` lub `build_flags`; [dotyk i kalibracja](docs/TOUCH_CALIBRATION_v0.3.0.md), [pinout LCD SPI](docs/TFT_PROFILES_v0.1.12.md). STATE pozostaje opcjonalne, EN/KEY nie jest sterowane; bez STATE stan połączenia BT jest nieznany. Nie uruchamiamy natywnego radia. Dawne API mostu/AT z v0.1.11 nie należy do bieżącego runtime.

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
