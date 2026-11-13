# Testy i dziennik pomiarów

## Dostępne środowisko

Windows, VS Code, PlatformIO `espressif32 7.0.1` / ESP-IDF 6.0.1, monitor UART0 = 115200. Ostatnio port COM10, ale może się zmienić.

### Komendy

```powershell
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
pio run -e esp32dev -t menuconfig
```

Powyższe komendy działają w terminalu, w którym dostępny jest `pio`. Są też zadania VS Code w `.vscode/tasks.json` używające standardowej ścieżki PlatformIO pod Windows.

## Tabela pomiarów / diagnoz

| Test | Wersja | Wynik | Uwagi |
|---|---|---|---|
| Boot ESP-IDF | v0.1.4 | OK | brak pętli restartów w logu |
| PSRAM boot test | v0.1.4 | OK | detected 8 MB / mapped 4 MB |
| UART2 initialization | v0.1.4 | OK | 25 TX / 26 RX, 115200; brak testu transmisji z podłączonym urządzeniem |
| SPP server starts | v0.1.4 | OK (software) | log listening |
| Android discovers SPP | v0.1.4 | NIE | nie pojawia się ESP32 w skanowaniu |
| BT pairing/echo | v0.1.4 | Nie testowano skutecznie | brak wykrycia |
| LCD RGB565 / I80 | — | Nie testowano | brak zdefiniowanego pinoutu |
| Touch resistance | — | Nie testowano | X+/X− i Y+/Y− na zdjęciu |

## Kontrole lokalne — 01.11.2026

| Kontrola | Wynik | Zakres |
|---|---|---|
| `platformio.exe run -e esp32dev` | SUCCESS, kod 0 | niezmieniony v0.1.4, espressif32 7.0.1 / ESP-IDF 6.0.1; bez uploadu |
| Cppcheck 2.11 | kod 0, bez zgłoszeń warning/performance/portability | `src/main.c`, baza kompilacji, model unix32, C11; bez modelowania współbieżności i RF |
| Porównanie SHA-256 po buildzie | zgodne z sumami z audytu | `src/main.c`, `platformio.ini`, `sdkconfig.esp32dev` |
| Nowe testy na ESP32 i Androidzie | nie wykonano | wyniki sprzętowe z wcześniejszej tabeli pozostają aktualne |

Szczegóły ustaleń i proponowane testy krótkich wiadomości UART, zmian sesji SPP oraz wykrywalności BT: [raport audytu](AUDIT_2026-11-01.md). Opisane tam scenariusze regresji są planem testów, nie wynikami ich wykonania.

## Kontrole v0.1.5 — 01.11.2026

| Kontrola | Wynik | Ograniczenie |
|---|---|---|
| Pełny build `esp32dev`, echo=1 | SUCCESS, kod 0; Flash 683 344 / 1 048 576 B (65,2%), statyczny RAM 45 956 / 327 680 B (14,0%); końcowy build przyrostowy 12,24 s | bez uploadu i pomiarów heapu na ESP32 |
| Kompilator Xtensa, echo=0 | kod 0, `-DSPP_TEST_ECHO=0 -fsyntax-only` | osobna kontrola kompilatora, bez linkowania/wgrywania wariantu mostu |
| `python tests/run_host_tests.py` | PASS: 16 scenariuszy | prawdziwy moduł `bt_tx_state.c`; brak modelu RTOS/UART/RF |
| Cppcheck 2.11 | kod 0 dla `main.c` i `bt_tx_state.c` | unix32/C11, warning/performance/portability; bez pełnego modelu Xtensa/C23 |
| v0.1.4 w `baseline/v0.1.4` | zachowana dokładna kopia źródła i konfiguracji | instrukcja powrotu w README bazy |

### Testy wymagające ESP32 i telefonu

Poniższych testów **jeszcze nie wykonano**. Użyć firmware v0.1.5; konsola UART0/USB 115200, UART2 TX GPIO25 / RX GPIO26 z logiką 3,3 V i wspólną masą. LCD pozostaje odłączony.

1. **Boot i GAP.** Po wgraniu zebrać log od resetu. Oczekiwać rzeczywistej wersji SDK i `echo=1`, `BT local address`, `BT SSP IO_CAP_NONE request=ESP_OK`, `SPP INIT status=0`, wyników set_name/scan_mode/get_name, `GAP GET_DEV_NAME ... ESP32-RetroTerminal` oraz `SPP START status=0 ... scn=...`. Komunikaty są asynchroniczne. Sam zestaw tych logów nie potwierdza radia.
2. **Wykrycie i echo.** Wykonać skan Classic na Androidzie; porównać adres z lokalnym adresem z logu. Po parowaniu/połączeniu oczekiwać GAP ACL/AUTH, `SPP SRV_OPEN status=0`, bannera v0.1.5 oraz identycznego echa `BT_TEST_001\r\n`. Sprawdzić także drugi odbiornik, jeśli pierwszy nie widzi ESP32. Nie kasować NVS w celu samego testu.
3. **Krótkie UART → BT.** Po logu `UART RX session boundary: handle=<aktywny uchwyt>` nadać na RX kolejno 1, 3, 127, 128, 129 i 257 B, z sekundowymi przerwami. Każda wiadomość ma dotrzeć w całości bez dopełniania do 128 B. Sprawdzić bajt po bajcie, w tym NUL i wartości 0x80–0xFF. Przy normalnych zdarzeniach oczekiwane szybkie przekazanie; awaryjne opróżnianie przy utraconym DATA następuje po oczekiwaniu do 100 ms plus czas planisty/BT.
4. **Sesje.** Nadać rozpoznawalny blok A, rozłączyć i ponownie połączyć, potem nadać blok B. Dane A nie mogą pojawić się w nowej sesji. Przez granicę połączenia dane mogą być celowo odrzucone; rozpoczynać B po logu granicy nowej sesji. Powtórzyć 30 razy, także podczas intensywnego echa.
5. **CLOSE podczas zapisu.** Przerwać połączenie podczas ciągłego nadawania. Jeżeli zapis był w toku, oczekiwać `SPP recovery: deinit request=ESP_OK`, `SPP UNINIT status=0`, ponownego INIT i START. W trakcie restartu nowy klient może być odrzucony; po START ponowić połączenie i sprawdzić echo. Zwykły CLOSE bez zapisu nie wymaga restartu. Nie oczekiwać zachowania danych będących w locie.
6. **Błędy UART i przeciążenie.** Użyć kontrolowanego nadawcy 3,3 V: rozbieżne baud/frame oraz strumień przewyższający możliwości odbioru. Co 5 s sprawdzać narastające `IO ... FIFO_OVF/BUFFER_FULL/FRAME/PARITY/READ` i liczniki odrzuceń. Przy 8N1 PARITY może pozostać 0. Sam niedopasowany baud nie gwarantuje błędu ramki przy każdym wzorze danych. Po zakończeniu przeciążenia i przywróceniu 115200 odbiór krótkiej wiadomości powinien wrócić. Nie wykonywać rutynowego flush/erase do odzyskania odbioru.
7. **Most w obu kierunkach.** Osobno zbudować z `SPP_TEST_ECHO=0` (makro ma teraz ochronę `#ifndef`, można przekazać build flag). Sprawdzić telefon → UART i UART → telefon. W tym wariancie nie ma bannera i lokalnego echa BT. Test hostowy/syntax-check nie zastępuje tego testu.

Brak WRITE lub trwałe congestion powodują po 3 s bezpieczny restart profilu, a nie wysłanie kolejnego pakietu bez potwierdzenia. Wstrzykiwanie błędu WRITE z uchwytem 0 i przeplot CLOSE/rezerwacja są objęte testami modułu na komputerze; nie dodano ukrytych przełączników zakłócających firmware produkcyjny. Jeśli restart profilu nie dostanie potwierdzeń, log wskazuje wyłączony TX i kolejne próby, a UART/diagnostyka mają nadal pracować.

Dokładność liczników, świadome odrzucanie danych na granicach sesji i brak gwarancji bezstratności przy przeciążeniu opisano w [FIXES_v0.1.5.md](FIXES_v0.1.5.md).

## Kontrole v0.1.6 — 02.11.2026

| Kontrola | Wynik | Zakres |
|---|---|---|
| `python tests/run_host_tests.py` | PASS: 16 BT + 10 LED/heartbeat | C11, `-Wall -Wextra -Werror`; bez urządzenia |
| Cppcheck 2.11 | kod 0, bez zgłoszeń warning/performance/portability | 4 pliki C projektu; unix32/C11, bez pełnego modelu Xtensa/RTOS |
| Wariant `SPP_TEST_ECHO=0` | kompilator Xtensa `-fsyntax-only`: kod 0 | bez osobnego linkowania / uploadu wariantu mostu |
| Kod błędu LED w pliku obiektowym | `debug_led_fault`, hook TWDT i wrapper abort w `.iram1.*`; znaczniki w `.dram1.*` | disassembly `debug_led_fault` bez wywołań innych funkcji |
| Pełny build i weryfikacja końcowego ELF | SUCCESS, kod 0; Flash 690 652 / 1 048 576 B (65,9%), statyczny RAM 46 012 / 327 680 B (14,0%); 181,09 s | espressif32 7.0.1 / ESP-IDF 6.0.1; firmware.bin 691 088 B |
| Końcowy ELF: IRAM/DRAM i wywołania | PASS: funkcja fault/hook/wrapper w `.iram0.text`, znaczniki w `.dram0.data`; `abort` wywołuje wrapper, cross-reference SDK TWDT wskazuje hook użytkownika, ścieżka fault nie wywołuje innych funkcji | sprawdzenie symboli, mapy linkera i ograniczonego do adresów funkcji disassembly; bez rzeczywistego panic na ESP32 |
| Test LED/LCD/touch na sprzęcie | nie wykonano | żadnego nowego uploadu ani zmian połączeń |

Jawna wersja `PROJECT_VER=0.1.6` pozwala konfigurować projekt również w obecnym repozytorium Git bez commitów. Nie wykonywano commitowania ani czyszczenia repozytorium. Baseline v0.1.4 jest zachowany.

### Plan sprawdzenia debug LED na płytce

1. Sprawdzić zgodność NodeMCU-32 v1.3 i GPIO2 ze schematem / połączeniem LED. Czerwona LED2 PWR powinna świecić niezależnie od programu; domyślne sterowanie dotyczy niebieskiej LED1, active HIGH. Odłączyć niezweryfikowany shield LCD przed pierwszym testem LED.
2. Po zbudowaniu i wgraniu v0.1.6 zebrać log `DEBUG_LED: GPIO2 active=1; toggle 500/1000 ms; reset_reason=... latched=0`. Bez telefonu oczekiwać 500 ms ON / 500 ms OFF. Tolerancja wynika z callbacku timera co 20 ms i pracy planisty.
3. Po udanym **SPP SRV_OPEN**, nie po samym sparowaniu, oczekiwać 1000 ms ON / 1000 ms OFF. Po zwykłym rozłączeniu wraca szybszy rytm. Jeśli Android nadal nie wykrywa modułu, ten krok pozostaje niezweryfikowany; nie uznawać go za zaliczony na podstawie testów hostowych.
4. Przerwać transfer SPP w trakcie zapisu. Podczas rzeczywiście zgłoszonego błędu/odzyskiwania usługi oczekiwać ON, a po udanym START powrotu migania. Szybki restart może dać bardzo krótki stan ON. Sam CLOSE bez zapisu w toku nie jest awarią i nie wymaga stałego światła.
5. W **oddzielnym buildzie testowym**, poza ścieżkami produkcyjnymi, wstrzymać heartbeat jednego zadania pozostawiając scheduler i timer aktywne. UART: po 2 s, BT: po 6 s oczekiwać logu maski stale i stałego ON. Powrót heartbeat nie kasuje zatrzasku; reset powinien go skasować. Nie dodano ukrytego polecenia do zawieszania produkcyjnego firmware.
6. Osobny test błędu: wywołać `debug_led_fault()` z testowego kodu, potem wykonać zwykły reset; sprawdzić powrót migania. Osobny test panic/TWDT powinien pokazać ON, a po automatycznym resecie `latched=1` i dalsze ON. Ręczny EN / ponowne zasilenie kasuje zatrzask, jeśli kolejny reset nie jest ponowną awarią.
7. Całkowite zatrzymanie CPU lub przerwań nie jest testem, dla którego program gwarantuje ON. Zatrzymanie wszystkich rdzeni przez debugger zatrzymuje również kod LED, a debugger może wyłączać watchdogi. Nie uznawać pozostałego OFF za dowód niesprawnej diody bez rozróżnienia tych warunków.

Na v0.1.6 powtórzyć testy UART/SPP v0.1.5 z uwzględnieniem bannera **v0.1.6**. Czasy oczekiwań SPP nie zostały skrócone na potrzeby heartbeat; UART TX nadal nie odpyta zajętości w pętli bez oczekiwania.

### Zatwierdzenie GPIO LCD i ADC dotyku

Plan rezerwuje X+ GPIO32/ADC1_CH4 i Y+ GPIO33/ADC1_CH5; X− GPIO13 i Y− GPIO14 warunkowo. Pomiary ciągłości, torów HC245, poziomów 3,3 V i strapów GPIO5/15 są wymagane przed zatwierdzeniem permutacji LCD. Opis złącza UNO i deklaracja odpowiednika ILI9341 nie są wynikiem tych pomiarów. Procedura: [LED_AND_PIN_PLAN.md](LED_AND_PIN_PLAN.md). Firmware nie konfiguruje jeszcze LCD ani ADC.

## Dostarczony test sprzętowy v0.1.6 — 02.11.2026

| Test | Wynik / zakres |
|---|---|
| Uruchomienie v0.1.6, SDK 6.0.1, PSRAM i UART2/SPP init | potwierdzone logiem użytkownika |
| Adres/nazwa | `7C:87:CE:2F:DE:DA`, `ESP32-RetroTerminal`; GET_DEV_NAME status=0 |
| Widoczność Windows | użytkownik potwierdza; błąd następuje przy dodawaniu/parowaniu |
| ACL Classic | dwa connect status=0; pierwsze zerwanie po 30,09 s, HCI 0x22 / GAP 290 |
| Zakończenie AUTH / otwarcie SPP | brak w dostarczonym logu; druga próba jest ucięta przed końcowym wynikiem |
| Samsung Galaxy A55 SM-A556B/DS / Android 16 | brak urządzenia w systemowym skanowaniu wg użytkownika |
| Echo / rzeczywiste dane UART / fizyczna sygnalizacja LED | niepotwierdzone tym logiem |
| Izolowany skan Samsunga przy wyłączonym BT Windows, po resecie ESP32, 30–50 cm | **nadal niewidoczny** wg odpowiedzi użytkownika; nowego logu UART nie dostarczono |

[Analiza, hipotezy i następne kroki](BT_LOG_ANALYSIS_2026-11-02.md), [zachowany pełny log](logs/2026-11-02_v0.1.6_windows_pairing.txt). W ramach analizy zmieniono wyłącznie dokumentację i zapisano materiał testowy; nie wykonano nowego buildu ani uploadu.

## Kontrole v0.1.7 — 02.11.2026

| Kontrola | Wynik | Zakres |
|---|---|---|
| Pełny build pierwotnego `BT_DISCOVERY_TEST=1` | SUCCESS, kod 0; Flash 692 704 / 1 048 576 B (66,1%), statyczny RAM 46 012 / 327 680 B (14,0%); 202,74 s | espressif32 7.0.1 / ESP-IDF 6.0.1; bez uploadu przez agenta |
| Pełny build bieżącego `BT_DISCOVERY_TEST=2` | SUCCESS, kod 0; Flash 692 972 / 1 048 576 B (66,1%), statyczny RAM 46 012 / 327 680 B (14,0%); 9,66 s | po negatywnym teście trybu 1; bez uploadu przez agenta |
| Tryby 0, 1 i 2 kompilatorem Xtensa | kod 0 dla `main.c` i `bt_discovery_diag.c` z `-fsyntax-only` | nie są to dodatkowe pełne linkowania trybów 0/2 ani testy radiowe |
| Testy hostowe stanu BT i LED | 16 + 10 PASS | logika produkcyjnych modułów, bez modelowania radia |
| Cppcheck 2.11 | kod 0 dla 5 plików C | unix32/C11, warning/performance/portability |
| Galaxy A55, v0.1.7 / tryb 1 | nadal niewidoczny wg użytkownika; INIT/START=0, CoD=0x000100, automatyczny CONFIG_EIR=0 z polami nazwa/UUID | pełny log do 261486 ms; brak ACL/AUTH/SRV_OPEN, błędów/recovery/resetów; zawartość UUID i odbiór radiowy niepotwierdzone |
| Galaxy A55, v0.1.7 / tryb 2 | nadal niewidoczny wg użytkownika; INIT/START=0, CoD=0x000100, jawny CONFIG_EIR=0, pola nazwa/UUID/flags | pełny log do 326456 ms; brak ACL/AUTH/SRV_OPEN, błędów/recovery/resetów; HCI Scan_Enable i odbiór RF niepotwierdzone |
| Windows na v0.1.7 | nie wykonano / nie dostarczono wyniku | wcześniejsze ACL / timeout dotyczyły v0.1.6 |

[Instrukcja i wyniki CoD/EIR v0.1.7](BT_DISCOVERY_TEST_v0.1.7.md), [log trybu 1](logs/2026-11-02_v0.1.7_identity1_samsung.txt), [log trybu 2](logs/2026-11-02_v0.1.7_identity2_samsung.txt). Obie próby potwierdziły lokalną konfigurację, ale nie poprawiły wykrywania. Następna weryfikacja dotyczy HCI Scan_Enable. Poprawne zwroty API i lokalny CoD nie potwierdzają, że Samsung odbiera dane. Wyniku Windows LMP timeout nie wolno uznać za naprawiony. Dla samej zmiany wyboru wariantu nie powtarzano 26 testów hostowych ani Cppcheck; późniejsza analiza logu trybu 2 nie zmieniła firmware i nie wymagała nowego buildu.

## Kontrole v0.1.8 — 02.11.2026

| Kontrola | Wynik / zakres |
|---|---|
| Build końcowego kodu z HCI observer=1 | SUCCESS, kod 0; 15,87 s po wcześniejszym pełnym buildzie bibliotek; Flash 695 164 / 1 048 576 B (66,3%), statyczny RAM 46 076 / 327 680 B (14,1%); espressif32 7.0.1 / ESP-IDF 6.0.1 |
| Host C11, Wall/Wextra/Werror | 16 BT + 10 LED + 13 HCI PASS; parser długości/kierunków, filtr, FIFO i błędy, reset/loss; 2 warianty adaptera (włączony/wyłączony) PASS |
| Przekazywanie adaptera ze stubami | oryginalne bufory i długości, oba callbacki, zwroty i błąd rejestracji zachowane; pełna kolejka śladu nie blokuje BT, brak logowania w ścieżce transportu |
| Target observer=0 | main.c / bt_hci_diag.c: Xtensa `-fsyntax-only`, kod 0; bez osobnego pełnego linkowania |
| Cppcheck 2.11 | kod 0 dla 7 C, unix32/C11, warning/performance/portability |
| Końcowe linkowanie | mapa: Bluedroid → oba wrappery → realne funkcje kontrolera; hooki LED/TWDT/abort nadal w IRAM |
| Hardware v0.1.8, wynik 03.11 | WRITE_SCAN_ENABLE=0x03 / status=0, pełny ślad bez drops; Samsung niewidoczny mimo widoczności innego urządzenia; Windows ACL=0 i LMP 0x22, bez potwierdzenia AUTH/SPP. To nie test stabilności RF. |

[Dokładna procedura i interpretacja HCI](BT_HCI_TEST_v0.1.8.md). Artefakty kontroli lokalnych: `.pio/host-tests/result.txt`, `.pio/audit/hci_v018_cppcheck.log`, `.pio/audit/hci-disabled-*.log`, `.pio/audit/hci_v018_elf_check.txt`. Nie wykonano uploadu. Baza v0.1.7 i jej manifest pozostają w `baseline/v0.1.7`.

## Kontrole 03.11.2026 — publiczny przykład Androida i backport Realtek

| Program / kontrola | Wynik |
|---|---|
| Oryginalny akceptor SPP Espressif w osobnym projekcie | SUCCESS, kod 0, 177,63 s; Flash 655 460 / 1 048 576 B (62,5%), RAM 45 812 / 327 680 B (14,0%); SDK 6.0.1 / platform 7.0.1 |
| Pochodzenie referencji | identyczne bajtowo upstream main/main.c, komponent CMake i Kconfig; SHA256 i tag w diagnostics/esp_idf_spp_acceptor/UPSTREAM.json |
| Główny v0.1.9 z lokalnym kontrolerem po poprawce #18145 | SUCCESS, kod 0, 63,93 s; Flash 695 916 / 1 048 576 B (66,4%), RAM 46 076 / 327 680 B (14,1%) |
| Kontrola podmiany biblioteki | oficjalny Git blob SHA1/SHA256, wyłącznie poprawiony plik w końcowej mapie v0.1.9, globalny oryginał nienaruszony; ABI pozwala na pełne linkowanie z SDK 6.0.1 |
| Kontrola aplikacji i baz | 18 hashy baseline/v0.1.8 PASS; app C zmieniony tylko w wersji/markerze; UART/LED/HCI/security/config zachowane, hooki IRAM zachowane |
| Główny v0.1.9 na Androidzie | użytkownik nadal nie widzi ESP32-Terminal; log Terminal / 0.1.9 / ELF 371e4d797... / kontroler 003f89d / btfix=83ede08, INIT/START i HCI 0x03; nie jest to próba referencji |
| Upload referencji na COM10 przez agenta | SUCCESS, kod 0, 17,04 s; aplikacja 655 888 B, hash flash zweryfikowany; nie kasowano całej flash/NVS |
| Boot referencji odczytany przez agenta | bt_spp_acceptor_demo / ref-idf-6.0.1 / ELF f54c58dd7... / kontroler a5d37fe, INIT/START; monitor zamknięty, COM10 zwolniony |
| Referencja — Android / AUTH / SRV_OPEN | użytkownik potwierdził nadal niewidoczna; brak potwierdzonego AUTH/SPP |
| Ostatni wariant rf_fullcal — kompilacja | SUCCESS, kod 0, 142,57 s; Flash 653 824 B / RAM 45 812 B / bin 654 256 B; ten sam C/SDK/kontroler, inna obsługa kalibracji i oznaczenie wersji |
| rf_fullcal — upload i start na COM10 | SUCCESS, kod 0, 18,92 s; hash flash zgodny; ref-fullcal-6.0.1 / ELF 3769a91ad..., INIT/START |
| rf_fullcal — skan Androida | użytkownik potwierdził nadal niewidoczny; koniec prób wbudowanego BT na jego polecenie |
| rf_fullcal — parowanie Windows | użytkownik potwierdził brak odpowiedzi; ACL status=0/handle128 przy 197984 ms, po 30,13 s disconnect 0x22, brak AUTH success/SRV_OPEN; oryginalny kontroler a5d37fe |

Dwa niezależne programy: [referencja Androida](../diagnostics/esp_idf_spp_acceptor/README.md) oraz [główny terminal v0.1.9 z poprawką Realtek](BT_CODE_COMPARISON_2026-11-03.md). Nie mylić zadań **BT reference: Upload** i **PIO: Upload**, nazw urządzeń i bibliotek kontrolera. Referencja ma starą bibliotekę 6.0.1 celowo do porównania z przetestowanym v0.1.8; nie jest testem skuteczności poprawki Realtek.

[Wyciąg z uploadu i startu referencji](logs/2026-11-03_reference_boot_excerpt.txt). Hostowe testy aplikacji nie modelują regresji binarnego kontrolera. Przy samym wgraniu istniejącego, niezmienionego obrazu nie powtarzano testów hostowych.

[Odczyt ostatniego wariantu](logs/2026-11-03_reference_fullcal_boot.txt). Porównano oba sdkconfig: zmieniono wyłącznie przełącznik kalibracji (i jego stary alias) oraz APP_PROJECT_VER/FROM_CONFIG. Wygenerowany config potwierdza wyłączony odczyt/zapis kalibracji. Sprawdzono trzy hashe plików upstream, niezmienność głównego main.c, bazowego config referencji i globalnego kontrolera. Nie powtarzano testów hostowych logiki terminala, której nie ma w referencji. TFT/HC-05 nie uruchomiono; przygotowano procedurę i arkusz pomiarów.

## Kontrole v0.1.10 — 13.11.2026, model ANSI/VT100

| Kontrola | Wynik i granice |
|---|---|
| Host C11 / Wall / Wextra / Werror | 16 BT + 11 LED + 13 HCI + 22 terminal = **62 PASS**, dodatkowo oba warianty adaptera HCI PASS |
| Parser/model terminala | domyślne biały/czarny; CR/LF/BS/TAB, ruch/edycja/czyszczenie, SGR 16/256/RGB, inverse, zapis kursora, scroll/origin, autowrap, alt, ramki DEC, UTF-8, ignorowanie ciągów sterujących, błędne/niepełne komendy |
| Odporność i porcjowanie | porównanie całego strumienia z każdym punktem podziału oraz porcjami po 1 B; 20 000 bajtów danych adversarial z kontrolą indeksów i wartowników obu buforów; skrajne współrzędne/parametry i siatka 1×1 |
| Nadzór nowego zadania | logika heartbeat: regularne zgłaszanie przy pustej kolejce jest zdrowe, brak heartbeat przez 2 s daje stale; bez fizycznego testu LED/TWDT |
| Cppcheck | 10 C projektu, baza kompilacji targetu / unix32 / C11; warning/performance/portability bez zgłoszeń, kod 0; bez pełnego modelowania RTOS |
| Końcowy build ESP32 | **SUCCESS**, kod 0; pełny build 204,38 s, końcowy przyrostowy 11,24 s po dodaniu nadzoru zadania; platform 7.0.1 / SDK 6.0.1 |
| Pamięć / obraz | Flash 702 260 / 1 048 576 B, RAM statyczny 46 692 / 327 680 B, bin 702 688 B; dwie strony modelu wymagają dodatkowo 46 080 B dynamicznej PSRAM; brak nowego pomiaru heapu na sprzęcie |
| Zachowanie bazy i linkowania | 7 hashy bazy v0.1.9 zgodnych, konfiguracja i dotychczasowe moduły transportu zachowane; funkcje nowego modelu w ELF, hooki LED/abort/TWDT w IRAM |
| UART/LCD/HC-05 na płytce | **Nie wykonano uploadu ani prób fizycznych**. Nie sprawdzono jeszcze obciążenia kolejki na ESP32, snapshotów z rzeczywistym rendererem, fontów i obrazu TFT |

[Dokładny zakres i próba sprzętowa](TERMINAL_EMULATION.md). Artefakty: `.pio/audit/terminal_v0110.json`, `.pio/audit/terminal_v0110_cppcheck.txt`, `.pio/host-tests/result.txt`. Model nie jest pełnym xterm; odpowiedzi DSR/DA i scrollback pozostają poza tym patchem. Bajty do mostu nie są przepisywane przez parser. W razie utraty danych model sygnalizuje przerwę i może wymagać odrysowania przez nadawcę.

## Co zbierać po przyszłych testach

- Wersja `src/main.c`, użyte ustawienia `sdkconfig`, data testu.
- Serial Monitor: od bootowania do wyniku; przy błędzie pełny pierwszy stack trace.
- W razie braku wykrywania BT: czy Android skanował Classic, czy adres BT był zgodny z logiem, czy drugi odbiornik widział urządzenie.
- Po podłączeniu LCD: zdjęcie testowego obrazu, napięcia zasilania i logiczne, mapowanie pinów, ewentualnie przebiegi WR/CS.
- Rejestrować wyniki w `docs/STATUS.md`.

## Uwagi bezpieczeństwa

- Nie pomierz rezystancji przy zasilonym shieldzie.
- Nie wolno podłączać wyjść shielda 5 V do wejść ESP32; HC245 nie jest automatycznie gwarancją konwersji poziomów do 3,3 V.
- Nie lutować toru RF przed weryfikacją padów, ścieżek i referencji.
