# Raport bootloopu ESP32-WROVER — 15.11.2026

## Wniosek i granice diagnozy

**Nie ustaliliśmy jeszcze pierwotnej przyczyny pozwalającej wskazać konkretny uszkodzony element albo konkretny błąd SDK.** Ustaliliśmy etap awarii, powtarzalny związek z inicjalizacją PSRAM oraz błędne wartości odczytane spod prawidłowych adresów kodu Flash. Mamy działający wariant aplikacji bez inicjalizacji PSRAM. To obejście umożliwia używanie terminala, ale nie naprawia PSRAM.

Najdokładniejsze obecnie uzasadnione stwierdzenie: **podczas standardowego startu pamięci zewnętrznej, przed app_main, występuje wyjątek CPU; zrzut spod adresu prawidłowej funkcji SDK różni się od kodu w dopasowanych ELF i BIN. Obszar do zbadania obejmuje wspólny tor Flash/PSRAM, cache i jego rekonfigurację. Nie rozstrzygnięto, czy przyczyna jest programowa, elektryczna, czy wynika z niezgodności konkretnej pamięci.**

## Platforma i aktualne warianty

- ESP32-D0WD-V3, rewizja v3.0 według bootloadera, płytka z modułem WROVER.
- PlatformIO espressif32 7.0.1, framework-espidf 4.60001.0 / ESP-IDF 6.0.1, narzędzia Xtensa 15.2.0+20251204. Język C, bez Arduino.
- Flash: odczytane JEDEC producent 5E, device 4016, rozmiar 4 MB; odczyt esptool wskazał strap 3,3 V. Sam JEDEC nie potwierdza nadruku/modelu fizycznego układu.
- PSRAM w niektórych próbach raportowała 8 MB, z 4 MB mapowanymi; zegar 40 MHz. GPIO16/17 należą do PSRAM, Flash używa 6–11.
- LCD SPI3: SCK18/MOSI23/MISO19/CS21/DC22/RESET27. HC-05 slave: UART1 TX32/RX34, 115200. UART2 nieaktywny w nowym runtime.
- `esp32dev`: PSRAM włączona, świeży sdkconfig.terminal, minimum rev3, -Os, Flash DIO 40 MHz, PSRAM 40 MHz, jawny alokator CAPS_ALLOC. Nie jest potwierdzony jako działający.
- `esp32dev_internal`: PSRAM wyłączona tylko w tym jawnym wariancie, model i bufory w RAM wewnętrznym. To aktualnie działająca ścieżka.

## Co dokładnie się restartowało

W najczytelniejszej próbie kontrolnej v0.2.0 (`a738b45`, środowisko esp32dev_core) nie uruchamiano zadań LCD ani UART. Pozostawiono standardowy start PSRAM SDK, bez projektowych wrapperów, zmian cache, rejestrów Flash i własnych sekcji linkera.

[Przechwycony cykl](logs/2026-11-15_v0.2.0_core.txt): bootloader poprawnie ładuje aplikację, po komunikacie `quad_psram` następuje `IllegalInstruction`, potem `Rebooting...`. W 15 s zarejestrowano 63 restarty, bez app_main/READY. Zdekodowany łańcuch wywołań:

```text
call_start_cpu0                 0x400824e8  cpu_start.c:997
  mspi_init                    0x400d3393  cpu_start.c:635
    esp_psram_chip_init         0x400d2aab  esp_psram.c:407
      s_psram_chip_init         0x400d299c  esp_psram.c:207
        psram_cmd_end           0x400d2cdc  esp_psram_impl_quad.c:414
```

Adresy odpowiadają wyłącznie zachowanemu ELF tej konkretnej próby. Nie należy używać ich do dekodowania innej kompilacji. Zachowany ELF ma SHA256 `9f09a18e175f596f02a22eec2ed4500fe957819a6d2fe60ef54fefa9f35fad69`. Bootloop uszkadzał również końcówkę backtrace, dlatego dekodowanie stosu jest wspierane przez prawidłowy PC i porównanie kodu, a nie traktowane jako bezbłędny pełny ślad wszystkich ramek.

`psram_cmd_end` czeka na zakończenie komendy SPI i przywraca zapamiętane rejestry USER/USER1/USER2. To funkcja SDK, nie nasza obsługa TFT ani kolejka danych. W dopasowanym ELF znajduje się w obszarze kodu mapowanego z Flash.

Po wyjątku uruchamia się panic handler, który w bieżącej konfiguracji restartuje procesor. Stąd powtarzalne `rst:0xc (SW_CPU_RESET)`. Sam powód resetu nie wskazuje miejsca pierwotnej awarii; wyjaśnia go wcześniejszy wyjątek. [Zachowanie panic handlera w ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-guides/fatal-errors.html).

## Nowe porównanie: kod oczekiwany a zrzut z awarii

15.11.2026 ponownie odczytano kod z zachowanych artefaktów v0.2.0, bez wykonywania kolejnego eksperymentu na sprzęcie. [Wynik GDB](logs/2026-11-15_v0.2.0_elf_comparison.txt):

| Adres CPU | Słowo w ELF i BIN | Zrzut po wyjątku |
|---|---|---|
| 0x400d2cd8 | 0xf01d0a2d | 0xffffffff |
| **0x400d2cdc — PC** | **0x91004136** | **0xffffffff** |
| 0x400d2ce0 | 0x20c0f592 | 0x20c0f592 |

Oczekiwane bajty od PC to `36 41 00`, czyli prawidłowa instrukcja Xtensa `entry a1,32`. Zgadza się ona w ELF i BIN. W BIN obszar porównania zaczyna się od offsetu 0x12cd8, co przy aplikacji wgranej pod 0x10000 odpowiada fizycznemu offsetowi Flash 0x22cd8.

To mocniejszy dowód niż samo przypuszczenie o nieprawidłowym skoku programu: PC trafia w istniejącą funkcję, plik zawiera prawidłowy rozkaz, a zrzut pokazuje inne dane w części tego obszaru. **Interpretacja:** prawdopodobnie zawodzi pobieranie/mapowany odczyt kodu w tej fazie. **Granica dowodu:** panic dump jest odczytem wykonanym przez obsługę wyjątku, a nie nagraniem magistrali ani bezpośrednim śladem instrukcji pobranych przez CPU. Nie ustala, czy błędna wartość powstała na pinach pamięci, w cache, w mapowaniu, czy wskutek rekonfiguracji kontrolera.

Weryfikacja hash przez esptool potwierdzała zgodność zapisu podczas programowania. Nie gwarantuje poprawnych odczytów w innych warunkach pracy, po włączeniu PSRAM.

## Dlaczego inicjalizacja PSRAM może wpływać na kod Flash

Flash przechowuje program, PSRAM jest dodatkową pamięcią danych. Na klasycznym ESP32 obsługa zewnętrznej PSRAM jest połączona sprzętowo z interfejsem pamięci Flash. Start PSRAM ustawia między innymi tryby dostępu i cache. GPIO16/17 są jej dodatkowymi liniami; sama rezerwacja tych dwóch GPIO nie opisuje całego wspólnego toru pamięci. [Opis architektury pamięci zewnętrznej Espressif](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-guides/external-ram.html).

To uzasadnia badanie współdziałania obu pamięci. Nie dowodzi, że sama PSRAM jest uszkodzona. Potencjalnie poprawny układ PSRAM może ujawniać problem konfiguracji Flash lub czasów/poziomów na wspólnych liniach.

## Co sprawdzono i z jakim skutkiem

| Próba | Zmiana / pomiar | Rzeczywisty wynik |
|---|---|---|
| v0.1.12 | Pierwszy dokładny odczyt i dekodowanie | LoadProhibited przy psram_is_32mbit_ver0; brak app_main. |
| v0.1.13 | Helpery PSRAM i rezerwacja GPIO w IRAM/DRAM | Bootloop pozostał; wyjątek przesunął się do s_psram_chip_init. |
| v0.1.14 | Szersze przeniesienie ścieżki startu do pamięci wewnętrznej | Nadal wyjątek, później w funkcji mapowania MMU. |
| v0.1.15–16 | Rejestry MSPI, odczyt nagłówka Flash przez ROM | Nagłówek odczytany prawidłowo; PSRAM rozpoznana, test pamięci PASS, później wyjątek w esp_clk_tree_initialize. |
| v0.1.17 | Doświadczalna bariera cache, headless | Brak naprawy. Headless oznaczał wyłączony sterownik, nie fizycznie odłączony LCD. |
| v0.1.18 | Flash QIO 40 MHz, dalsze obejścia | Nadal wyjątek po teście PSRAM. Późniejszy odczyt z LCD odłączonym według użytkownika również wykazał ten wyjątek. |
| v0.1.19 | Standardowy start SDK, usunięte obejścia, QIO | Nadal wyjątek podczas psram_read_id. |
| v0.1.20 | Przywrócenie czasów CS Flash z bootloadera | Zmiana CTRL2 0x10→0→0x10 potwierdzona; nadal uszkodzone dane kodu i wyjątek. |
| v0.2.0 core | Przepisany runtime, czyste defaults, standardowy SDK, -Os/rev3/DIO40/PSRAM40, bez startu peryferiów | 63 restarty/15 s; IllegalInstruction w psram_cmd_end przed app_main. |
| v0.2.1 internal | Pominięta inicjalizacja PSRAM; nowy terminal w RAM wewnętrznym | Brak restartów w przechwyconym 15 s odczycie; uruchomione TFT/UART1; test danych PASS; powitanie i LED potwierdzone przez użytkownika. |

Lokalna, niewgrana próba v0.1.21 nie jest wynikiem sprzętowym i nie stanowi wydanej wersji. Wszystkie doświadczalne wrappery pamięci i reguły linkera usunięto z aktywnej implementacji v0.2.

Dowody szczegółowe i historia korekt są w [STATUS](STATUS.md), a dopasowane surowe artefakty pozostają lokalnie w `.pio/audit`.

## Jakie hipotezy osłabiono albo wykluczono w danym zakresie

1. **Błąd inicjalizacji SPI TFT jako konieczna przyczyna tych restartów:** przeczy temu awaria przed app_main i próba core bez uruchamiania sterownika LCD. Nie jest to dowód poprawności każdego możliwego fizycznego połączenia.
2. **Wykorzystywanie GPIO16/17 przez kod LCD:** w przejrzanym aktywnym przydziale nie występuje. Fizycznego okablowania nie mierzyliśmy zdalnie; użytkownik potwierdził próbę z odłączonym LCD, także od LED/GND Arduino. Wcześniejsze niejednoznaczne odpowiedzi zostały skorygowane w historii i nie tworzą dodatkowych niezależnych prób.
3. **Przepełnienie kolejki/modelu terminala lub błąd fontu:** te moduły nie wykonywały się przed app_main. Nie mogą wyjaśniać zarejestrowanego wyjątku wczesnego startu.
4. **Brak podłączonego dotyku lub aktywność natywnego BT/Wi-Fi:** dotyk był wyłączony, komponenty radiowe nie występują w nowym buildzie; błąd core pozostał.
5. **Samo DIO kontra QIO albo same czasy CS:** zmiany nie były wystarczające do naprawy. Nie wyklucza to innego problemu trybu, czasów lub zgodności układów pamięci.
6. **Udowodniony reset brownout/watchdog:** przechwycone cykle pokazują wyjątki CPU i restart po panic, a nie komunikat brownout lub timeout watchdog. Nie wyklucza to krótkiego zakłócenia zasilania pamięci, które najpierw uszkodziłoby odczyt bez wyzwolenia detektora brownout CPU.

## Czego nie wolno wywnioskować z dotychczasowych prób

- `SPI SRAM memory test OK` w kilku eksperymentalnych wariantach potwierdzał testowany dostęp w tamtej chwili, nie stabilne współdziałanie Flash i PSRAM podczas całego startu.
- Ostrzeżenie o 8 MB PSRAM i4 MB mapowanych nie jest samo w sobie błędem. Dostępna przestrzeń adresowa klasycznego ESP32 jest ograniczona; pojemność fizyczna i rozmiar mapowania to różne wielkości.
- Odczyt przez ROM w v0.1.16 obejmował **16 B nagłówka pod 0x10000**, nie cały obszar kodu i nie dokładnie adres obecnego wyjątku. Nie stanowi pełnego testu Flash.
- Błędnie opisane indeksy MMU w dawnych komunikatach nie dowodzą uszkodzenia mapowania. Te etykiety były korygowane; raport nie używa ich jako potwierdzenia przyczyny.
- Przesuwanie wyjątku po przenoszeniu kodu do IRAM nie oznaczało usunięcia problemu. Obchodziliśmy jedną zależność od Flash, a awaria ujawniała się później.
- Działanie RAM wewnętrznego nie dowodzi uszkodzenia kostki PSRAM. W porównywanych wariantach zmieniają się też układ linkowania i opcje obsługi pamięci; nie był to laboratoryjny test jednego bitu przy identycznych wszystkich pozostałych warunkach.
- Obecna wiedza nie pozwala łączyć wcześniejszych problemów wbudowanego Bluetooth/Wi-Fi z tym bootloopem. Nie wykonujemy nowych prób RF.

## Co faktycznie naprawiono użytkowo

Nowa aplikacja v0.2 działa bez inicjalizacji PSRAM: HC-05 master przy urządzeniu przesyła dane do slave przy UART1 terminala; UART1 zasila parser i TFT. W teście COM14 wysłano, odebrano i przetworzono 17894 B, CRC32 odbioru/parsera były zgodne (`0x40cad4e7`), brak strat i błędów UART, zinterpretowano 13 sekwencji sterujących. [Wynik](logs/2026-11-15_v0.2.1_uart1_result.json).

Użytkownik potwierdził widoczne powitanie i miganie GPIO2. Pierwsze zdjęcie ujawniło osobny błąd orientacji obrazu. Nowe zdjęcie wzoru v0.2.2 wykazało obrót o 180° względem fizycznego ustawienia panelu; v0.2.3 zmienia obrót z 1 na 3. To problem obrazu już działającej aplikacji, nie wyjaśnienie wcześniejszego wyjątku przed jej startem. Użytkownik zaakceptował to ustawienie i polecił pominąć dalsze wzory narożników; [wynik uploadu i kontroli startu](RESULTS_v0.2.3.md).

Poprzednie zmiany nazywane „poprawkami PSRAM” były próbami obejścia, których skuteczności testy **nie potwierdziły**. Nie należy przedstawiać ich jako zakończonej diagnozy lub naprawy.

## Co rozstrzygnęłoby pierwotną przyczynę

To plan dalszych badań, nie lista kolejnych już wykonanych prób:

1. Minimalny standardowy projekt PSRAM bez aplikacji terminala, na tej płytce i na znanym sprawnym WROVER, z identycznym SDK/toolchain i ustawieniami. Różny wynik zawęzi problem do egzemplarza/pamięci; taki sam u obu — do konfiguracji/oprogramowania.
2. Kontrolowane porównanie niezmodyfikowanego SDK 6.0.1 z inną znaną stabilną wersją na tej samej płytce. Wymaga osobnej decyzji, bo ustalonym środowiskiem projektu pozostaje 6.0.1; w ramach tego raportu niczego nie przełączano.
3. Pomiar oscyloskopem zasilania pamięci i sygnałów CS/CLK w momencie włączania PSRAM, z właściwymi sondami. Multimetr nie pokaże krótkich zakłóceń i nie zastąpi pomiaru sygnałów o tej częstotliwości.
4. Porównanie odczytu tych samych bajtów kodu przez mapowanie CPU i bezpośredni odczyt Flash, przed/po krytycznym kroku, przy kodzie pomiarowym niezależnym od badanej pamięci. To miałby być jeden ograniczony eksperyment, a nie kolejne rozszerzanie wrapperów całego startu.

Do czasu takiego rozstrzygnięcia poprawna diagnoza brzmi: **potwierdzony problem w ścieżce startu pamięci zewnętrznej, z nieprawidłowym mapowanym odczytem kodu; pierwotna przyczyna pozostaje nieustalona.**
