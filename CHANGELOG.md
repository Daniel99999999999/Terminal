# Historia wersji

## v0.3.5 — 15.11.2026

- Gesty TEST/TERMINAL zachowują kontakt przez przerwę krótszą niż80 ms od ostatniej poprawnej próbki. CAL nie zeruje czasu po pojedynczym odrzuceniu ADC; PAUZA/CR nie przełączają wielokrotnie przy jednym kontakcie. Pełne puszczenie lub zbyt długa przerwa nadal kończy gest.
- CAL startuje nowym dotknięciem właściwego przycisku; po starcie toleruje niewielki ruch do wnętrza ekranu. Przeciągnięcie poza ten obszar anuluje gest. Postęp10% na pasku i logi raw→screen/start/anulowanie/przyjęcie.
- Regresje rzeczywistych ciągów próbek z brakami, oddzielnych krótkich przytrzymań, przeciągnięcia i przerwy w odpytywaniu. Dotychczasowy próg punktów kalibracji i NVS zachowane.

## v0.3.4 — 15.11.2026

- Przywrócono osobne 24-taktowe transakcje XPT2046 z buforami inline i PD0=0 przy każdej konwersji, jak w fizycznie odczytującej punkty v0.3.1. Jedna seria z v0.3.2/3 została wycofana po zerowych odczytach podczas rzeczywistych naciśnięć; nie uznajemy jeszcze jej za ustaloną przyczynę.
- Zachowano medianę, odrzucanie pierwszych próbek osi, pomiar nacisku bez wymagania IRQ, diagnostykę Z1/Z2, tolerancję krótkich braków próbki i NVS. Testy obu trybów IRQ sprawdzają ramki i czas życia całego statycznego descriptora po timeout.

## v0.3.3 — 15.11.2026

- Domyślnie pomiar nacisku/osi XPT2046 przy każdym odpytywaniu, również przy IRQ=HIGH. IRQ pozostaje diagnostyczny; `RETRO_TOUCH_REQUIRE_IRQ=1` przywraca poprzednie bramkowanie. Progi, osie, piny i mapa NVS zachowane.
- TOUCH_RAW zawiera surowe Z1/Z2, stan IRQ przed serią i ustawienie REQUIRE_IRQ. end_high liczy teraz poprawne próbki ADC przy końcowym HIGH; w trybie polling nie są one odrzucane.
- Regresje obu trybów: nacisk przy stałym HIGH oraz brak nacisku przy HIGH. Weryfikacja fizyczna rozdzieli stan IRQ od odpowiedzi ADC; przyczyna stałego HIGH pozostaje nieustalona.

## v0.3.2 — 15.11.2026

- XPT2046: pojedyncza seria SPI przy CS=LOW, aktywny ADC między konwersjami, PENIRQ przywracane ostatnią komendą. Bufory statyczne/aligned z zachowaniem czasu życia po timeout.
- Kalibracja toleruje krótki zanik pojedynczej próbki; pełne puszczenie i przerwa co najmniej 50 ms zerują zbieranie. Zachowano próg nacisku, minimum ośmiu poprawnych próbek i 100 ms; dodano postęp pomiaru.
- Diagnostyka TOUCH_RAW co 2 s, testy protokołu i utraty próbki. Przyczyna fizyczna ignorowanych dotknięć nie jest jeszcze ustalona; wyniki próby w STATUS.

## v0.3.1 — 15.11.2026

- Opcja wyświetlania CR jako nowej linii, domyślnie ON, dla zgłoszenia Enter z PuTTY. CRLF nie podwaja przejścia, również na granicy porcji; wybierany standardowy CR dla VT i linii postępu.
- Pasek dotykowy PAUZA/WZNÓW, tryb CR i przytrzymanie CAL po prawej. Pauza zamraża obraz, odbiór i model pracują; wznowienie odrysowuje aktualny ekran. Bez historii scrollback.
- Pozycja kursora i ustawienia widoku w diagnostyce SCREEN, regresje zakończeń wiersza i sterowania dotykiem. Wyniki w STATUS.

## v0.3.0 — 15.11.2026

- Kalibracja XPT2046: pięć punktów, dopasowanie afiniczne, niezależny punkt walidacji, odrzucanie niestabilnych/zdegenerowanych pomiarów, wymagane puszczenie między punktami.
- Ekran testowy i powrót „GOTOWE”; ponowna kalibracja po przytrzymaniu paska terminala2 s. Odbiór/parser działają w tle.
- NVS z CRC i kontrolą rozmiaru/obrotu; brak kasowania istniejącej partycji. Osobny profil esp32dev_touch, UART ISR w IRAM przy zapisie Flash, wariant bez dotyku zachowany.
- Nowe regresje modelu kalibracji i UI z symulacją NVS; wyniki kontroli i prób fizycznych w STATUS.

## v0.2.3 — 15.11.2026

- Zdjęcie aktualnego wzoru v0.2.2 potwierdza obrót obrazu o180°, nie samo odbicie: LEWY GORNY znajduje się na dole po prawej, litery są do góry nogami.
- Domyślny RETRO_TFT_ROTATION zmieniony z1 na3, czyli obrót o180° przy zachowanym poziomym rozmiarze320×240. Bez zmian pinów i ustawień pamięci.

## v0.2.2 — 15.11.2026

- Zmieniona tabela orientacji ILI9341 na mapowanie referencyjne dla czterech obrotów. Dla domyślnego poziomego obrazu MADCTL zmienia się z 0x68 na 0x28 (BGR zachowane).
- Zdjęcie użytkownika pokazało tekst odbity lewo–prawo w v0.2.1. Po wgraniu v0.2.2 użytkownik nadal zgłasza zamianę lewo/prawo; oczekujemy aktualnego zdjęcia, nie uznajemy orientacji za naprawioną. Wyniki i szczegółowy raport bootloopu w STATUS.

## v0.2.1 — 15.11.2026

- Jawny wariant `esp32dev_internal`: ten sam terminal HC-05 UART1/TFT, modele i bufory w RAM wewnętrznym, PSRAM wyłączona tylko w tym profilu. Domyślny profil i zarezerwowane GPIO PSRAM pozostają zachowane.
- Umożliwia sprawdzenie aplikacji niezależnie od powtarzalnego błędu standardowego startu PSRAM. Nie jest deklarowany jako naprawa PSRAM; wyniki w STATUS.

## v0.2.0 — 15.11.2026

- Nowa warstwa startu, LED/heartbeat, odbioru UART1 i wyświetlania; usunięte eksperymentalne wrappery/cache/linker/Flash timing.
- Badane urządzenie przez HC-05 master/slave do UART1 TX32/RX34 i TFT; UART2/most nieaktywne, telefonu nie ma.
- Świeży sdkconfig.terminal: ESP32 rev3, -Os, DIO40/PSRAM40, jawne alokacje PSRAM. Profile SPI/I80/NONE, powitanie 3 s, parser/font i dotykOFF zachowane.
- COM14 testuje właściwe wejście modelu, z licznikami i CRC32 odbioru/parsera. Faktyczne wyniki w STATUS.

## v0.1.20 — 15.11.2026, kontrola czasów Flash CS

- Zachowanie czasów hold/setup CS Flash z bootloadera przez inicjalizację współdzielonego PSRAM; log wartości przed/po. Niewielki wrapper IRAM, bez ręcznej bariery cache, bez zmiany zegarów/pinów. Wynik sprzętowy w STATUS; wcześniejszy standardowy SDK/QIO także nie startował.

## v0.1.19 — 15.11.2026

- Powrót do standardowej inicjalizacji SDK: usunięte nieskuteczne projektowe noflash/wrapper/bariera cache i ich test adresów; wariant QIO/40 MHz zachowany do weryfikacji osobno od tych obejść. Powitanie TFT przez 3 s, dotyk OFF i przydział GPIO zachowane. Wyniki w STATUS.

## v0.1.18 — 15.11.2026

- Wybór Flash QIO/40 MHz w PlatformIO i SDK, zgodny z odczytanym ID 5E4016 i wspólnymi liniami Quad PSRAM. Wcześniej status QE=0, tryb DIO; tryb QIO ustawia QE przez standardowy bootloader Espressif. Bez zmiany pinów, częstotliwości PSRAM, eFuse i zawartości użytkownika poza aplikacją/bootloaderem. Wynik fizyczny w STATUS.

## v0.1.17 — 15.11.2026

- Bariera cache CPU0 przy rekonfiguracji PSRAM przed startem CPU1/RTOS; zależności bootloadera i tablice GPIO w wewnętrznym RAM. Usunięto kontrolny odczyt fizyczny Flash z v0.1.16, zachowano poprawione logi kontrolne MMU i powitanie TFT 3 s. Zegar i piny pamięci bez zmian. Kontrola ELF obejmuje nowe zależności; skuteczność w STATUS.

## v0.1.16 — 15.11.2026, diagnostyka Flash/MMU

- Odczyt wpisów MMU, słowa mapowanego DROM i fizycznego nagłówka Flash przez API ROM po inicjalizacji PSRAM. Odczyt nie zapisuje Flash ani eFuse. Tryb, dummy i zegar Flash pozostały identyczne w próbie v0.1.15; nadal bootloop. Powitanie zachowane, wynik COM5 w STATUS.

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
