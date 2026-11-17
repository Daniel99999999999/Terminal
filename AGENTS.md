# Instrukcje dla Codex — ESP32 RetroTerminal

**17.11.2026 / v0.3.11:** użytkownik wybrał pauzę przez tapnięcie w tekst, wznowienie przez WZNOW. Kod i kompilacje internal/touch przygotowane; nowej wersji nie wgrano. W fizycznej próbie poprzedniego firmware potwierdził działające gesty w obu kierunkach, ale trafianie w PAUZA/WZNOW nie działało: logowane y222–231 poza dawnym y>=232. Nowe pole WZNOW/CR ma24 px. Odbiór937 wierszy /28968 B, CRC zgodne, zero błędów/strat, historia128; oględziny końcowego obrazu i wznowienie wymagają odbioru. [Raport](docs/SCROLL_TEST_2026-11-17.md), [STATUS](docs/STATUS.md). COM5/COM14 zamknięte, bez resetu/uploadu w tej próbie. Nie powtarzać kalibracji NVS bez potrzeby. Zachować kompilacja / commit+push / kontrole / osobny zapis wyników. Poniższe oczekiwanie na rozpoczęcie testów jest historyczne.

**Potwierdzenie17.11.2026:** użytkownik zgłosił działający LCD na **P4**. Pinout CS4 przyjęty do dalszej pracy; przyczyna P21 nieustalona. Przerwany etap to fizyczne próby IRQ XPT2046 i historii128 wierszy z gestami, które mają już testy programowe PASS. Kalibracja wcześniej ukończona i zachowana w NVS. [Bieżący STATUS](docs/STATUS.md); wiadomość użytkownika pytała o stan prób, nie uruchamiano portów ani uploadu w tej sesji. Starsze oczekiwanie na obraz z P4 poniżej jest historyczne.

**Najnowsze zlecenie17.11.2026 / v0.3.10:** użytkownik wybrał inny pin zamiast dalszych pomiarów P21. LCD SPI CS przeniesione na **GPIO4/P4**; nieużywana rezerwa GPIO4 dla EN podświetlenia usunięta, LED na3,3 V. [Bieżący pinout](docs/TFT_PROFILES_v0.1.12.md), wyniki w STATUS. Nowy przydział wymaga przepięcia tylko CS przy wyłączonym zasilaniu i nowego firmware. Nie wykonywać automatycznego uploadu ani dalszych testów ROM/P21. Ostatnia potwierdzona płytka v0.3.9 touch i CS odłączony; nie deklarować poprawy obrazu. Zachować kompilacja / commit+push / testy / osobny zapis wyników. Starsze przydziały CS21 są historyczne.

**Nowszy wynik16.11.2026:** COM5 potwierdził już wgraną v0.3.9 touch / ELF06590579c…; aplikacja działa, LCD biały. VCC/RESET około3,25 V, CS około0 V. Przewód LCD CS odłączony; wymuszony HIGH w ROM daje wejście GPIO21=1, użytkownik mierzy5 mV na wyprowadzeniu, kontrolnie3V3=3,257 V. Końcowe LOW→HIGH daje wejście0→1. Próba zakończona resetem, COM5 zamknięty, bez zapisu Flash. Kolejny krok: dokładny punkt pomiaru i jego połączenie na PCB; nie rozpoznano uszkodzenia GPIO ani przyczyny LCD. [Dane](docs/LCD_WHITE_v0.3.9.md), STATUS. Nie utożsamiać wcześniejszego „testy później / v0.3.8” z bieżącym stanem. Nie ogłaszać naprawy po samym READY ani statusie API.


**Aktualizacja16.11.2026 — v0.3.9:** przygotowane IRQ XPT2046 i przewijanie128 wierszy gestem, audyt czasów LCD i logi rejestrów. [Opis](docs/LCD_INIT_AUDIT_v0.3.9.md), faktyczne wyniki w STATUS. Zlecenie użytkownika: testy sprzętowe później; bez uploadu/resetu/otwierania COM5/COM14 w tej zmianie. Na płytce pozostaje v0.3.8 touch10 MHz, ostatnie cztery zdjęcia zaakceptowane, wcześniejsze bielenie/pasy nadal bez potwierdzonej przyczyny. GPIO36 IRQ nie ma wewnętrznego podciągania; odczyty współrzędnych podczas kontaktu odbywają się w zadaniu, nie ISR. Native BT/Wi-Fi wyłączone, UART1→TFT; UART2 nieaktywny, piny PSRAM16/17 zarezerwowane. Zachować kompilacja / commit+push / testy programowe / osobny commit wyników. Starsze stany niżej są historią.

## Najnowszy stan 15.11.2026 — v0.1.19

**Korekta użytkownika:** LCD nigdy wcześniej nie był fizycznie odłączany. Dopiero po próbie v0.1.18 odłączono go naprawdę; bezpośredni COM5 potwierdził ten sam wyjątek przed app_main. Wariant headless oznacza wyłączenie obsługi w kodzie, nie odłączenie sprzętu. LCD SPI używa 18/23/19/21/22/27, SDK PSRAM CLK17/CS16, Flash6..11: nie ma wspólnego GPIO w kodzie. Sprawdzony Flash 5E4016 / 4 MB / 3,3 V, status przed QIO 0x100000 (QE=0). Próby noflash/bariera/QIO z v0.1.13–18 nie naprawiły bootloopu. v0.1.19 usuwa eksperymentalne wrappery i linker fragment oraz wraca do standardowego startu SDK, zachowując QIO/40 MHz do próby. Stare nakazy testowania usuniętego check_psram_iram.py dotyczą historycznych wersji. Powitanie TFT 3 s i test COM14 do UART1 pozostają zlecone; nie deklarować ich fizycznego działania, zanim aplikacja ruszy. W chwili tej aktualizacji LCD jest odłączony. Bieżący wynik w STATUS.

## Bieżące zlecenie 15.11.2026 — v0.1.14

Powitanie TFT ma trwać 3 sekundy po inicjalizacji; UART działa w tle. Użytkownik zatwierdził upload/test przez COM5 i udostępnił COM14 z drugim HC-05 do pseudo danych. Potwierdził, że radio prowadzi do **UART1 GPIO32/34**, nie UART2: nie utożsamiać testu COM14 z odbiorem na TFT. [Opis](docs/WELCOME_UART_v0.1.14.md). Fizyczny test v0.1.13: bootloop nadal występuje, nowy wyjątek w s_psram_chip_init; v0.1.14 obejmuje kod wywołujący i literały w noflash. Potwierdzić działający start przez log, nie sam build. Zachować kolejność kompilacja / commit+push / testy / osobny commit wyników.

## Nowszy stan 15.11.2026 — v0.1.13 / bootloop PSRAM

Użytkownik wybrał w nagłówku domyślny **SPI**, nie odwracać tej zmiany. Dotyk jest teraz fizycznie niepodłączony: domyślny `RETRO_TOUCH_ENABLE=0`, włączenie po okablowaniu. Bootloop występuje także bez LCD. Przechwycony COM5 wykazał LoadProhibited przed app_main w `psram_is_32mbit_ver0`; [dowody i poprawka](docs/PSRAM_BOOT_FIX_v0.1.13.md). Projektowy `psram_startup.lf` przenosi obiekty wczesnego startu PSRAM i rezerwacji GPIO do noflash, bez edycji SDK i zmiany pinów/zegarów PSRAM. Sprawdzać rzeczywiste adresy przez `tests/check_psram_iram.py`. Domyślny esp32dev to SPI bez dotyku; `esp32dev_headless` jawnie NONE. Biały ekran nie jest dowodem usterki LCD, gdy program nie przechodzi inicjalizacji PSRAM. Zachować kolejność: kompilacja, commit/push przed testami, osobne wyniki. Użytkownik udostępnił COM5 do odczytu logu; nie utożsamiać tego z nowym poleceniem uploadu.

## Nowsze polecenia z 15.11.2026 — v0.1.12

Użytkownik dodał alternatywny moduł **ILI9341 SPI + XPT2046**, z potwierdzonym **VCC i LED 3,3 V**. Profile wybiera `#define RETRO_TFT_PROFILE` w `src/display_profile.h` lub `-D`: NONE / I80 / SPI. Domyślny NONE; gotowe środowiska `esp32dev_i80` i `esp32dev_spi` celowo aktywują odpowiedni profil. **Nie łączyć obu planów okablowania.** [Bieżący pinout i instrukcja](docs/TFT_PROFILES_v0.1.12.md): LCD SPI3 SCK18/MOSI23/MISO19/CS21/DC22/RST27, dotyk SPI2 CLK14/DIN13/DO39/CS33/IRQ36. LED nowego modułu z szyny 3V3; GPIO4 opcjonalnie do EN zewnętrznego drivera, bez bezpośredniego obciążenia LED. GPIO16/17 PSRAM, UART-y, LED2 i dotychczasowe warunki KEY12 zachowane. XPT2046 ma wstępny surowy odczyt bez kalibracji/UI; dawny zakaz dotyku dotyczy starego shielda I80, nie tego nowego kontrolera SPI. [Zapis na SD](docs/FUTURE_SD.md) to przyszłe zadanie, nie aktywna funkcja. Najnowsza kolejność użytkownika: **udana kompilacja, commit/push nowszej wersji przed testami**, następnie osobny zapis wyników. Nie wykonywać uploadu bez zlecenia. Poprzedni shield I80 nie ma podświetlenia przy samym 5V/GND; przyczyna nieustalona. Nie zmieniać tego faktu w deklarację jego awarii.

## Bieżące polecenia z 13.11.2026 — v0.1.11

Użytkownik zlecił implementację HC-05 i TFT (opisany klon ILI9341) oraz usunięcie natywnego BT/Wi-Fi z bieżącego firmware. HC-05 ZS-040, AT przez EN sprawdzone przez użytkownika, baud transmisji **115200**. Aktywne źródła nie zawierają starego SPP/GAP/HCI; powrót przez `baseline/v0.1.10` i Git. Nie uruchamiać eksperymentów radiowych. [Konfiguracja i stan](docs/HC05_TFT_v0.1.11.md). TFT/STATE/EN domyślnie wyłączone, pomiary shielda i strapa GPIO12 nadal wymagane. Na polecenie użytkownika: **po zakończeniu zmian podnieść wersję i wykonać commit + push jeszcze przed końcowymi testami**; wyniki kontroli zapisać osobnym commitem. Zachować starsze bazy i zmiany użytkownika.

## Cel projektu

Zbuduj terminal szeregowy DIY wykorzystujący podzespoły użytkownika: ESP32-WROVER (D0WD-V3), shield TFT 2,4 cala z 8-bitowym interfejsem równoległym, dotyk rezystancyjny 4-wire. Docelowo terminal dwukierunkowy UART ↔ Bluetooth SPP; LCD wyświetla ekran terminala i historię, dotyk może obsługiwać UI.

## Środowisko i zasady niepodlegające negocjacji

- Windows + VS Code + PlatformIO, `platform = espressif32`, `board = esp32dev`, **ESP-IDF 6.0.1** z logów użytkownika (sprawdź faktycznie zainstalowane zależności), **C**, bez Arduino.
- Kod nieblokujący w operacjach komunikacyjnych; preferuj zdarzenia, kolejki, DMA i kontrolowane timeouty. Nie zastępuj tego pętlami z `delay_ms()`.
- Zachowaj realny kod v0.1.4 jako punkt odniesienia. Nie przepisuj całej architektury bez wskazania problemu i testów regresji. Zmieniaj małe, sprawdzalne fragmenty.
- **GPIO16 i GPIO17 są zajęte przez PSRAM** w ESP32-WROVER — nigdy nie przypisuj ich do UART/LCD/dotyku.
- UART2: **TX GPIO25, RX GPIO26**, log konsoli UART0 przez USB, 115200 baud. Wszelkie sygnały do ESP32 muszą mieć zgodne poziomy 3,3 V. Nie łącz sygnałów RS-232 ani 5 V bez odpowiedniej konwersji.
- **Nie zakładaj**, że shield to ILI9341: kontroler LCD jeszcze nie został zidentyfikowany. Nie przypisuj pinów DB0–DB7/WR/DC/CS/RD/RST bez rzeczywistej weryfikacji PCB.
- Shield ma dwa układy buforowe z rodziny HC245 (oznaczenie widoczne na zdjęciu); ich zasilanie, kierunki i poziomy logiczne trzeba zbadać. Nie podłączaj wprost nieznanych sygnałów z płytki Arduino do ESP32.
- Panel rezystancyjny ma wyprowadzenia **X+/X−/Y+/Y−**, widoczne na zdjęciu spod mikroskopu. Przebieg do pinów shielda **nie jest potwierdzony**. Nie kopiuj domyślnego pinoutu popularnych shieldów.
- PSRAM: wykryty układ 8 MB, **4 MB mapowane** i działające w logu testowym. Na klasycznym ESP32 I80 DMA ma korzystać z buforów **DMA-capable INTERNAL RAM**; framebuffer/logi można przechowywać w PSRAM i kopiować porcjami.
- Wi-Fi w module zostało zgłoszone jako niesprawne. Android nie wykrywa nazwy ani adresu ESP32 SPP, mimo że serwer się uruchamia. Traktuj Bluetooth RF jako **niepotwierdzony**, nie jako działający.
- W obszarze RF użytkownik widział podejrzane połączenie w pobliżu L1/L4/C15. **Nie polecaj usuwania cyny ani przeróbek** bez schematu, pomiarów i identyfikacji elementu; nie myl go ze zworką antenową R14/R15.

## Reguły pracy

1. Na początku sesji przeczytaj `README.md`, `docs/STATUS.md`, `docs/HARDWARE.md`, `docs/NEXT_STEPS.md` i istniejący `src/main.c`.
2. Rozróżniaj **potwierdzone wynikami testów** od **hipotez**. Nie opisuj podzespołów ani pinów jako ustalonych, jeśli nie są.
3. Wprowadzaj zmiany przez małe moduły `.c/.h`, aktualizuj `src/CMakeLists.txt` i dokumentację; unikaj monolitu, ale nie rozbijaj bez potrzeby działającej części.
4. Po każdej zmianie wykonaj dostępne kontrole. Jeśli nie możesz uruchomić PlatformIO lub testu fizycznego, napisz to wprost. Nie deklaruj "skompilowane" albo "działa" bez potwierdzenia.
5. Przed przyłączeniem LCD: zweryfikuj sterownik, pinout, napięcia i kierunki linii. Dla dwukierunkowego odczytu rejestrów pamiętaj o buforach i tri-state.
6. Przy diagnozie Bluetooth: instrumentuj API / eventy GAP i sprawdź zwroty, nie wyciągaj wniosków o sprawności RF z samego logu SPP.
7. Zachowaj prostą ścieżkę powrotu do działającego kodu v0.1.4; unikaj czyszczenia niepowiązanych plików i kasowania ustawień użytkownika.
8. Komunikuj się z użytkownikiem po polsku. Dokumentuj zmiany, plan testu i ryzyko dla hardware.

## Pierwsze zadanie

Aktualizacja zgodnie z poleceniem użytkownika z 03.11.2026: **zakończono próby wbudowanego Bluetooth**. Oryginalny akceptor Espressif oraz ostatni wariant pełnej kalibracji RF również pozostały niewidoczne na Samsungu. Nie rozpoczynaj kolejnych eksperymentów BT bez nowego polecenia. Priorytetem jest **TFT LCD**: najpierw bezpieczna identyfikacja kontrolera, mapowanie PCB/HC245/dotyku oraz napięcia, potem kod i test grafiki. Użytkownik ma multimetr. Docelowy Bluetooth to **zewnętrzny HC-05 na osobnym UART**, z zachowaniem UART2 TX25/RX26 i USB/UART0. Uwzględnij dodatkowy UART w budżecie GPIO; nie kopiuj starej tabeli samego LCD jako gotowego schematu. Nie uznawaj toru RF za uszkodzony bez dowodu. Aktualny stan: `docs/STATUS.md`, procedura: `docs/TFT_BRINGUP.md`.

Wymaganie z 13.11.2026: zwykły tekst ma być biały na czarnym tle, a sekwencje ANSI/VT100 i popularne rozszerzenia mają być interpretowane automatycznie. W v0.1.10 dodano model/parser w pamięci oraz odbiór kopii UART przez kolejkę. Dokładny podzbiór, ograniczenia i testy są w `docs/TERMINAL_EMULATION.md`; nie deklaruj pełnej zgodności xterm ani fizycznego obrazu TFT bez testu. Nie gub stanu komend/UTF-8 na granicach porcji i nie zmieniaj bajtów przekazywanych przez most. Fizyczny renderer nadal wymaga wcześniejszej identyfikacji i pomiarów shielda.

Nowsze polecenie z 13.11.2026: użytkownik zlecił przydział GPIO dla HC-05 i TFT **bez dotyku na obecnym etapie**. Plan ESP32 jest w `docs/PIN_ASSIGNMENT_HC05_TFT.md` i `docs/PINOUT_TODO.csv`; nie oznacza weryfikacji sieci/napięć PCB. UART1 TX32/RX34, przyszły STATE35 i warunkowe EN/KEY12; GPIO2 pozostaje debug LED. Użytkownik potwierdził **ZS-040 i sprawdzone wejście AT przez EN**. GPIO12 na razie nie podłączać do HC-05: sprawdzić obciążenie EN/przycisku i zachować fabryczny strap VDD_SDIO. RD0/CS5/RS15 wymagają oceny stanów przy resecie i programowaniu. Dawne rezerwacje ADC dotyku nie obowiązują w tym wariancie. Fizyczne połączenie TFT nadal zależy od pomiarów; samo wpisanie pinów nie uruchamia UART1 ani LCD w firmware.
