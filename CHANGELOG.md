# Historia wersji

## v0.1.15 — 15.11.2026, diagnostyka startu

- Odczyt kontrolnych rejestrów Flash przed i po inicjalizacji PSRAM przez wrapper IRAM/ROM; bez zapisu rejestrów i modyfikacji SDK. v0.1.14 rozpoznawała PSRAM, ale dalszy odczyt kodu Flash zwracał `ffffffff`. Powitanie 3 s zachowane. Wyniki w STATUS.

## v0.1.14 — 15.11.2026

- Ekran powitalny: nazwa i wersja firmware, biały tekst na czarnym, 3 sekundy po zakończeniu rysowania. Komunikacja i model terminala działają w tym czasie. Diagnostyczne pasy kolorów domyślnie OFF.
- Po negatywnym fizycznym teście v0.1.13 rozszerzono lokalne noflash na kod wywołujący PSRAM i start MSPI, wraz z kontrolą adresów w ELF. Parametry PSRAM i SDK zachowane; skuteczność zależy od testu sprzętowego.
- Skrypt testu pseudo UART przez udostępniony COM14, z zapisem payloadu/logów i kontrolą liczników mostu. [Zakres i ograniczenia](docs/WELCOME_UART_v0.1.14.md), wyniki w STATUS.

## v0.1.13 — 15.11.2026

- Lokalny linker fragment noflash dla wczesnego startu PSRAM i rezerwacji GPIO na ESP32. Przechwycono LoadProhibited w `psram_is_32mbit_ver0` przed app_main; dotyczy także startu bez LCD. SDK i parametry pamięci pozostają niezmienione. [Dowody i zakres poprawki](docs/PSRAM_BOOT_FIX_v0.1.13.md).
- Zachowano użytkownikowi domyślny SPI; niepodłączony dotyk domyślnie OFF. Jawny profil `esp32dev_headless`, wczesny log wersji/profilu/dotyku i etapy inicjalizacji TFT.
- Dodano kontrolę rzeczywistego umieszczenia helperów PSRAM/atomików/stanu w RAM; test polityki HC-05 oddzielono od wybranego profilu LCD, którego GPIO sprawdzają dedykowane testy trzech wariantów. Test XPT2046 jawnie włącza swój driver niezależnie od domyślnego OFF.
- Kompilacja i publikacja poprzedzają testy zgodnie z poleceniem. Fizyczne potwierdzenie naprawy wymaga uruchomienia nowego obrazu; [STATUS](docs/STATUS.md).

## v0.1.12 — 15.11.2026

- Wybór przez `#define` w `display_profile.h`: NONE (domyślnie), ILI9341 I80 albo ILI9341 SPI. Środowiska PlatformIO `esp32dev`, `esp32dev_i80`, `esp32dev_spi` i osobne zadania VS Code. Usunięto niejednoznaczny drugi wybór TFT z menuconfig; ustawienia LCD/dotyku są makrami z nadpisaniem `-D`.
- SPI3 LCD: SCK18/MOSI23/MISO19, CS21/DC22/RESET27. SPI2 XPT2046: CLK14/DIN13/DO39/CS33/IRQ36. VCC i LED nowego modułu: 3,3 V potwierdzone przez użytkownika. GPIO4 rezerwą na EN zewnętrznego drivera LED, domyślnie bez sterowania. UART-y, GPIO2 i PSRAM16/17 zachowane.
- Wspólny renderer ANSI/VT100, INTERNAL DMA i kontrola żywotności bufora; I80 zachowany, SPI z właściwą kolejnością bajtów RGB565. Wstępny odczyt XPT2046: surowe X/Y/nacisk, mediana, IRQ i ograniczone oczekiwania SPI; bez kalibracji i UI dotyku. Testy profili/GPIO, protokołu/timeoutu dotyku i RGB565 przygotowano do wykonania po commicie.
- [Zapis na karcie SD](docs/FUTURE_SD.md) wpisano do planu przyszłych funkcji; bez inicjalizacji karty i przydziału CS w tej wersji. Źródłem wersji w logu startowym jest deskryptor aplikacji ESP-IDF.
- Przed commitem wszystkie trzy warianty skompilowano z wynikiem SUCCESS w espressif32 7.0.1 / ESP-IDF 6.0.1. Zgodnie z poleceniem opublikowano [2c1806e](https://github.com/Daniel99999999999/Terminal/commit/2c1806e), następnie przeszły testy hosta (w tym nowe profile/konfiguracje/XPT2046/timeout/RGB565), Cppcheck 12 C × 3 profile bez zgłoszeń i kontrola obiektów/ELF/PSRAM/braku radia. Wyniki i ograniczenia w [STATUS](docs/STATUS.md); bez automatycznego uploadu i bez deklaracji działania sprzętowego.

[Konfiguracja i pełny pinout](docs/TFT_PROFILES_v0.1.12.md). Poprzedni shield I80 nadal nie ma potwierdzonego podświetlenia.

## v0.1.11 — 13.11.2026

- HC-05 ZS-040 na UART1 TX32/RX34, 115200 8N1; dwukierunkowy most z UART2 TX25/RX26. Osobne kolejki i zadania RX/TX, ograniczone oczekiwania FIFO, liczniki błędów/strat.
- Opcjonalne STATE35: nieznany stan bez przewodu, debounce połączenia, generacje sesji i debug LED. Opcjonalny EN12: kolejka przełączania AT, osobny baud i API komend/odpowiedzi, bez automatycznej zmiany nastaw modułu.
- Oficjalny sterownik ILI9341 2.1.0 Espressif, równoległe I80/DMA, renderer 8×8 z kolorami/atrybutami ANSI i paskiem statusu BT. Bufor DMA w INTERNAL, model/kopia ekranu w PSRAM. Domyślnie TFT/STATE/EN wyłączone do pomiarów; brak dotyku.
- Usunięto natywne SPP/GAP/HCI z aktywnych źródeł i zależności, wyłączono włączanie komponentów BT/Wi-Fi przez minimalny build. Archiwum v0.1.10: 26 plików z SHA256; starsze bazy zachowane.
- Testy polityki HC-05, rasteryzacji oraz nowych kanałów heartbeat. Zgodnie z poleceniem źródła wysłano w `46ea0fa` przed końcowymi testami. Następnie: 6 zestawów hosta i 2 warianty adaptera PASS, oba buildy (domyślny i z TFT/STATE/KEY) SUCCESS, Cppcheck obu konfiguracji bez zgłoszeń; brak komponentów/archiwów/implementacji natywnego radia potwierdzony. [Szczegóły](docs/STATUS.md).

[Konfiguracja, FabGL, zakres i ograniczenia](docs/HC05_TFT_v0.1.11.md). Brak uploadu i potwierdzenia działania na TFT/HC-05.

## v0.1.10 — 13.11.2026

Pierwszy import projektu ESP32 RetroTerminal do repozytorium [Terminal](https://github.com/Daniel99999999999/Terminal). Wersja rozwojowa: model terminala jest zaimplementowany i przetestowany na komputerze, fizyczny ekran TFT pozostaje do uruchomienia.

### Dodane funkcje

- Strumieniowy parser `ansi_parser` i model ekranu `terminal`: zwykły tekst domyślnie biały na czarnym tle, automatyczna interpretacja podzbioru ANSI/VT100 oraz popularnych rozszerzeń xterm.
- Kolory 16/256/RGB, atrybuty znaków, pozycjonowanie kursora, czyszczenie i edycja, regiony przewijania, ramki DEC oraz ekran alternatywny `?1049`. Zachowanie stanu UTF-8 i komend na granicach odebranych porcji.
- Adapter `terminal_view`: kopia UART2 RX trafia przez kolejkę do osobnego zadania także bez połączenia Bluetooth. Dwie strony logiczne 80×24 zajmują 46 080 B PSRAM. Udostępniono API kopiowania wierszy dla przyszłego renderera.
- Liczniki utraty danych i reset częściowego parsera po przerwie w strumieniu; zadanie modelu objęto diagnostyką heartbeat LED.
- 22 scenariusze regresji modelu/parsera, w tym podziały strumienia, błędne sekwencje, wartowniki buforów i 20 000 bajtów danych adversarial.

Dokładny podzbiór i ograniczenia: [TERMINAL_EMULATION.md](docs/TERMINAL_EMULATION.md). Nie jest to pełna emulacja xterm.

### Zachowane elementy projektu

- UART2 TX GPIO25 / RX GPIO26, 115200 8N1, logika 3,3 V; konsola USB/UART0. GPIO16/17 pozostają zarezerwowane dla PSRAM.
- Most UART/SPP z generacjami sesji, obsługą błędów i timeoutów; debug LED, pasywna diagnostyka HCI oraz lokalny backport biblioteki kontrolera Espressif z pochodzeniem i licencją.
- Bazy v0.1.4, v0.1.6, v0.1.7, v0.1.8 i v0.1.9 oraz osobny projekt referencyjny Espressif. Starsze wersje są kopiami w `baseline/`, nie wcześniejszymi commitami tego repozytorium. Zakres każdej kopii i sposób powrotu opisuje jej README.
- Dokumentacja sprzętu, audytu, wyników wcześniejszych prób BT oraz procedury bezpiecznych pomiarów TFT/HC245/dotyku.

### Weryfikacja

Kontrole wykonane 13.11.2026 na Windows, PlatformIO **espressif32 7.0.1 / ESP-IDF 6.0.1**:

| Kontrola | Wynik |
|---|---|
| Build `esp32dev` | SUCCESS; Flash 702 260 B, statyczny RAM 46 692 B |
| Testy hosta C11 | 62 scenariusze PASS: BT 16, LED 11, HCI 13, terminal 22 |
| Adapter HCI | Oba warianty, diagnostyka włączona i wyłączona: PASS |
| Cppcheck, 10 plików C | Brak zgłoszeń warning/performance/portability |
| Manifesty baz v0.1.7–v0.1.9 | 39 sum SHA256 zgodnych |

Szczegóły i granice kontroli: [STATUS.md](docs/STATUS.md), [TESTS.md](docs/TESTS.md). Artefakty kompilacji i cache `.pio/` nie są częścią repozytorium.

### Stan sprzętu i dalsze prace

- v0.1.10 nie został wgrany na płytkę ani sprawdzony na fizycznym TFT/UART2. Ostatni potwierdzony program na ESP32 to `ref-fullcal-6.0.1`.
- Kontroler TFT, mapowanie PCB, zasilanie/kierunki HC245 i połączenia dotyku nadal wymagają pomiarów. Sterownik LCD, renderer i dotyk nie są uruchomione.
- Próby wbudowanego Bluetooth zakończono decyzją użytkownika. Android nie wykrywał ESP32, Windows nie zakończył parowania; przyczyna pozostaje nieustalona. Nie uznano RF za sprawny ani za uszkodzony.
- Docelowy Bluetooth: zewnętrzny HC-05 na osobnym UART; integracja pozostaje do wykonania. Autobaud jest dostępny sprzętowo, ale nie został włączony w aplikacji.

Kolejność prac i kryteria odbioru: [NEXT_STEPS.md](docs/NEXT_STEPS.md).
