# Historia wersji

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
