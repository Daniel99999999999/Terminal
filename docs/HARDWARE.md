# Sprzęt, bezpieczeństwo i nieustalone połączenia

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
| Napięcie zasilania shielda | Do pomiaru, nie zakładać 3,3 V |
| Kierunek HC245 / OE | Do prześledzenia |
| Podświetlenie | Schemat/sterowanie jeszcze nieustalone |
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
