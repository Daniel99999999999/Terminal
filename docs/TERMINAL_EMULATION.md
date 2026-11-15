# Automatyczna interpretacja ANSI / VT100 — v0.1.10

**Wynik na fizycznym TFT / v0.3.1:** [próby VT100/ANSI 15.11.2026](VT100_TEST_2026-11-15.md), przez HC-05 na UART1, z działającym rendererem 40×29 + stopka. Użytkownik potwierdził końcowy obraz, kolory, ramkę DEC i miganie. Dawne opisy uruchamiania LCD, UART2 i adaptera v0.1.10 poniżej są historyczne; bieżący runtime i wyniki opisują README/STATUS.

**Bieżący runtime v0.3.1:** [wybierany tryb CR jako nowej linii](DISPLAY_CONTROLS_v0.3.1.md) jest domyślnie włączony w aplikacji dla zwykłych logów/Enter z PuTTY. Standardowe CR opisane poniżej pozostaje dostępne przez CR=CR/Kconfig. Parser nadal automatycznie obsługuje sekwencje ANSI. Odbiór i CRC zachowują oryginalne bajty.

Wymaganie użytkownika z 13.11.2026: zwykły tekst ma być biały na czarnym tle, a sterowanie VT100 i popularne nowsze sekwencje ma być interpretowane automatycznie. Implementacja przetwarza jeden ciągły strumień tekstowy: zwykłe znaki trafiają do komórek, rozpoznane komendy zmieniają stan ekranu. Nie trzeba wybierać profilu przed pierwszą sekwencją. Atrybuty pozostają aktywne aż do ich zmiany/resetu przez nadawcę, również przez granice pakietów.

## Co jest zaimplementowane

| Funkcja | Zakres modelu w pamięci |
|---|---|
| Domyślny wygląd | znaki RGB 0xFFFFFF, tło 0x000000; SGR 0/39/49 przywraca odpowiednie wartości |
| Zwykły tekst | ASCII i dekodowanie UTF-8, także przy podziale znaku między porcjami UART |
| Sterowanie podstawowe | CR, LF, BS, TAB; standardowo LF zachowuje kolumnę, CR wraca do początku; LNM pozwala połączyć LF z CR |
| Pozycja kursora | CUP/HVP, ruch względny, pozycja kolumny/wiersza, zapis/odtworzenie |
| Czyszczenie i edycja | ekran/wiersz/znaki, wstawianie/usuwanie znaków i wierszy; ED 2 nie przesuwa kursora |
| Przewijanie | region przewijania, IND/NEL/RI, scroll up/down, tryb origin |
| Atrybuty SGR | bold, underline, inverse i blink zapisane w komórkach |
| Kolory | 16 kolorów, paleta 256 oraz RGB 24-bit, zapis parametrów rozdzielanych średnikami |
| Ramki DEC | wybór G0/G1 ASCII lub DEC Special Graphics i SI/SO; ramki zapisane jako znaki Unicode |
| Popularne rozszerzenia xterm | widoczność kursora, autowrap, ekran alternatywny przez `?1049h/l`, zapis/odtworzenie `?1048h/l` |
| Dane spoza zakresu | nieobsługiwane pełne CSI konsumowane; OSC/DCS/APC/PM/SOS pomijane do terminatora, bez wyświetlania zawartości |

To określony podzbiór zgodności ANSI/VT100 i rozszerzeń xterm. Nie jest pełną emulacją wszystkich modeli VT ani całego xterm. Nie ma jeszcze odpowiedzi DSR/DA, obsługi klawiatury/myszy, pełnego VT52, surowych 8-bitowych komend C1, składni kolorów z dwukropkami, szerokich znaków CJK/łączenia znaków, zmian geometrii ani historii scrollback. OSC nie zmienia schowka, palety ani tytułu.

Źródłem znaczenia sekwencji jest [dokumentacja autora xterm](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html). Nie kopiowano kodu biblioteki terminalowej ani pinów z projektów Arduino.

## Strumień i pamięć

- `src/ansi_parser.c/.h`: automat bajtowy, maksymalnie 16 parametrów i 128 bajtów CSI; wartości liczbowe ograniczone do uint16. Długie/nieobsługiwane CSI są odrzucane do końcowego bajtu, a CAN/SUB przerywają sekwencję. ESC może rozpocząć nową komendę.
- `src/terminal.c/.h`: czysty C, model komórek i dwie strony ekranu. Bez alokacji i I/O; bufory dostarcza wywołujący. Przewijanie obraca indeksy wierszy zamiast kopiować całą zawartość PSRAM.
- `src/terminal_view.c/.h`: adapter ESP-IDF. UART RX kopiuje dane do kolejki bez oczekiwania. Osobne zadanie o priorytecie 6 aktualizuje model. Dwie strony 80×24 zajmują **46 080 B PSRAM**; kolejka ma 32 porcje po maksymalnie 128 B.
- Dane UART2 są przekazywane do modelu przed warunkiem aktywnego Bluetooth, więc lokalny model przyjmuje tekst również offline. Bajty przekazywane dalej przez istniejący most pozostają nieprzetworzone; parser pracuje na kopii. To odbiór UART2, bez lokalnego echa danych TX.
- Zadanie modelu zgłasza heartbeat również przy pustej kolejce; jego brak przez 2 s trafia do istniejącego nadzoru debug LED. Jest to kontrola żywotności zadania, nie dowód poprawnego obrazu LCD.

Utrata bajtów, błąd UART albo opróżnienie RX na dotychczasowej granicy sesji BT powodują oznaczenie przerwy strumienia. Następna przyjęta porcja anuluje częściową sekwencję ANSI/UTF-8 i przywraca domyślne atrybuty. Liczniki `dropped` i `gaps` są widoczne w logu TERMINAL. Nie można odtworzyć zgubionego pozycjonowania; nadawca może potrzebować ponownie narysować ekran. Kolejka modelu nigdy nie blokuje UART ani callbacku BT.

## Połączenie z TFT — nadal do wykonania

**Sterownik fizycznego LCD nie jest jeszcze uruchomiony.** Rozmiar 80×24 jest logiczną siatką, nie deklaracją rozmiaru fontu lub rozdzielczości tego shielda. Nadal potrzebne są pomiary PCB/HC245, identyfikacja kontrolera i dobranie fontu/viewportu. Nowe moduły nie przypisują GPIO LCD ani dotyku.

Przyszły renderer kopiuje wiersze przez `terminal_view_copy_row()`, respektuje numer rewizji i położenie/widoczność kursora. `terminal_cell_colors()` uwzględnia inverse; flagi bold/underline/blink mają zostać narysowane w warstwie TFT. Kopiowanie pod mutexem jest krótkie; wszelkie I/O LCD/DMA odbywa się po jego zwolnieniu. Bufory DMA pozostają w INTERNAL RAM. Font powinien objąć podstawowy tekst, polskie znaki i ramki DEC.

Nie wykonano uploadu ani próby na LCD. Wyniki kompilacji i testów są w [TESTS.md](TESTS.md). Baza głównego v0.1.9 z nazwami użytkownika została zachowana w [baseline/v0.1.9](../baseline/v0.1.9/README.md).

## Próba po uruchomieniu ekranu

Nadać na RX GPIO26 z urządzenia UART o poziomach 3,3 V, 115200 8N1:

1. `Zwykly tekst\r\n` — białe litery na czarnym tle od stanu po resecie.
2. Bajty ESC + `[31mBLAD` + ESC + `[0m OK\r\n` — kolor pierwszego słowa i powrót do domyślnego wyglądu.
3. ESC + `[2J` + ESC + `[H` — wyczyszczenie i osobno ustawienie kursora w lewym górnym rogu.
4. Te same dane wysłać po jednym bajcie i w różnych porcjach; obraz końcowy powinien pozostać identyczny.
5. Przetestować polskie UTF-8, ramki DEC, region przewijania i wejście/wyjście z ekranu alternatywnego. Równolegle sprawdzić UART i liczniki przepełnienia.

Zapisy `ESC` i `\r\n` w opisie oznaczają rzeczywiste bajty 0x1B, 0x0D, 0x0A, nie ich dosłowny tekst. Te próby fizyczne pozostają niewykonane.
