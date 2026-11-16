# v0.3.8: pasy w pokazie VT100 i zegar SPI

Zdjęcie użytkownika z16.11.2026 pokazuje kilka trwałych, wąskich pasów i fragmenty tekstu zamiast paneli/menu/okna. Lokalny pasek dotyku pozostaje widoczny. To błąd fizycznego obrazu, nie pomylenie paneli tła z pozostałościami okna. Surowe zdjęcie użytkownika nie jest publikowane. Poprzednie [wyniki pokazu](VT100_DEMO_2026-11-16.md): model C i logiczny scheduler zgodne, HC-05 bez strat, bez błędu API TFT; nie dowodzi to poprawności transmisji elektrycznej.

W konfiguracji wykryto `RETRO_TFT_SPI_HZ=20000000`. [ILI9341 DS V1.05, §18.3.4, s.237](https://files.waveshare.com/upload/e/e3/ILI9341_DS.pdf#page=237) podaje okres zapisu co najmniej100 ns oraz czas HIGH/LOW co najmniej40 ns. Zegar20 MHz daje50 ns i nominalnie25 ns na połówkę, poza tą specyfikacją. Nie zidentyfikowano dokładnego producenta klona ani nie zmierzono przebiegów oscyloskopem; przekroczenie nominalnej specyfikacji jest faktem konfiguracji, natomiast związek z konkretnymi pasami pozostaje hipotezą do sprawdzenia.

Jedyna zmiana funkcjonalna v0.3.8 to domyślne10 MHz (okres100 ns, nominalnie50 ns HIGH/LOW), wybrane przez użytkownika zamiast proponowanych5 MHz; dodano log żądanej częstotliwości. Zachowano piny, tryb SPI0, rozmiar paska320×8, czas po resecie LCD, kolejność DMA, prędkość HC-05, dotyk i kalibrację NVS. Nie zmieniano SDK ani upstream ILI9341. Pamięć pozostaje internal, GPIO16/17 zarezerwowane; nie uznajemy PSRAM za naprawione.

Plan: build internal/touch → commit/push źródeł → istniejące kontrole LCD i profilu → upload wyłącznie aplikacji touch przez COM5 z zachowaniem NVS i zapis startu → ten sam pokaz 5×7 s przez COM14. Kryterium fizyczne: pełny spójny obraz każdego ekranu i poprawny stan końcowy bez pasów. Bajty/CRC są oddzielnym kryterium transportu. Gdy10 MHz nie usuwa objawu, nie przypisywać mu przyczyny bez kolejnego dowodu. Zachowany plik v0.3.7 touch umożliwia powrót.

## Wynik wykonania

Po publikacji `081ca4c` przeszły [regresje LCD](logs/2026-11-16_v0.3.8_host_lcd.txt) i [kontrole profili](logs/2026-11-16_v0.3.8_host_profiles.txt). Firmware touch204608 B zapisano do0x10000, hash zweryfikowany, NVS zachowane. [Log startu](logs/2026-11-16_v0.3.8_touch_boot.txt) potwierdza wersję0.3.8 / ELF f728ca03d…, żądane10 MHz, guard150 ms, kalibrację NVS, powitanie3009 ms i brak powtarzanych restartów w obserwacji15 s.

Próby wizualne zachować oddzielnie:

1. Po uploadzie: [transport PASS](logs/2026-11-16_v0.3.8_vt100_demo_result.json), użytkownik zgłosił wyłącznie biały ekran od wgrywania.
2. Ręczny EN/RESET: zapis COM5 uruchomiono przed naciśnięciem. [Nowy start](logs/2026-11-16_v0.3.8_manual_reset_boot.txt) poprawny w logu; [powtórzony pokaz](logs/2026-11-16_v0.3.8_manual_vt100_demo_result.json) transport PASS, ale użytkownik zgłosił pasy. Zdjęcia pokazują pozostałości powitania pośrodku oraz panele/menu widoczne tylko w pasach górnej części obrazu.
3. Kolejne odtworzenie na żądanie, odliczanie5 s, ten sam firmware bez uploadu i bez resetu wykonywanego przez skrypt: [transport PASS](logs/2026-11-16_v0.3.8_repeat_vt100_demo_result.json). Użytkownik zgłosił poprawny obraz („Teraz działa”), następnie doprecyzował, że wcześniej ekran był funkcjonalny tylko do połowy. Aktualne zdjęcia potwierdzają prawidłowy układ czterech ekranów opisanych poniżej.

Każda sesja przesłała18037 B /1438 komend, CRC32 c7565c7b po obu stronach, zgodny kursor, bez strat/błędów UART/paniki w obrębie zapisu. Sesji nie wolno traktować jako jednego ciągłego uruchomienia: po zakończeniu drugiej RX=18037 przy uptime50,310 s; trzecia zaczyna się od RX=0 przy uptime65,320 s, czyli nastąpił start poza zapisem pomiędzy sesjami. Nie ustalono jego dokładnej chwili ani przyczyny. Aktualny stan obu portów: zamknięte; LCD pozostawiony bez zmian do zdjęć.

Wynik nie rozstrzyga pierwotnej przyczyny pasów ani białego obrazu. Samo obniżenie zegara nie dało od razu poprawnego obrazu w dwóch pierwszych próbach; nie deklarować, że rozwiązało wszystkie objawy. Nie modyfikowano transportu SDK; porównany fragment sterowania D/C odpowiada [oryginalnemu plikowi v6.0.1 Espressif](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_lcd/spi/esp_lcd_panel_io_spi.c). Użytkownik potwierdził fizyczne połączenie LCD SDO z GPIO19; odczyt rejestrów nie został wykonany. [Zbiorcze wyniki i hashe artefaktów](logs/2026-11-16_v0.3.8_validation.json).

## Odbiór aktualnych zdjęć

Porównano zdjęcia użytkownika z [podglądem modelu](logs/2026-11-16_v0.3.7_vt100_demo_preview.png) i pozycjami/atrybutami w `tests/serial_vt100_demo.py`. Podgląd powstał wcześniej, ale strumienie testowe pozostają identyczne w v0.3.8. Surowych zdjęć użytkownika nie kopiowano do repozytorium.

| Zdjęcie | Ekran | Wynik porównania |
| --- | --- | --- |
| 20261116_195623.jpg | 02, menu Plik | Pozycja i ramka poprawne; Kopiuj/F5 wybrane przez inverse. |
| 20261116_195629.jpg | 03, okno kopiowania | Treść i ramka poprawne; wybrany OK, Anuluj bez zaznaczenia. |
| 20261116_195637.jpg | 04, okno kopiowania | Układ zachowany; zaznaczenie przeszło na Anuluj, OK bez zaznaczenia. |
| 20261116_195646.jpg | 05, koniec pokazu | Kompletne okno z treścią końcową i wybranym OK. |

Na tych czterech zdjęciach nie widać wcześniejszych uszkodzonych pasów, pozostałości powitania, lustrzanego odbicia ani ograniczenia do połowy ekranu. Panele widoczne wokół okien oraz częściowo zasłonięte przez nie nazwy plików są celowym tłem wzorca. Tła okien zasłaniają właściwy obszar, a dolny lokalny pasek PAUZA/CR=NL/CAL pozostaje odrębny i kompletny.

Tło okna we wzorcu to SGR47, RGB #aaaaaa; na zdjęciach wygląda turkusowo. Użytkownik potwierdził **szare tło na żywo**, dlatego sam wygląd fotografii nie jest podstawą do zmiany ustawień RGB/BGR ani palety. Nie wykonywano pomiaru barw ani kolejnego testu RGB.

**Zaliczono wizualnie układ i zaznaczenia czterech pokazanych ekranów oraz potwierdzono szare tło na żywo.** Pierwszy ekran01 nie występuje w tym zestawie zdjęć. To odbiór zastosowanych w pokazie CUP, ramek DEC i atrybutu SGR7, nie deklaracja pełnej zgodności VT100 ani powtarzalności wszystkich przyszłych startów. Nie zmieniano firmware ani nie otwierano portów podczas analizy. Przyczyna wcześniejszego bielenia/pasów nadal nieustalona; 10 MHz zachowane.
