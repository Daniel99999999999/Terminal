# v0.1.14 — ekran powitalny i test UART

> Aktualizacja v0.1.19: późniejsze próby noflash i bariery cache były nieskuteczne i zostały usunięte. Bieżący start to standardowe SDK i Flash QIO/40 MHz; skuteczność wymaga logu. Użytkownik skorygował wcześniejszą informację o odłączeniu LCD: faktyczna izolacja nastąpiła dopiero na v0.1.18 i nadal występował bootloop. Aktualne wyniki w STATUS. Sekcja PSRAM poniżej opisuje historyczną próbę v0.1.14, nie finalną naprawę.

Po inicjalizacji LCD firmware rysuje biały tekst na czarnym tle: nazwę ESP32 RetroTerminal, rzeczywistą wersję aplikacji, UART / HC-05 oraz ANSI / VT100. Pełny ekran pozostaje widoczny przez **3 sekundy od zakończenia ostatniego transferu DMA** (z dokładnością planisty), następnie renderer pokazuje bieżący model terminala. UART i parser działają podczas powitania; nie czyścimy odebranych danych. Osobny test kolorów jest domyślnie wyłączony; `RETRO_TFT_COLOR_TEST=1` uruchamia go po powitaniu. Dotyk pozostaje OFF.

## Start PSRAM

Fizyczny test v0.1.13 nie powiódł się: nowy wyjątek był w `s_psram_chip_init:214`, po powrocie z implementacji PSRAM, przed app_main. Dopasowane ELF/bin/map i log zachowano. Wywołujący kod i jego literały nadal znajdowały się w Flash. W v0.1.14 lokalny linker fragment obejmuje także obiekty `esp_psram` i `cpu_start`, aby ścieżka powrotu przez konfigurację MSPI nie wymagała dostępu do Flash. Kontrola ELF obejmuje teraz także `s_psram_chip_init`, `esp_psram_chip_init`, `esp_psram_init`, `mspi_init`. Nie zmieniamy SDK, zegara ani pinów PSRAM. Skuteczność wymaga uruchomienia; wcześniejsze kontrole samych helperów nie gwarantowały poprawnego startu.

## COM14

Użytkownik udostępnił drugi HC-05 przez USB–UART **COM14**, łączący się radiowo z HC-05 na **UART1 ESP32 (TX32/RX34)**. COM5 to konsola/programowanie ESP32. Oba porty 115200 8N1. Test nie zmienia konfiguracji AT ani połączeń GPIO.

`tests/serial_smoke.py` odczytuje bazowe liczniki, wysyła oznaczony tekst oraz dzielone ANSI/UTF-8 i sprawdza przyrost RX UART1 / TX FIFO UART2 bez błędów/strat. Wyniki, payload i logi zapisuje w `.pio/audit/serial_smoke`. Brak raportu działającego mostu przerywa test przed wysłaniem danych.

```powershell
& "$env:USERPROFILE/.platformio/penv/Scripts/python.exe" tests/serial_smoke.py --console COM5 --peer COM14
```

To sprawdza kierunek **PC → HC-05 → UART1 → UART2 TX FIFO**, nie zawartość na fizycznym przewodzie UART2 ani obraz TFT. TFT interpretuje kopię **UART2 RX (GPIO26)**. Do pełnego testu ekranu potrzebny jest nadajnik na UART2 lub kontrolowana pętla TX25→RX26, przy odłączonym innym nadajniku RX26. Nie wprowadzamy echa ani zmiany kierunku modelu w firmware tylko na potrzeby testu.

Kolejność: kompilacja, commit/push nowszej wersji, kontrole i zatwierdzony upload/test, potem osobny commit wyników w STATUS. Publikacja źródeł nie oznacza potwierdzenia startu ani obrazu LCD.
