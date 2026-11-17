# Graficzny pasek i STATE HC-05 — v0.3.12

Na polecenie użytkownika dolny pasek ma dwa graficzne przyciski i kontrolkę BT. Ma24 px wysokości: WZNOW x0–139, KALIBR. x140–279, kontrolka x280–319. Obszar trafienia jest zgodny z grafiką i obejmuje y216–239. Napisy są dwukrotnie większe od znaków terminala. Terminal z dotykiem ma teraz40×27 komórek; wariant bez dotyku zachowuje40×29 i jednoliniowy status.

Tapnięcie w tekst zatrzymuje obraz; ponowne tapnięcie pozostawia pauzę. WZNOW przywraca bieżące dane, a podczas odbioru przycisk pokazuje nieaktywny LIVE. KALIBR. wymaga przytrzymania2 s; postęp widać na przycisku. Przeciągnięcia nadal przewijają i anulują tapnięcie. Osobny przycisk CR usunięto z nowego układu; zachowano ustawienie `CONFIG_RETRO_DISPLAY_CR_AS_NEWLINE` i domyślne CR=NL, bez zmiany parsera ani bajtów UART.

`terminal_toolbar.c/.h` rysuje trzy paski RGB565 w istniejącym buforze DMA, bez pełnego framebuffera, alokacji ani I/O wewnątrz modułu. Jedno współdzielone mapowanie definiuje pola dotykowe. Nie zmieniono pinów, zegara/inicjalizacji LCD, kalibracji NVS ani IRQ XPT2046.

## Połączenia potwierdzone przez użytkownika

- T_CS dotyku → **GPIO33**. Użytkownik odkrył wcześniejsze połączenie do P35, przeniósł je na P33 i potwierdził działanie. W kodzie od początku było CS33. Możliwość działania przy stale niskim CS jest hipotezą zgodną z [notą XPT2046, str.9](https://files.waveshare.com/upload/9/98/XPT2046-EN.pdf#page=9), bez pomiaru dawnego T_CS.
- HC-05 STATE → **GPIO35**, poziom przy aktywnym połączeniu zmierzony **3,24 V**. Użytkownik potwierdził końcowo oba połączenia i działający sprzęt.
- Zgłoszony przy podłączaniu STATE biały ekran ustąpił po poprawieniu niełączących przewodów według użytkownika. Odczyt COM5 potwierdził v0.3.11, start aplikacji i jedną sekwencję restartu zasilania. Nie rozpoznano konkretnego luźnego połączenia, awarii GPIO ani nowego błędu firmware. [Log](logs/2026-11-17_v0.3.11_state_white_console.txt).

GPIO35 jest tylko wejściem i nie ma wewnętrznego pull-up/down — [Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html). Nie zamieniać go z CS33. UART1 pozostaje TX32/RX34, LCD CS4, IRQ36, PSRAM16/17 i Flash6–11 zarezerwowane.

## Kontrolka połączenia

Profil `esp32dev_touch` włącza `CONFIG_RETRO_HC05_STATE_ENABLE=y` i `CONFIG_RETRO_HC05_STATE_ACTIVE_HIGH=y` po potwierdzeniu podłączenia/napięcia. Istniejący odczyt STATE działa w zadaniu głównym co około100 ms; HIGH jest zatwierdzany po co najmniej50 ms, LOW kasuje stan połączenia przy kolejnym odczycie. Ten sam stan steruje debug LED i kontrolką TFT. Zmiana odświeża pasek także bez nowych danych UART; log co5 s podaje `STATE GPIO35 connected=0/1`.

- Pełna niebieska kropka: STATE potwierdza połączenie.
- Pełna ciemnoszara: brak połączenia.
- Szary obrys: obsługa STATE wyłączona, stan nieznany.

Jeśli STATE nie jest podłączone, wyłączyć tę opcję w konfiguracji projektu. W istniejącym `sdkconfig.terminal_touch` może być zachowana wcześniejsza wartość; domyślne ustawienia służą świeżej konfiguracji. Lokalnie zaktualizowano także wygenerowany plik przed kompilacją. Odbiór danych nie służy do zgadywania połączenia BT. Natywne radio ESP32 pozostaje wyłączone.

## Weryfikacja i ograniczenia

Kompilacja, publikacja źródeł i kontrole są zapisywane w STATUS; nowego firmware nie należy utożsamiać z programem obecnie na płytce. Panel v0.3.11 został odebrany przez użytkownika, [wyniki przepełnienia](SCROLL_TEST_2026-11-17.md). v0.3.12 zachowuje bufor128×40 komórek /60 KiB. Algorytm historii nie został zmieniony w tym etapie: podczas długiej pauzy nadpisanie całej starej historii ogranicza jej zakres do0; WZNOW i ponowne przewijanie udostępniają najnowszą historię. Odtworzony problem dostępu podczas pauzy pozostaje osobnym zadaniem.

Plan kontroli po publikacji: mapowanie pełnych przycisków, ponowne tapnięcia/gesty/CAL, trzy stany kontrolki i granice bufora, renderer przy27 wierszach oraz zmiana kontrolki bez UART, regresja historii. Próba nowego paska i odłączania/ponownego zestawiania HC-05 wymaga osobnego fizycznego odbioru po wgraniu.
