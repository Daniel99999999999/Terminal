# Plan kolejnych etapów i kryteria odbioru

**Aktualne polecenie 15.11.2026 / v0.2:** badane urządzenie wysyła logi przez HC-05 master do slave przy UART1 TX32/RX34 ESP32; to UART1 zasila TFT. UART2 i most wyłączone, telefonu nie ma. [Nowy runtime](REWRITE_v0.2.0.md), faktyczne wyniki w STATUS. Starsze etapy poniżej opisują poprzedni projekt i nie zmieniają tego toru danych.

## Aktualizacja 15.11.2026 — alternatywy TFT i przyszłe SD

- Wybrać jeden [profil i pinout TFT](TFT_PROFILES_v0.1.12.md). Nowy ILI9341 SPI + XPT2046 ma potwierdzone przez użytkownika VCC/LED 3,3 V. Zweryfikować fizycznie wzór kolorów/tekst/ANSI oraz reakcję dotyku RAW, następnie dodać kalibrację i UI. Dotychczasowe odłożenie panelu rezystancyjnego dotyczy tylko shielda I80.
- Stary I80: brak podświetlenia nawet przy samym 5V/GND; identyfikacja i pomiary toru LED nadal otwarte. Dodanie profilu SPI nie potwierdza sprawności ani uszkodzenia starego LCD.
- **Przyszła funkcja zlecona do zapamiętania: zapis na karcie SD.** Zakres, SDSPI/FATFS, kolejki, wymagania i kryteria testów w [FUTURE_SD](FUTURE_SD.md). Nie ma jeszcze kodu zapisu ani przydziału CS.
- Kod kompilować przed commitem; nowszą wersję opublikować przed testami zgodnie z poleceniem użytkownika. Kontrole po publikacji zapisać oddzielnie. Bez automatycznego uploadu.

Poniższy plan zachowuje historię poprzednich etapów i shielda I80.

## Etap A — wbudowany Bluetooth: diagnoza zamknięta decyzją użytkownika

Oryginalny akceptor Espressif i ostatni wariant pełnej kalibracji RF pozostają niewidoczne na Samsungu. Windows na ostatnim wariancie zestawia ACL, ale po 30,13 s następuje LMP timeout 0x22, bez potwierdzonego AUTH/SPP. Nie ustalono przyczyny. Szczegóły i logi w [STATUS](STATUS.md) oraz [porównaniu kodu](BT_CODE_COMPARISON_2026-11-03.md).

**Nie wykonywać kolejnych prób wewnętrznego BT bez nowego polecenia użytkownika.** Wybrano HC-05; aktualny priorytet to TFT. Ostatni ref-fullcal-6.0.1 pozostaje na płytce do następnego uploadu.

## Etap B — identyfikacja shielda LCD (bieżący priorytet)

**Decyzja 13.11.2026:** TFT na razie bez dotyku. Obowiązuje [wspólny przydział HC-05 + LCD](PIN_ASSIGNMENT_HC05_TFT.md), nadal wymagający pomiarów PCB i napięć. Dawne rezerwy ADC oraz tabela samego LCD są historyczne.

- Użytkownik ma multimetr. Wykonać pierwsze cztery pomiary VCC U2/U3 → 3V3/5V na odłączonym shieldzie, zgodnie z [procedurą](TFT_BRINGUP.md). Wyniki w [arkuszu](TFT_MEASUREMENTS.csv).
- Zweryfikować stany startowe GPIO0/5/15 nowego planu, w szczególności CS nieaktywne podczas BOOT/uploadu przy RD0=LOW. Nie łączyć niezbadanych poziomów shielda z ESP32.
- Nie uruchamiać dotyku ani nie dokładać przewodów osi. Jeśli osie są fabrycznie wspólne z LCD, uwzględnić to przy mapowaniu i przyszłym powrocie do dotyku.
- Odczytać dostępne oznaczenia układu LCD lub bezpiecznie odczytać ID po określeniu torów 8080 i poziomów napięć.
- Rozrysować D0–D7, WR, RD, CS, DC/RS, RST, podświetlenie, VCC/GND oraz sterowanie HC245; dopiero potem zatwierdzić fizyczne połączenia według planu GPIO ESP32-WROVER.
- **Kryterium odbioru:** potwierdzony kontroler, tabela połączeń poparta pomiarem, brak 5 V na GPIO ESP32.

## Etap C — test grafiki I80 (w ESP-IDF)

- Wydzielić `lcd_bus.c/.h` i `lcd_panel.c/.h`; dobrać sterownik **po** identyfikacji LCD.
- Przetestować reset, inicjalizację, wypełnienia kolorami, test wzoru RGB565 i obrót; transfer przez I80 + DMA, bez pełnoekranowych blokad.
- Bufor DMA w INTERNAL DMA RAM, opcjonalnie framebuffer w PSRAM, odświeżanie obszarów zmienionych.
- **Kryterium odbioru:** powtarzalne testy kolorów i poprawny obraz bez zawieszania BT/UART.

## Etap D — dotyk 4-wire

Odłożony decyzją z 13.11.2026. Nowy wariant nie rezerwuje GPIO32/33 dla panelu; wznowienie wymaga ponownego bilansu, ewentualnie zewnętrznego kontrolera.

- Pomierzyć X+/X−, Y+/Y−, zmapować do shielda i sprawdzić potencjalny współdział z magistralą LCD.
- Zaimplementować stan wyjść i odczyt ADC z filtrowaniem, kalibrację i wykrywanie kliknięcia / gestów.
- **Kryterium odbioru:** poprawne i powtarzalne współrzędne w testowym UI.

## Etap E — HC-05 na osobnym UART1

- Zidentyfikować moduł i płytkę nośną, poziomy UART/zasilanie, nastawy baud oraz ewentualny STATE.
- Pierwszy etap: RXD→GPIO32, TXD→GPIO34, GND i zasilanie dobrane do nośnika. Plan obejmuje późniejszy STATE35 i warunkowy KEY12, przy zachowaniu UART2 TX25/RX26, konsoli USB i debug LED na GPIO2; GPIO16/17 zajęte PSRAM.
- Użytkownik potwierdził ZS-040 i AT przez EN. Przed EN/KEY12 zabezpieczyć fabryczny strap VDD_SDIO; określić obciążenie, poziomy oraz sekwencję sterowania z ESP32 i potrzebę restartu/zasilania do pełnego AT. STATE i dodatkową LED sprawdzić oddzielnie; nie obciążać wyjścia STATE diodą bezpośrednio.
- Wydzielić transport nieblokujący UART1 i sprawdzić telefon ↔ UART2 oraz sygnalizację LED. [Szczegóły](HC05_INTEGRATION.md).
- **Kryterium odbioru:** sparowanie HC-05 i stabilny przesył w obu kierunkach przy równoległej pracy TFT.

## Etap F — funkcje terminala

- W v0.1.10 dodano `terminal.c/.h`, `ansi_parser.c/.h` i adapter kolejki `terminal_view.c/.h`: domyślnie biały/czarny, automatyczne sterowanie strumieniem, podzbiór ANSI/VT100 i kolorów xterm. [Dokładny zakres](TERMINAL_EMULATION.md). Model jest testowalny bez LCD; fizyczne wyświetlanie nadal wymaga etapu B/C.
- Dodać renderer TFT respektujący kolory/atrybuty/kursor i zmienione komórki, następnie scrollback. Siatkę/font/viewport dobrać do ustalonego kontrolera i wymiarów.
- Tryb telefon ↔ UART oraz lokalny ekran, historia, status linii i nastawy 9600–115200, CR/LF.
- Testy funkcjonalne: ANSI CSI, CR/LF/BS, zawijanie, scrollback, długie bloki danych, brak gubienia bajtów (o ile przepływ/limity poprawnie ustawione).
- **Kryterium odbioru:** dwukierunkowa stabilna komunikacja z urządzeniem docelowym i responsywny UI.

**Zasada:** nie realizować całego planu w jednym patchu. Każdy etap powinien zakończyć się logiem, kryteriami i aktualizacją `docs/STATUS.md`.
