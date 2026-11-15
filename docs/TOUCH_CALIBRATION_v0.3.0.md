# XPT2046 i kalibracja — v0.3.0

**Rozszerzenie v0.3.1:** [pauza/wznowienie, tryb CR i kalibracja z prawej części paska](DISPLAY_CONTROLS_v0.3.1.md). Poniższa procedura pięciu punktów pozostaje aktualna; wejście do ponownej kalibracji odbywa się teraz przez przytrzymanie CAL po prawej.

Kolejny etap wybrany przez użytkownika po zaakceptowaniu obrotu LCD o180°. UART1 z HC-05 nadal odbiera logi, UART2 nie jest uruchamiany. Dotyk korzysta z osobnej magistrali SPI2. RAM wewnętrzny pozostaje działającą konfiguracją; PSRAM nie została naprawiona.

## Połączenia

Wyłączyć zasilanie przed zmianą przewodów. LCD i dotyk są na module zasilanym3,3 V zgodnie z deklaracją użytkownika, ze wspólną masą.

| Pin modułu | ESP32 |
|---|---|
| T_CLK | GPIO14 |
| T_DIN | GPIO13 |
| T_DO | GPIO39 / VN |
| T_CS | GPIO33 |
| T_IRQ | GPIO36 / VP |

Nie łączyć tych linii z SCK/MOSI/MISO LCD (18/23/19). GPIO36 i39 są wejściami bez wewnętrznych pull-up. T_IRQ jest aktywne LOW; jeżeli moduł nie ma własnego podciągania, potrzebne jest ok.10 kΩ do3,3 V. Nie podawać5 V na sygnały. GPIO16/17 i Flash6–11 pozostają zarezerwowane.

## Profil i przebieg

`pio run -e esp32dev_touch` kompiluje SPI LCD z XPT2046, RAM wewnętrznym i nową kalibracją. Upload przez zadanie VS Code `PIO: Upload SPI TFT + touch calibration`. Ten profil ma osobny `sdkconfig.terminal_touch` z `sdkconfig.touch.defaults`. Zwykły `esp32dev_internal` nadal ma dotykOFF. Wgrywanie profilu z dotykiem do próby fizycznej po potwierdzeniu jego okablowania.

1. Po powitaniu3 s, gdy nie ma pasujących ustawień, pojawi się ekran kalibracji.
2. Przytrzymać czerwony krzyżyk do komunikatu „PUSC EKRAN”, potem puścić. Powtórzyć dla pięciu punktów. Samo trzymanie palca nie zatwierdza następnego punktu.
3. Dotknąć dodatkowego punktu „SPRAWDZENIE DOKLADNOSCI”. Nie brał udziału w dopasowaniu. Błędna walidacja ponawia kalibrację, bez nadpisania zapisanych ustawień.
4. Na ekranie testowym zielony znacznik ma podążać za dotykiem. „GOTOWE” wraca do terminala, który przez cały czas przetwarzał logi w tle.
5. Przytrzymanie dolnego paska terminala przez2 s ponownie otwiera kalibrację. Przy kolejnym starcie poprawne zapisane ustawienia pomijają kalibrację.

## Model, zapis i ograniczenia

`touch_calibration.c` dopasowuje transformację afiniczną z pięciu par współrzędnych. Ujmuje skalę, zamianę osi, odwrócenie i niewielkie przekoszenie; punkty docelowe są już współrzędnymi aktualnego obrazu. Nie nakłada dodatkowego obrotu po dopasowaniu. Punkty zdegenerowane i błąd dopasowania ponad8 px są odrzucane. Niezależna walidacja dopuszcza błąd do12 px na oś. Odczyt stabilnego dotknięcia wymaga co najmniej8 próbek i100 ms; zwolnienie potwierdzają3 kolejne odczyty.

Zapis używa wyłącznie klucza `retro_touch/affine` w NVS. Rekord zawiera wersję, rozmiar obrazu, obrót, współczynniki i CRC32. Zmieniony obrót lub nieprawidłowy rekord wymaga nowej kalibracji. Nie kasujemy istniejącej partycji ani innych przestrzeni NVS. Błąd NVS pozostawia kalibrację tylko w RAM; ekran informuje „ZAPIS TYLKO DO RESETU”.

Profil dotykowy włącza oficjalne `CONFIG_UART_ISR_IN_IRAM`, ponieważ zapis NVS korzysta z Flash i czasowo wyłącza cache. ISR UART i bufory wewnętrzne mogą obsługiwać odbiór podczas tych operacji. Nie oznacza to nieograniczonej gwarancji braku strat: ring RX ma4096 B, nie ma RTS/CTS, błędy/przepełnienia są liczone. Ręczne buildy z dotykiem bez ISR UART w IRAM nie zapisują nowych ustawień do NVS, aby nie wprowadzać tej przerwy w odbiorze. [Ograniczenia Flash Espressif](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-reference/peripherals/spi_flash/spi_flash_concurrency.html), [NVS](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-reference/storage/nvs_flash.html).

Kalibracja i test dotyku nakładają osobny widok na terminal. Model tekstu nadal działa; po „GOTOWE” cały obraz terminala jest odrysowywany. Historia scrollback i sterowanie terminalem gestami nie są częścią tego etapu. Błąd SPI zatrzymuje dotyk i sygnalizuje fault, zachowując odbiór i obraz terminala.

## Kontrole

Nowy test hosta `python tests/run_host_tests.py --suite calibration` sprawdza transformację zamienionych/odwróconych osi, zdegenerowane punkty, duży błąd, granice zaokrągleń, bramkę zwolnienia, niezależną walidację, zapis/odtworzenie NVS z symulacją, CRC, zmianę obrotu, długie przytrzymanie i granice renderowania. Nie zastępuje fizycznego pomiaru XPT2046. Dotychczasowe testy protokołu XPT2046 pozostają osobne.

Kolejność: kompilacja, publikacja wersji, testy i osobny commit wyników. Potwierdzenie faktycznego podłączenia oraz kalibracja przez użytkownika są wymagane do opisu „sprawdzone na sprzęcie”; aktualny wynik w STATUS.
