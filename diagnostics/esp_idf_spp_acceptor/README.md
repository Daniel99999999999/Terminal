# Oryginalny akceptor SPP Espressif — porównanie dla Androida

To osobny projekt z **niezmienionym `main/main.c` z ESP-IDF v6.0.1**. Pochodzenie i hashe plików zapisano w [UPSTREAM.json](UPSTREAM.json). Oryginalne opisy są w [UPSTREAM_README.md](UPSTREAM_README.md) i [ESP32_SSP.md](ESP32_SSP.md). [Kod opublikowany przez Espressif](https://github.com/espressif/esp-idf/blob/v6.0.1/examples/bluetooth/bluedroid/classic_bt/bt_spp_acceptor/main/main.c).

Kod serwera, komponent CMake oraz Kconfig są identyczne bajtowo z przykładem. Dostosowano wyłącznie opakowanie PlatformIO, jawną wersję projektu i konfigurację WROVER skopiowaną z bazy v0.1.8. Zachowano ESP-IDF 6.0.1, BR/EDR-only, PSRAM, zegar, 4 MB flash i COM10. Program nie uruchamia naszych kolejek UART2, LED ani obserwatora HCI. Nie przypisuje GPIO16/17 do urządzeń zewnętrznych.

**Używa oryginalnego kontrolera ESP-IDF 6.0.1**, tak jak przetestowany terminal v0.1.8. Nie dołącza lokalnej poprawki Realtek z głównego projektu v0.1.9. Dzięki temu porównanie dla Androida dotyczy aplikacji referencyjnej i terminala przy tej samej wersji kontrolera. Poprawka Windows jest oddzielną zmianą.

## Wgranie i Android

**03.11.2026: agent wgrał ten program na COM10**, upload SUCCESS / hash flash zgodny. Odczyt UART potwierdził `bt_spp_acceptor_demo`, `ref-idf-6.0.1`, ELF `f54c58dd7...` oraz INIT/START. [Wyciąg z odczytu](../../docs/logs/2026-11-03_reference_boot_excerpt.txt). Użytkownik potwierdził: **nadal niewidoczny na Samsungu**. Poprzedni negatywny wynik z logiem `Terminal 0.1.9` dotyczył głównego terminala.

1. Zakończyć monitor COM10. W VS Code: **Terminal → Run Task → BT reference: Upload**. Następnie uruchomić istniejący **PIO: Monitor (115200)** i zresetować ESP32.
2. Sprawdzić w boot `App version: ref-idf-6.0.1`, potem tag `SPP_ACCEPTOR_DEMO`, `ESP_SPP_INIT_EVT` i `ESP_SPP_START_EVT`. Nie oczekiwać bannera RetroTerminal ani logów HCI_SCAN.
3. Wyłączyć BT Windows i wyszukać w systemowym ekranie Samsunga nazwę **ESP_SPP_ACCEPTOR**. Jeśli telefon pokazuje kod porównania, porównać go z linią `ESP_BT_GAP_CFM_REQ_EVT` na UART i potwierdzić na telefonie. Przykład automatycznie potwierdza żądanie po stronie ESP32. Jeżeli wystąpi osobny legacy PIN request, oryginalny przykład wypisuje właściwy PIN w logu.
4. Po sparowaniu można otworzyć połączenie SPP w aplikacji [Serial Bluetooth Terminal](https://github.com/kai-morich/SimpleBluetoothTerminal) (Bluetooth Classic). Oczekiwane na ESP32: `authentication success` i później `ESP_SPP_SRV_OPEN_EVT status:0`.

Ten przykład jest odbiornikiem/pomiarem prędkości, **nie implementuje echa ani mostu UART2**. Brak odesłanych znaków nie oznacza więc błędu połączenia. Wyniki buildu i startu na płytce są w [TESTS.md](../../docs/TESTS.md); skan zakończył się niewidocznością, połączenia SPP nie uzyskano.

## Ostatnia próba — pełna kalibracja RF, wynik negatywny

Środowisko **rf_fullcal** dziedziczy tę samą platformę 7.0.1 / SDK 6.0.1, ten sam oryginalny kod C, PSRAM i parametry BT. Osobny `sdkconfig.rf_fullcal` wyłącza `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE`; SDK wykonuje wtedy pełną kalibrację PHY bez ładowania i zapisywania danych kalibracji NVS. Pozostałe zmiany to oznaczenie wersji aplikacji **ref-fullcal-6.0.1**. Nazwa w skanie nadal **ESP_SPP_ACCEPTOR**. NVS nie jest kasowana.

Build SUCCESS / 142,57 s, upload na COM10 SUCCESS / 18,92 s. Odczyt boot potwierdził `ref-fullcal-6.0.1`, ELF `3769a91ad...`, INIT/START. [Log](../../docs/logs/2026-11-03_reference_fullcal_boot.txt). Użytkownik potwierdził **nadal niewidoczny**. To koniec prób wbudowanego BT na polecenie użytkownika; dalsza praca to TFT i HC-05.

Windows na tym samym wariancie również nie sparował urządzenia: ACL status=0, a po 30,13 s rozłączenie 0x22; brak AUTH success/SRV_OPEN. [Wyciąg z logu użytkownika](../../docs/logs/2026-11-03_reference_fullcal_windows_excerpt.txt). Nie jest to próba poprawionej biblioteki Realtek, ponieważ referencja celowo korzysta z oryginalnego kontrolera 6.0.1. Nie wykonujemy kolejnych prób BT.

[Oficjalna procedura pełnej kalibracji Espressif](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-guides/RF_calibration.html). Sposób odtworzenia zapisano poniżej jako historię, nie jako kolejny zlecony test:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d diagnostics/esp_idf_spp_acceptor -e rf_fullcal -t upload
```

Alternatywnie, z katalogu głównego projektu:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d diagnostics/esp_idf_spp_acceptor -e esp32dev -t upload
```

## Interpretacja i powrót

- Referencja widoczna/paruje, terminal v0.1.8 nie: problem zawężony do różnic aplikacji/jej ustawień. Zacząć od kolejności START/nazwy/discoverable i obsługi interakcji parowania; przenosić pojedyncze różnice z działającej referencji.
- Referencja również niewidoczna: wynik nie potwierdza błędu naszych kolejek/parsera/LED, bo ich tu nie ma. Pozostają wspólny SDK/kontroler, stan urządzeń i RF; nadal nie jest to dowód uszkodzenia płytki.
- Widoczna, lecz nie paruje: zapisać konkretny event i status AUTH/CFM/PIN z tego przykładu; nie utożsamiać wykrywania z parowaniem ani z otwarciem SPP.

Powrót do głównego terminala v0.1.9: zamknąć monitor i uruchomić zwykłe **PIO: Upload**. Root source i jego konfiguracja nie są nadpisywane przez build referencji. Starsze bazy są w `baseline/`.
