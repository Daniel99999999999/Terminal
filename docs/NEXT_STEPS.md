# Plan kolejnych etapów i kryteria odbioru

## Etap A — wbudowany Bluetooth: diagnoza zamknięta decyzją użytkownika

Oryginalny akceptor Espressif i ostatni wariant pełnej kalibracji RF pozostają niewidoczne na Samsungu. Windows na ostatnim wariancie zestawia ACL, ale po 30,13 s następuje LMP timeout 0x22, bez potwierdzonego AUTH/SPP. Nie ustalono przyczyny. Szczegóły i logi w [STATUS](STATUS.md) oraz [porównaniu kodu](BT_CODE_COMPARISON_2026-11-03.md).

**Nie wykonywać kolejnych prób wewnętrznego BT bez nowego polecenia użytkownika.** Wybrano HC-05; aktualny priorytet to TFT. Ostatni ref-fullcal-6.0.1 pozostaje na płytce do następnego uploadu.

## Etap B — identyfikacja shielda LCD (bieżący priorytet)

- Użytkownik ma multimetr. Wykonać pierwsze cztery pomiary VCC U2/U3 → 3V3/5V na odłączonym shieldzie, zgodnie z [procedurą](TFT_BRINGUP.md). Wyniki w [arkuszu](TFT_MEASUREMENTS.csv).
- Przy planowaniu GPIO uwzględnić UART1 dla HC-05. Stara tabela samego LCD zużywa wszystkie dostępne wyjścia i nie obejmuje dodatkowego TX; nie stosować jej jako gotowego połączenia.
- Wstępny plan GPIO oraz rezerwy ADC1 dla dotyku są w `docs/LED_AND_PIN_PLAN.md`. Nie łączyć przykładowych LCD_D0/D1/WR/RS z osiami bez pomiaru; GPIO32/33 mają ostatecznie trafić na rzeczywiste sieci X+/Y+.
- Odczytać dostępne oznaczenia układu LCD lub bezpiecznie odczytać ID po określeniu torów 8080 i poziomów napięć.
- Rozrysować D0–D7, WR, RD, CS, DC/RS, RST, podświetlenie, VCC/GND oraz sterowanie HC245; dopiero potem ustalić GPIO ESP32-WROVER.
- **Kryterium odbioru:** potwierdzony kontroler, tabela połączeń poparta pomiarem, brak 5 V na GPIO ESP32.

## Etap C — test grafiki I80 (w ESP-IDF)

- Wydzielić `lcd_bus.c/.h` i `lcd_panel.c/.h`; dobrać sterownik **po** identyfikacji LCD.
- Przetestować reset, inicjalizację, wypełnienia kolorami, test wzoru RGB565 i obrót; transfer przez I80 + DMA, bez pełnoekranowych blokad.
- Bufor DMA w INTERNAL DMA RAM, opcjonalnie framebuffer w PSRAM, odświeżanie obszarów zmienionych.
- **Kryterium odbioru:** powtarzalne testy kolorów i poprawny obraz bez zawieszania BT/UART.

## Etap D — dotyk 4-wire

- Pomierzyć X+/X−, Y+/Y−, zmapować do shielda i sprawdzić potencjalny współdział z magistralą LCD.
- Zaimplementować stan wyjść i odczyt ADC z filtrowaniem, kalibrację i wykrywanie kliknięcia / gestów.
- **Kryterium odbioru:** poprawne i powtarzalne współrzędne w testowym UI.

## Etap E — HC-05 na osobnym UART1

- Zidentyfikować moduł i płytkę nośną, poziomy UART/zasilanie, nastawy baud oraz ewentualny STATE.
- Zatwierdzić wspólny budżet GPIO z TFT i dotykiem, zachować UART2 TX25/RX26 oraz konsolę USB; nie używać GPIO16/17.
- Wydzielić transport nieblokujący UART1 i sprawdzić telefon ↔ UART2 oraz sygnalizację LED. [Szczegóły](HC05_INTEGRATION.md).
- **Kryterium odbioru:** sparowanie HC-05 i stabilny przesył w obu kierunkach przy równoległej pracy TFT.

## Etap F — funkcje terminala

- W v0.1.10 dodano `terminal.c/.h`, `ansi_parser.c/.h` i adapter kolejki `terminal_view.c/.h`: domyślnie biały/czarny, automatyczne sterowanie strumieniem, podzbiór ANSI/VT100 i kolorów xterm. [Dokładny zakres](TERMINAL_EMULATION.md). Model jest testowalny bez LCD; fizyczne wyświetlanie nadal wymaga etapu B/C.
- Dodać renderer TFT respektujący kolory/atrybuty/kursor i zmienione komórki, następnie scrollback. Siatkę/font/viewport dobrać do ustalonego kontrolera i wymiarów.
- Tryb telefon ↔ UART oraz lokalny ekran, historia, status linii i nastawy 9600–115200, CR/LF.
- Testy funkcjonalne: ANSI CSI, CR/LF/BS, zawijanie, scrollback, długie bloki danych, brak gubienia bajtów (o ile przepływ/limity poprawnie ustawione).
- **Kryterium odbioru:** dwukierunkowa stabilna komunikacja z urządzeniem docelowym i responsywny UI.

**Zasada:** nie realizować całego planu w jednym patchu. Każdy etap powinien zakończyć się logiem, kryteriami i aktualizacją `docs/STATUS.md`.
