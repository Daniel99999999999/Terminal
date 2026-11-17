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

## Wyniki po publikacji źródeł

Źródła `9ef8c9c` opublikowane po kompilacjach internal/touch SUCCESS (5,007/49,488 s w końcowym przebiegu) i przed uruchomieniem testów. Efektywne konfiguracje: touch STATE=y/ACTIVE_HIGH=y, internal STATE wyłączone. Touch BIN209568 B, ELF SHA256 `22ef38b44563f3852ac1140d60b88a6afa2fcd9efc009172568894385fee6da8`. Artefakty BIN/ELF/map zachowane lokalnie; [manifest i wyniki](logs/2026-11-17_v0.3.12_validation.json).

Po publikacji: [toolbar](logs/2026-11-17_v0.3.12_toolbar.txt), [kalibracja/UI](logs/2026-11-17_v0.3.12_calibration.txt), [renderer](logs/2026-11-17_v0.3.12_screen.txt) i [historia](logs/2026-11-17_v0.3.12_history.txt) PASS. Renderer674 porównania obrazu/32 zdarzenia, w tym kropka podczas zmiany połączenia bez danych UART. Test grafiki sprawdza także orientację powiększonego glifu, barwy trzech stanów, pełne granice hitboxów, postęp CAL i strażników pamięci. [Podgląd rzeczywistego modułu C](logs/2026-11-17_v0.3.12_toolbar_preview.png) obejrzany: kolejno LIVE/nieznany, PAUSE/rozłączony, PAUSE/połączony. Nowej wersji nie wgrano; fizyczny obraz i przejścia STATE wymagają odbioru po uploadzie.

## Próba fizyczna — 17.11.2026, wynik późniejszy

Na osobne zlecenie użytkownika **wgrano v0.3.12 `esp32dev_touch` przez COM5**. [Upload](logs/2026-11-17_v0.3.12_upload.txt) SUCCESS9,351 s, hash zapisu zweryfikowany. [Log](logs/2026-11-17_v0.3.12_panel_console.txt) potwierdza0.3.12 / ELF22ef38b44…,40×27, native BT/Wi-Fi OFF oraz mapę kalibracji odtworzoną z NVS. Jeden zamierzony reset po uploadzie pozwolił przechwycić start od początku.

Użytkownik potwierdził poprawny pasek i niebieską kropkę, pauzę tapnięciem, przewijanie i duży WZNOW. Strumień COM14→HC-05→UART1:123 wiersze, **3821 B, CRC32ab3ebb7a**, RX=parsed, errors=drop0; cursor0,26 i historia97 przed późniejszym resetem. Po zakończeniu strumienia zamknięto COM14.

Użytkownik potwierdził zielony postęp KALIBR. po krótkim przytrzymaniu i znikanie po puszczeniu. Log zawiera też przyjęcie pełnego gestu CAL po2007 ms. Użytkownik potwierdził ręczny reset ESP32 podczas prób, a po nim mapa NVS została ponownie wczytana. Zapis zawiera dodatkowe sekwencje POWERON_RESET, więc nie jest próbą nieprzerwanej stabilności; nie utożsamiamy ich z samoistną awarią.

Próba odłączenia i ponownego zasilenia drugiego HC-05: użytkownik potwierdził **niebieska→szara→niebieska**. COM5 rejestrował oba stany `STATE GPIO35 connected=0/1`; [wynik](logs/2026-11-17_v0.3.12_panel_result.json) zachowuje serię zmian i obserwacje. Dotyk zgłaszał io0, wolny wewnętrzny heap w zarejestrowanych próbkach148744 B. To krótki odbiór funkcjonalny, bez deklaracji pełnej zgodności VT ani długotrwałej stabilności.

**Końcowy stan:** na płytce v0.3.12 touch, kalibracja zachowana, T_CS33/STATE35, kontrolka i przyciski odebrane fizycznie. Zapis zakończono poprawnie, COM5/COM14 wolne. Znany problem zakresu historii podczas długiej pauzy nadal pozostaje osobnym zadaniem. Powyższe oczekiwanie na upload opisuje stan sprzed tej próby.
