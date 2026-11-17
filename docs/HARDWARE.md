# Sprzęt, bezpieczeństwo i nieustalone połączenia

**Próba17.11.2026:** użytkownik potwierdził przewijanie w obu kierunkach i automatyczną pauzę gestem, odczyty uruchamia IRQ. Kontakty przy dolnym napisie trafiały na y222–231, poza dawnym8-pikselowym polem; v0.3.11 zmienia UI, bez przepinania przewodów ani kasowania NVS. [Raport i ograniczenia](SCROLL_TEST_2026-11-17.md). CS LCD pozostaje GPIO4; pierwotna przyczyna problemu P21 nadal nieustalona.

**Potwierdzenie17.11.2026:** użytkownik zgłosił działający LCD przy **CS→P4**. Przyjmujemy ten pinout do dalszych prób. Przyczyna problemu P21 nieustalona; nowa obsługa IRQ i przewijania gestami jeszcze bez fizycznego odbioru. [Aktualne wyniki](STATUS.md). Poniższa zapowiedź oczekiwanej próby opisuje stan sprzed potwierdzenia.

**Najnowszy przydział17.11.2026 / v0.3.10:** LCD SPI **CS→GPIO4/P4 zamiast GPIO21/P21**, na polecenie użytkownika. Zmienia się tylko przewód CS; przepiąć przy wyłączonych zasilaniach. LED LCD nadal zasilane3,3 V. GPIO4 nie jest już rezerwą EN podświetlenia; sterowanie podświetleniem przez GPIO pozostaje bez przydziału i zablokowane. [Aktualny pinout](TFT_PROFILES_v0.1.12.md), [CSV](PINOUT_SPI.csv). Inne sygnały bez zmiany; przydział I80 jest osobną alternatywą. Nowy firmware nie był wgrywany, połączenie P4 i efekt fizyczny jeszcze niepotwierdzone.

**Doprecyzowanie17.11.2026:** użytkownik mierzył5 mV bezpośrednio na goldpinie P21 ESP32. Punkt pomiaru potwierdzony; do sprawdzenia połączenie goldpinu z jego własnym polem lutowniczym i rezystancja P21↔GND przy całkowicie wyłączonym zasilaniu. Wyniki oczekiwane. Przewód LCD CS pozostaje odłączony. [Bieżący zapis](STATUS.md).

**Nowsze pomiary16.11.2026:** na płytce v0.3.9 touch, LCD ponownie biały. Po odłączeniu samego przewodu LCD CS użytkownik zgłosił GPIO21=0 V w aplikacji, następnie5 mV podczas wymuszonego HIGH w ROM; kontrolne3V3=3,257 V. Cyfrowy odczyt GPIO21 podążał LOW→HIGH. Dokładny punkt pomiaru i połączenie na PCB wymagają sprawdzenia; nie rozpoznano uszkodzenia GPIO. Próba ROM zakończona, COM5 zamknięty, CS nadal odłączony. [Dane](LCD_WHITE_v0.3.9.md), STATUS. Poniższy stan v0.3.8 jest historyczny.

**Aktualizacja16.11.2026:** LCD SDO→GPIO19 fizycznie podłączony według użytkownika; ostatnie cztery zdjęcia pokazu v0.3.8 poprawne, tło okien szare na żywo. Dotyk XPT2046 podłączony, pełna kalibracja ukończona wcześniej i odtwarzana z NVS. Przygotowana v0.3.9 wymaga IRQ na GPIO36/SVP, bez wewnętrznego pull-up; poziomy i nowa obsługa IRQ do sprawdzenia później. Płytka pozostaje v0.3.8, niczego nie przepinano ani nie wgrywano podczas tej zmiany. [Audyt i plan](LCD_INIT_AUDIT_v0.3.9.md), bieżące wyniki w STATUS. Starsze stany poniżej są historyczne.

**Aktualizacja 15.11.2026:** użytkownik potwierdził podłączenie XPT2046. Użyto planu SPI2: CLK14/DIN13/DO39/CS33/IRQ36, osobnego od SPI3 LCD. Wgrano v0.3.1 `esp32dev_touch`, inicjalizacja i wejście w kalibrację w logu poprawne. Faktyczne współrzędne i wynik pięciu punktów jeszcze weryfikujemy; [bieżące wyniki](STATUS.md). Poniższy zapis o niepodłączonym dotyku opisuje poprzedni etap.

**XPT2046 — v0.3.0:** użytkownik wybrał dotyk i kalibrację, ale dotyk nie jest jeszcze podłączony. [Aktualny plan](TOUCH_CALIBRATION_v0.3.0.md): osobne SPI2, T_CLK14/T_DIN13/T_DO39/T_CS33/T_IRQ36, sygnały 3,3 V. GPIO16/17 pozostają zarezerwowane także przy działającym wariancie bez PSRAM. Nie uznawać samej kompilacji za potwierdzenie działania dotyku.

**Potwierdzenie 15.11.2026 / v0.2.1:** użytkownik potwierdził podłączony LCD SPI, widoczne powitanie i migającą diodę GPIO2. Dotyczy to wgranego wariantu `esp32dev_internal` bez inicjalizacji PSRAM. Test COM14 potwierdził odbiór HC-05 na UART1 i przekazanie wszystkich bajtów do parsera; wizualna treść/kolory tego strumienia nie są jeszcze potwierdzone. Dotyk pozostaje wyłączony. [Bieżące wyniki](STATUS.md).

**Nowa alternatywa 15.11.2026:** użytkownik dodał moduł **ILI9341 SPI z XPT2046**, VCC i LED **3,3 V** potwierdzone przez użytkownika. [Osobny pinout SPI](TFT_PROFILES_v0.1.12.md) zastępuje połączenia I80 przy wybraniu tego profilu. Nie stosować zasilania 5 V starego shielda do nowego modułu. Dawne nieustalone HC245 i osie dotyku dotyczą shielda I80. Nowy kontroler XPT2046 ma cyfrowe SPI, nie wymaga ADC ESP32. LED zasilać z szyny 3V3 zgodnie z deklaracją napięcia modułu, nie z GPIO; zapewnić odpowiednią wydajność zasilania. Powitanie na LCD potwierdzono jak wyżej; dotyk nie był sprawdzany fizycznie.

**Aktualizacja 03.11.2026:** po ostatniej negatywnej próbie referencji z pełną kalibracją użytkownik zakończył diagnozę wbudowanego BT. Następny etap to [pomiary TFT](TFT_BRINGUP.md), Bluetooth docelowo przez [HC-05](HC05_INTEGRATION.md). Użytkownik ma multimetr; pomiary shielda i identyfikacja kontrolera nie są jeszcze wykonane. HC-05 wymaga osobnego UART i korekty budżetu wyjść względem starej tabeli samego LCD.

## Moduł bazowy

| Cechy | Stan |
|---|---|
| Płytka | NodeMCU-32 V1.3 / Ai-Thinker (oznaczenie na fotografii) |
| Moduł MCU | ESP32-WROVER / ESP32-D0WD-V3 |
| Flash | 4 MB według konfiguracji i bootloadera |
| PSRAM | układ raportuje 8 MB, 4 MB mapowane i testowane |
| Konsola USB/UART0 | 115200 bps (GPIO1, GPIO3) |
| UART2 | TX GPIO25, RX GPIO26, 115200 8N1, logika 3,3 V |
| Rezerwacja pinów | GPIO16/17 dla PSRAM, GPIO6–11 dla Flash; GPIO34–39 tylko wejścia |
| Diody | schemat NodeMCU-32 v1.3: czerwona LED2 PWR i niebieska LED1 GPIO2 active HIGH; zgodność egzemplarza niezmierzona |

**Uwaga:** GPIO0, GPIO2, GPIO5, GPIO12, GPIO15 mają funkcje strapujące boot ESP32; ich obciążenie może zmienić start układu. Nie przydzielaj ich przypadkowo. Wspólna masa z urządzeniem UART jest wymagana. Bezpośrednio do GPIO nie wolno podłączać sygnałów RS-232/5 V.

**Przydział z 13.11.2026:** [HC-05 + TFT bez dotyku](PIN_ASSIGNMENT_HC05_TFT.md). UART1 TX32/RX34, przyszły STATE35 i warunkowy EN/KEY12; LCD ma pełne 13 linii. HC-05 to **ZS-040, wejście AT przez EN sprawdzone przez użytkownika**. Wymagane sprawdzenie strapów GPIO0/5/15 i zasilania/buforów shielda. GPIO12 nie jest zatwierdzone do połączenia bez zachowania fabrycznego strapa i sprawdzenia obciążenia EN. W tym wariancie dawne rezerwacje ADC dla dotyku nie obowiązują. [Opis debug LED i historyczny plan dotyku](LED_AND_PIN_PLAN.md).

## LCD 2,4" — stan identyfikacji

Shield Arduino UNO-like z 8-bitową magistralą równoległą, dwa układy HC245 (prawdopodobnie transceivery/bufory). **Nie** utożsamiaj samego nadruku PCB z modelem kontrolera LCD.

| Sygnał / właściwość | Stan |
|---|---|
| Magistrala danych | 8-bit parallel, do potwierdzenia mapowania D0–D7 |
| Kontroler LCD | użytkownik opisuje odpowiednik ILI9341; brak odczytu ID / potwierdzenia wariantu |
| CS / RS(DC) / WR / RD / RST | Do mapowania na PCB |
| Napięcie zasilania shielda | 15.11.2026: użytkownik podaje 5 V na wejściu i poprawną pracę stabilizatora 3,3 V; napięcia HC245 i LED nadal niezmierzone |
| Kierunek HC245 / OE | Do prześledzenia |
| Podświetlenie | 15.11.2026: nie świeci także przy samym 5V/GND, bez sygnałów ESP32 i bez zasilania pinu 3V3; tor LED i ewentualne sterowanie nadal nieustalone, brak rozpoznanej przyczyny |
| Czytanie ID kontrolera | Tylko po bezpiecznym ustaleniu linii RD i poziomów |

### Dotyk

Na zdjęciach spod mikroskopu widać 4 styki: **X+, X−, Y+, Y−**. Jest to panel rezystancyjny 4-wire; **nie** widać dedykowanego XPT2046. Bez kontrolera można mierzyć osie przez GPIO/ADC, ale linie mogą być współdzielone z LCD, szczególnie przez shield Arduino.

**Nie stosuj losowego mapowania** X+/X−/Y+/Y− (np. A1, A2, D6, D7 z innych modeli); najpierw pomiar ciągłości i sprawdzenie działania buforów.

### Weryfikacja dotyku bez zasilania

1. Rozłącz całe zasilanie i inne układy.
2. Delikatnie zmierz rezystancję X+↔X− oraz Y+↔Y− na dostępnych stabilnych padach (nie na delikatnej taśmie).
3. Zmierz przejście między osiami bez nacisku i z naciskiem; opisz wyniki i jednostki.
4. Sprawdź ciągłość każdego ze styków do złącza shielda (ostrożnie i bez zwierania sąsiadów).
5. Dopiero potem przypisz GPIO ESP32.

## Grafika i DMA

RGB565 320×240 = **153 600 bajtów** na pełen ekran. Wewnętrzny RAM ma pozostać dostępny dla DMA i Bluetooth; framebuffer i bufory tekstowe można umieścić w mapowanej PSRAM. Na klasycznym ESP32 `esp_lcd` I80 wymaga bufora nadawczego spełniającego wymagania wewnętrznego DMA; nie zakładaj bezpośredniego DMA z PSRAM. Buforować porcje, ograniczać zakres aktualizacji i przetestować wymagane wyrównanie/rozmiar.

## Tor RF

W pobliżu opisów **L1/L4/C15** użytkownik zobaczył podejrzane połączenie lutownicze. Nie mylić z konfiguracją wyboru anteny R14/R15. Bez identyfikacji padów i sieci na PCB **nie wiadomo**, czy połączenie jest fabrycznym zworem szeregowym, częścią komponentu, czy zwarciem. Nie ingerować lutownicą na podstawie samej fotografii.

## Zdjęcia referencyjne

Patrz `docs/photos/` (wybrane kadry z fotografii dostarczonych przez użytkownika). Zdjęcia są orientacyjne, a nie schematem pinów.
