# v0.1.13 — start PSRAM przed uruchomieniem TFT

## Potwierdzone objawy i log

15.11.2026 użytkownik zgłosił biały ekran nowego LCD SPI, brak migania debug LED i bootloop. Dotyk nie był podłączony. Bootloop występuje także po odłączeniu wszystkich przewodów LCD.

Po zwolnieniu COM5 przechwycono bezpośrednio powtarzalny panic: **LoadProhibited, EXCVADDR=0xffffffff**, po `quad_psram: This chip is ESP32-D0WD`, przed `app_main()`. [Jeden pełny cykl](logs/2026-11-15_v0.1.12_psram_bootloop.txt). To reset programowy po wyjątku (`rst:0xc`), nie log brownout. Nie przesądza o uszkodzeniu pamięci ani LCD.

Sześć segmentów aplikacji i czas bootloadera odpowiadały lokalnemu obrazowi SPI v0.1.12, ELF SHA256 `1ef5fd13baaefbc43eb6fc4ec85b0517a076d2f3748c5d714f8852b4e9939fa3`. Adresy zdekodowano przed przebudową:

| Adres | Funkcja / plik ESP-IDF 6.0.1 |
|---|---|
| `0x400df47e` | `psram_is_32mbit_ver0`, `esp_psram_impl_quad.c:848` |
| `0x40084bd0` | `esp_psram_impl_enable`, ten sam plik, wywołanie w linii 996 |
| `0x400dc784` | `s_psram_chip_init`, `esp_psram.c:207` |
| `0x400dc8bf` | `esp_psram_chip_init`, `esp_psram.c:407` |
| `0x400df953` | `mspi_init`, `cpu_start.c:635` |
| `0x4008500c` | `call_start_cpu0`, `cpu_start.c:997` |

Ponieważ wyjątek występuje przed uruchomieniem aplikacji, firmware nie zdążył skonfigurować debug LED ani kontrolera LCD. Brak dotyku nie jest wyjaśnieniem tego wyjątku. SPI nie potwierdza obecności urządzenia przez ACK; samo `ESP_OK` transakcji nie identyfikuje podłączonego XPT2046.

## Lokalna poprawka rozmieszczenia kodu

W wadliwym ELF helper `psram_is_32mbit_ver0` i inne procedury tego pliku znajdowały się w IROM/Flash; `esp_gpio_reserve` także miał adres Flash `0x400db9c8`. Stan `s_psram_id` i maska pinów były w DRAM. Wywołania tych helperów następują podczas rekonfiguracji wspólnego kontrolera Flash/PSRAM. Zależność od pobierania instrukcji/stałych przez cache w tej fazie jest niepożądana; dokładny mechanizm błędnego odczytu pozostaje hipotezą do potwierdzenia próbą nowego obrazu.

`src/psram_startup.lf`, dołączony przez `LDFRAGMENTS`, umieszcza całe obiekty `esp_psram_impl_quad` i `esp_gpio_reserve` w schemacie **noflash** (kod IRAM, stałe DRAM), warunkowo tylko dla klasycznego ESP32 z PSRAM. Dzięki temu nie polegamy na tym, czy kompilator akurat zinline'uje helper lub czy jego linia jest w cache. Nie modyfikujemy zainstalowanego SDK, algorytmu PSRAM, GPIO16/17, napięć ani zegara 40 MHz. Kosztem jest niewielkie dodatkowe zajęcie wewnętrznego RAM, weryfikowane po kompilacji.

Wersja zachowuje **wybrany przez użytkownika domyślny profil SPI** w `display_profile.h`. `RETRO_TOUCH_ENABLE=0` pasuje do aktualnego okablowania; dotyk włączyć przez `-DRETRO_TOUCH_ENABLE=1` po jego podłączeniu. Nowe `esp32dev_headless` wymusza NONE niezależnie od domyślnego profilu. Dodano wczesny log `app_main v... profile=... touch=...` oraz etapy inicjalizacji TFT.

## Weryfikacja i kolejność

Zgodnie z poleceniem: kompilacja, commit/push wersji, potem testy. `tests/check_psram_iram.py` sprawdza rzeczywiste adresy helperów PSRAM, `esp_gpio_reserve`, zależności `__atomic_fetch_or_8` oraz stanu DRAM w ELF. To kontrola regresji linkowania; nie zastępuje uruchomienia na sprzęcie. [Wyniki](STATUS.md).

Próba fizyczna ma potwierdzić przejście przez test PSRAM, pojawienie się `app_main`, stabilny log i miganie LED, a dopiero potem obraz na ponownie podłączonym LCD. Nie deklarować naprawionego obrazu na podstawie samej kompilacji lub poprawnych adresów.

Źródła: [lokalna reguła noflash zgodna z dokumentacją ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-guides/linker-script-generation.html), [ESP-IDF 6.0.1: helper i wywołania](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_psram/esp32/esp_psram_impl_quad.c), [oryginalne mapowania SDK](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_psram/linker.lf). Przejrzano także [poprawkę bariery cache Espressif 475ec39](https://github.com/espressif/esp-idf/commit/475ec39cb4f8fd98dd8d926c4e3a273048959a5a): dotyczy innych ścieżek MSPI, nie jest bezpośrednią poprawką klasycznego ESP32 i nie została skopiowana do tego projektu.

## Wynik fizyczny 15.11.2026

Po zgodzie u?ytkownika wgrano obraz caf6bde przez COM5; weryfikacja zapisu SUCCESS. Bootloop pozostaje: nowy PC 0x400db83f / s_psram_chip_init:214 po powrocie z impl_enable, EXCVADDR=0x800db96e. Kontrola IRAM helper?w przesz?a, ale nie obejmowa?a kodu wywo?uj?cego i jego litera??w Flash. [Log](logs/2026-11-15_v0.1.13_psram_bootloop.txt). Ta wersja nie naprawi?a startu urz?dzenia.
