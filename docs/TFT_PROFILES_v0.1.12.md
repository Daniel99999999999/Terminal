# TFT I80 lub SPI + XPT2046 — v0.1.12

> **Aktualizacja v0.1.13:** zachowano wybór użytkownika: domyślny profil w nagłówku to teraz **SPI**. Dotyk jest niepodłączony, więc domyślnie OFF; po podłączeniu włącz `RETRO_TOUCH_ENABLE=1`. `esp32dev_headless` wymusza NONE, a `esp32dev_spi` SPI. Poniższy opis domyślnych NONE/touch ON dotyczy oryginalnej v0.1.12. [Naprawa startu PSRAM i stan sprawdzenia](PSRAM_BOOT_FIX_v0.1.13.md). Pinout nie zmienił się.

Warianty są **alternatywnymi połączeniami**, nie dwoma ekranami podłączonymi jednocześnie. ESP32-WROVER: GPIO16/17 pozostają dla PSRAM; UART2 TX25/RX26, HC-05 TX32/RX34, STATE35, warunkowy KEY12, USB1/3 i debug LED2 nie zmieniają przydziału. Natywne BT/Wi-Fi pozostają poza firmware.

Tabela do dalszych pomiarów: [PINOUT_SPI.csv](PINOUT_SPI.csv). Wszystkie przypisania GPIO są planem i implementacją, nie wynikiem fizycznego testu połączeń.

## Wybór przez `#define`

W `src/display_profile.h` ustaw jedną wartość:

```c
#define RETRO_TFT_PROFILE RETRO_TFT_NONE          // bez ekranu, domyślnie
// albo RETRO_TFT_ILI9341_I80                    // dotychczasowy shield równoległy
// albo RETRO_TFT_ILI9341_SPI                    // nowy moduł SPI + XPT2046
```

Można również podać `-DRETRO_TFT_PROFILE=RETRO_TFT_ILI9341_SPI` w `build_flags`. Gotowe środowiska w `platformio.ini`:

| Środowisko | Profil | Komenda kompilacji |
|---|---|---|
| `esp32dev` | NONE, chyba że zmienisz define w nagłówku | `pio run -e esp32dev` |
| `esp32dev_i80` | ILI9341 I80 | `pio run -e esp32dev_i80` |
| `esp32dev_spi` | ILI9341 SPI + XPT2046 | `pio run -e esp32dev_spi` |

Wgrywaj przez `pio run -e WYBRANE_ŚRODOWISKO -t upload`. Zwykłe zadanie VS Code `PIO: Upload` nadal używa `esp32dev`; dla SPI dodano osobne zadania. Nie wykonano automatycznego uploadu.

Wybór transportu zastępuje stare `CONFIG_RETRO_TFT_ENABLE` z menuconfig. Wszystkie ustawienia LCD/dotyku są teraz makrami w `display_profile.h`, z możliwością nadpisania przez `-D`: obrót 0..3 (domyślnie 1), BGR=1, inwersja=0, wzór kolorów=1 przez 2 s, I80=1 MHz, LCD SPI=20 MHz, XPT2046 SPI=1 MHz. `RETRO_TOUCH_ENABLE=0` wyłącza dotyk w profilu SPI. Nieznany profil, zły obrót i dotyk w wariancie I80/NONE powodują błąd kompilacji. NONE nie ustawia GPIO LCD ani dotyku.

## Nowy LCD ILI9341 SPI — przydział po stronie ESP32

Użytkownik potwierdził **VCC i LED = 3,3 V**. To inny moduł niż dotychczasowy shield 5 V. Logika także wymaga 3,3 V; nie podawać 5 V na nowy moduł.

| Wyprowadzenie LCD | Połączenie ESP32 / zasilanie | Funkcja |
|---|---|---|
| SDO (MISO) | **GPIO19** | wejście ESP32, SPI3 MISO |
| LED | **3V3** | stałe zasilanie podświetlenia zgodnie z deklaracją użytkownika, nie GPIO |
| SCK | **GPIO18** | SPI3 SCLK |
| SDI (MOSI) | **GPIO23** | SPI3 MOSI |
| DC | **GPIO22** | dane / komenda |
| RESET | **GPIO27** | reset aktywny LOW |
| CS | **GPIO4 (P4)** | wybór LCD aktywny LOW; od v0.3.10 zamiast GPIO21 |
| GND | **GND** | wspólna masa |
| VCC | **3V3** | zasilanie modułu |

Szyna 3V3 musi zasilać także podświetlenie; jej wydajność prądowa powinna uwzględniać ten pobór. **Od v0.3.10 GPIO4 jest LCD CS**; nie jest już rezerwą dla EN drivera podświetlenia. LED pozostaje na potwierdzonym zasilaniu3,3 V, nie na GPIO. `RETRO_TFT_BACKLIGHT_CONTROL=0`; brak przydzielonego wyjścia podświetlenia jest chroniony błędem kompilacji przy próbie włączenia tej opcji. Przy wyłączonych zasilaniach przepiąć wyłącznie LCD CS z P21 na P4. Nowy pinout wymaga firmware v0.3.10; poprzednie wersje SPI nadal używają P21.

## XPT2046 — druga magistrala SPI

| Wyprowadzenie dotyku | GPIO ESP32 | Funkcja |
|---|---|---|
| T_IRQ | **GPIO36 / VP** | wejście, aktywne LOW; pull-up ok. 10 kΩ do 3V3, jeżeli nie ma go na module |
| T_DO | **GPIO39 / VN** | wejście ESP32, SPI2 MISO |
| T_DIN | **GPIO13** | SPI2 MOSI |
| T_CS / T_CS_ | **GPIO33** | wybór XPT2046 aktywny LOW |
| T_CLK | **GPIO14** | SPI2 SCLK |

GPIO36/39 są tylko wejściami i nie mają wewnętrznych pull-up. LCD i dotyk mają oddzielne zegary, MOSI i MISO: **nie zwierać** T_CLK z SCK, T_DIN z SDI ani T_DO z SDO w tym profilu. Osobne SPI2/SPI3 pozwalają niezależnie obsłużyć kontrolery i ich wyjścia danych. Żadna nowa linia LCD/dotyku w tym profilu nie używa strapów ESP32. EN HC-05 na GPIO12 nadal podlega starszym ograniczeniom.

`xpt2046.c` wykonuje wstępny odczyt surowych 12-bitowych X/Y oraz wskaźnika nacisku: mode 0, PD=00 i ponowne włączanie PENIRQ, odrzucenie pierwszego pomiaru osi, mediana trzech próbek. Poziom IRQ jest sprawdzany w zadaniu TFT, bez osobnego ISR GPIO. Transakcje SPI korzystają z kolejki przerwań SPI, z limitem 20 ms na zakończenie. Po timeoutcie statyczny deskryptor pozostaje ważny i nie jest używany ponownie do resetu. Błąd dotyku zatrzymuje jego odczyty i sygnalizuje fault LED; renderer i most pozostają aktywne.

Log `TOUCH: DOWN/UP raw x=... y=... pressure=...` potwierdzi reakcję po podłączeniu, ale **nie oznacza skalibrowanych pikseli**. Kalibracja, obrót współrzędnych i obsługa przycisków UI są przyszłym etapem; ta wersja nie steruje terminalem dotykiem. Podzbiór ANSI/VT100 i źródło obrazu (kopia UART2 RX) nie zmieniają się.

## Zachowany I80

Profil I80 zachowuje [poprzedni przydział](PIN_ASSIGNMENT_HC05_TFT.md): D0..D7=13/14/18/19/21/22/23/27, WR33, DC15, CS5, RD0, RST4; bez dotyku i sterowania podświetleniem. RD pozostaje HIGH, nie odczytujemy ID przez niezbadane HC245. Test samego zasilania tego shielda 15.11.2026 wykazał brak podświetlenia. Dodanie wariantu SPI nie naprawia ani nie identyfikuje tej usterki.

## Renderer i plan sprawdzenia

Wspólny renderer używa tego samego sterownika ILI9341 Espressif 2.1.0, modeli PSRAM i bufora INTERNAL DMA 5120 B w poziomie. I80 zamienia bajty RGB565 sprzętowo, SPI programowo przed transmisją. Zakończenie DMA musi nastąpić przed ponownym użyciem bufora; timeout 250 ms zatrzymuje rysowanie z zachowaniem zasobów. Wewnętrzne operacje SDK `esp_lcd` nie mają wszędzie kontrolowanych timeoutów; nie deklarujemy pełnej odporności na awarię peryferium.

Kolejność zlecona 15.11.2026: zakończyć zmiany, udana kompilacja wariantów, commit/push v0.1.12 **przed testami**, następnie kontrole hosta i zapis wyników. Próba fizyczna wymaga połączenia właściwego modułu i jawnego uploadu: wzór RGB/biały/czarny, tekst/ANSI, obrót, reakcje IRQ i surowe dane dotyku, równoległy ruch UART. Wyniki są w [STATUS](STATUS.md).

Zapis na SD jest [zaplanowany oddzielnie](FUTURE_SD.md), obecnie wyłączony i bez sterownika karty.

Źródła referencyjne: [Espressif: przykład SPI LCD + touch, release/v6.0](https://github.com/espressif/esp-idf/blob/release/v6.0/examples/peripherals/lcd/spi_lcd_touch/main/spi_lcd_touch_example_main.c), [API SPI LCD dla ESP32](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-reference/peripherals/lcd/spi_lcd.html), [autor sterownika XPT2046 — protokół](https://github.com/atanisoft/esp_lcd_touch_xpt2046/blob/main/esp_lcd_touch_xpt2046.c). Nowy moduł XPT2046 napisano lokalnie na podstawie opisu protokołu; nie dodawano zależności LVGL ani kodu Arduino.
