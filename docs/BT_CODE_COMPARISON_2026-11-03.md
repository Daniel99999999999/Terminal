# Porównanie kodu Bluetooth i poprawka kontrolera — 03.11.2026

## Wynik

Znaleziono **udokumentowaną regresję w używanej bibliotece kontrolera ESP32**. Zainstalowane ESP-IDF 6.0.1 zawiera dokładnie archiwum sprzed poprawki [Espressif #18145](https://github.com/espressif/esp-idf/issues/18145). Zgłoszenie dotyczy problemów SPP z niektórymi urządzeniami Realtek. Odczyt lokalnego PnP potwierdził **Realtek Bluetooth Adapter**, VID_0BDA/PID_4853, w komputerze użytkownika; zgłoszenie dotyczyło również innych identyfikatorów urządzeń tego producenta.

Oficjalny [commit ESP-IDF 83ede087025eb3edc5ccad83bfa4cf064ca6b8cc](https://github.com/espressif/esp-idf/commit/83ede087025eb3edc5ccad83bfa4cf064ca6b8cc) łagodzi kontrolę TID przy Encryption Pause Resume, aby współpracować z częścią urządzeń Realtek. Aktualizuje tylko submoduł kontrolera z `6a3c709f402f5a39a056a2097f3997dc4b8436cd` na `10f4e9ad6260788765863ed7f05e0e649b288e36`; zmienionym plikiem jest `esp32/libbtdm_app.a`. Pozostałe wymienione zmiany dotyczą eSCO i informowania hosta o automatycznie odrzuconym żądaniu połączenia.

**Potwierdzono obecność starej biblioteki i jej zgodność bajtową z wersją upstream. Nie potwierdzono jeszcze, że ten błąd jest przyczyną naszego timeoutu 0x22 ani niewidoczności na Samsungu.** Poprawka jest konkretnym backportem upstream, nie kolejną zmianą nazwy/klasy lub poziomu logowania. Nie ma podstaw do uznania płytki za uszkodzoną na podstawie dotychczasowych wyników.

## Porównanie z oficjalnym C

### Publiczny kod działający z Androidem

Kod istnieje i nie wymaga Arduino ani zamiany SPP na BLE:

1. [Espressif bt_spp_acceptor, C / ESP-IDF v6.0.1](https://github.com/espressif/esp-idf/tree/v6.0.1/examples/bluetooth/bluedroid/classic_bt/bt_spp_acceptor) — producent opisuje Android/PC jako klientów tego serwera. Niezmienione C, komponent CMake i Kconfig skopiowano do [osobnego projektu](../diagnostics/esp_idf_spp_acceptor/README.md); to następna próba Androida.
2. [nopnop2002/esp-idf-uart2bt — wariant Classic](https://github.com/nopnop2002/esp-idf-uart2bt/tree/397a1138bfa2f5680be18f4e5f9d85ab2a422751/classic_bt) — publiczny most UART/SPP z instrukcją Androida i zrzutami ekranu. Przeczytano `bt_spp_server.c` oraz `main.c`, nie tylko README.
3. [kai-morich/SimpleBluetoothTerminal](https://github.com/kai-morich/SimpleBluetoothTerminal) — kod klienta Android dla Classic SPP / UUID 0x1101, nie kod firmware ESP32.

Kod społecznościowy używa tych samych AUTHENTICATE/SLAVE/ERTM oraz inicjalizacji enhanced_init; nazwę/scan ustawia po START, używa DisplayYesNo i akceptuje CFM, ma też legacy PIN. To te same istotne różnice co w przykładzie producenta. Jego domyślne GPIO16/17 są **nieodpowiednie dla naszego WROVER** i nie zostały skopiowane; dodatkowo bridge usuwa CR, więc nie jest przezroczystym transportem bajtów w naszej definicji. Nie wdrożono jego kodu do terminala.

Poprawka Realtek w głównym v0.1.9 **nie jest rozpoznaniem problemu Androida**. Referencja Androida jest niezależnym programem i korzysta z oryginalnego kontrolera 6.0.1, zgodnego z przetestowanym v0.1.8. Nazwa w telefonie: **ESP_SPP_ACCEPTOR**, nie ESP32-RetroTerminal.

Porównano `src/main.c` z [akceptorem SPP ESP-IDF v6.0.1](https://github.com/espressif/esp-idf/blob/v6.0.1/examples/bluetooth/bluedroid/classic_bt/bt_spp_acceptor/main/main.c) oraz lokalnym odpowiednikiem z zainstalowanego SDK (SHA256 `dbd5b141e9dc576599ade2e3a3c7ccf215f9aa10f37e972ddc91087124f7a117`). Sprawdzono też implementacje GAP, `btc_dm_sp_cfm_req_evt`, inicjalizację Bluedroid i efektywny sdkconfig.

| Element | Oficjalny przykład / nasz kod | Ocena |
|---|---|---|
| Kontroler i pamięć BLE | BR/EDR-only, zwolnienie BLE przed init, zgodny tryb enable | zgodne; efektywny mode=2 |
| Bluedroid | domyślna konfiguracja SSP=1, SC=0 | zgodne; nie włączamy niedostępnego SC w ciemno |
| SPP | enhanced_init, callback mode, ERTM=true, bufor VFS=0 | zgodne; ERTM nie jest samodzielnie dowiedzioną przyczyną |
| Serwer | AUTHENTICATE, SLAVE, automatyczny SCN | zgodne |
| Nazwa i discoverable | przykład po START, u nas po INIT przed start_srv | różnica i możliwe okno przy starcie; sama nie wyjaśnia utrzymywania się objawu po minutach, skoro HCI potwierdził 0x03 |
| IO capability | przykład DisplayYesNo, u nas NoInputNoOutput | adaptacja do braku UI; nie ma podstaw kopiować DisplayYesNo bez potwierdzania przez użytkownika |
| CFM / PIN | przykład akceptuje CFM i ma legacy PIN, u nas odrzucenie nieobsługiwanych interakcji | ograniczenie kompatybilności; nie dowiedziono jego uruchomienia w pokazanych logach |
| Just Works | lokalny Bluedroid automatycznie potwierdza przed callbackiem aplikacji | sam brak CFM w logu nie oznacza błędu |
| UART / TX / LED | nasza dodatkowa logika zaczyna przesyłanie SPP po SRV_OPEN | w pokazanym przypadku SRV_OPEN nie wystąpił; brak dowodu, że to ta logika blokuje parowanie |

Nie znaleziono potwierdzonego błędu w aplikacyjnym C, który sam wyjaśniałby wszystkie objawy. Zmienianie wszystkich różnic względem przykładu naraz utrudniłoby ustalenie przyczyny. W v0.1.9 **zachowano dotychczasową logikę C**; poza oznaczeniami wersji/`btfix` zmienia się wyłącznie linkowana biblioteka kontrolera.

Zgłoszenie [#18634](https://github.com/espressif/esp-idf/issues/18634) dotyczy otwierania portu COM po udanym parowaniu. To inny etap niż brak potwierdzonego AUTH w naszych logach; nie uznajemy go za rozpoznanie obecnego błędu. Zgłoszenia BLE i ESP32-S31 nie dotyczą tej konfiguracji klasycznego ESP32.

## Dowód wersji biblioteki

Metadane pobrano z publicznego GitHub API Espressif. Git blob SHA1 obliczono ze wszystkich bajtów lokalnego archiwum wraz z nagłówkiem obiektu Git, a nie z nazwy pliku czy deklarowanej wersji SDK.

| | Przed poprawką / zainstalowana | Poprawka projektu |
|---|---|---|
| Commit esp32-bt-lib | `6a3c709f402f5a39a056a2097f3997dc4b8436cd` | `10f4e9ad6260788765863ed7f05e0e649b288e36` |
| Git blob SHA1 | `c010e165274ddbeca2ec45030cf273cc227676ac` | `4600c8f40b4ec887570ccf7defc40241ea306f12` |
| Rozmiar | 893 882 B | 896 358 B |
| SHA256 | `9552f29e7e58823d3d62ecf6310d89c110239da41655d0783be80ac77829e7db` | `dc4d03d1ace55a3399cb3104284b3d1c5b76fc0931212ed999d4d8f59414bb19` |

Pochodzenie, linki i hashe zapisano w `vendor/esp32-bt-lib/10f4e9ad6260788765863ed7f05e0e649b288e36/UPSTREAM.json`; dołączono niezmienioną licencję upstream. `cmake/bt_controller_compat.cmake` po utworzeniu targetów SDK podmienia `IMPORTED_LOCATION` targetu `bt_btdm_app`. Sprawdza układ ESP32, SDK 6.0.1 i SHA256 starego/nowego archiwum. Build nie pobiera niczego z internetu.

**Globalna instalacja SDK nie została podmieniona.** Mapa końcowego linkowania zawiera tylko archiwum z `vendor/...`, a SHA256 globalnego oryginału pozostał identyczny. Dokładna baza poprzedniego kodu jest w [baseline/v0.1.8](../baseline/v0.1.8/README.md), manifest zawiera 18 plików; wcześniejsze bazy także pozostały zachowane.

## Dotychczasowe wyniki sprzętowe

- [Pełny log v0.1.8 z telefonu](logs/2026-11-03_v0.1.8_android_hci.txt): poprawny WRITE_SCAN_ENABLE 0x03 i dopasowany Command Complete status=0; attached=1/drops=0/trace_complete=1/pending=0/confirmed=1. Do 126757 ms brak późniejszego wyłączenia/resetu/ACL/AUTH/SRV_OPEN. Nie jest to niezależny READ ani pomiar RF. Samsung nie znalazł ESP32, ale użytkownik potwierdził wykrywanie innego znanego urządzenia.
- [Wyciąg Windows](logs/2026-11-03_v0.1.8_windows_excerpt.txt): rozłączenie handle=128 przy 405017 ms, HCI 0x22; początek tego łącza nie jest pokazany. Następne ACL connect status=0, handle=129 przy 412847 ms, ostatnia linia przy 422347 ms. Wynik drugiego łącza nie jest znany — nie łączyć tych dwóch handle w jedną próbę.
- Udane ACL dowodzi przynajmniej chwilowej wymiany radiowej. Timeout LMP nie identyfikuje konkretnego PDU/procedury, która zawiodła. Brak AUTH completion w wycinku nie dowodzi, że procedura parowania w ogóle się nie rozpoczęła.
- UART RX=41/offline=41 oznacza odbiór 41 B i ich odrzucenie bez aktywnego SPP; brak źródła/payloadu nie pozwala zaliczyć testu terminala UART.

## Weryfikacja v0.1.9 i próba użytkownika

Build: **SUCCESS**, kod 0, 63,93 s; Flash **695 916 B**, statyczny RAM **46 076 B**. Zweryfikowano mapę linkowania, hashe, pozostawienie hooków HCI/LED oraz brak zmian w konfiguracji i logice aplikacji. Nie powtarzano testów hostowych dla samej podmiany binarnego kontrolera — nie modelują one tej regresji. Poprzednie wyniki dotyczą niezmienionych modułów aplikacji.

Użytkownik uruchomił v0.1.9 (`Terminal / ESP32-Terminal`, ELF `371e4d797...`, kontroler `003f89d`, btfix=83ede08); Samsung nadal go nie widzi. [Pełny log](logs/2026-11-03_v0.1.9_android_not_reference.txt). Następnie agent wgrał oryginalną referencję i potwierdził `ref-idf-6.0.1`, ELF `f54c58dd7...`, INIT/START. Użytkownik potwierdził, że również referencja pozostaje niewidoczna. To występowanie objawu bez naszych modułów aplikacji, nie dowód uszkodzenia RF. Poniższa procedura Windows pozostaje niewykonana; użytkownik zdecydował o końcu prób wbudowanego BT po jednej próbie pełnej kalibracji i przejściu do LCD / HC-05:

1. Zakończyć monitor, wykonać **PIO: Upload**, uruchomić monitor i reset. Oczekiwać **v0.1.9 / btfix=83ede08**, nie v0.1.8. SDK nadal zgłasza 6.0.1, bo poprawka jest lokalnym backportem.
2. Wyłączyć BT telefonu na czas próby Windows. Jeśli istnieje stary wpis tego ESP32, usunąć tylko ten wpis i wykonać świeże dodawanie/parowanie. Nie kasować całej NVS ani ustawień pozostałych urządzeń.
3. Sprawdzić, czy dodawanie się kończy i pojawia się `GAP AUTH status=0`. Dopiero potem otworzyć odpowiedni port SPP i oczekiwać `SPP SRV_OPEN status=0` / echo. Samo parowanie Windows nie musi otworzyć SPP.
4. Osobno sprawdzić systemowy skan Samsunga przy wyłączonym Windows BT. Zachować wynik, także jeśli poprawi się tylko Windows. Nie zakładać, że jeden błąd wyjaśnia oba objawy.

Późniejszy test zwykłej referencji i ostatniego wariantu pełnej kalibracji także nie poprawił widoczności na Androidzie. Windows na `ref-fullcal-6.0.1` zgłosił brak odpowiedzi; [log](logs/2026-11-03_reference_fullcal_windows_excerpt.txt) pokazuje 30,13 s od ACL do disconnect 0x22, bez AUTH success/SRV_OPEN. Referencja ma oryginalny kontroler `a5d37fe`, więc ten wynik nie rozstrzyga skuteczności lokalnej poprawki Realtek z głównego v0.1.9. Użytkownik zakończył diagnozę wewnętrznego BT; następne etapy to TFT i HC-05, bez kolejnych eksperymentów BT i bez deklarowania awarii RF.
