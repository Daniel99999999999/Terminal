# Uruchomienie TFT 2,4 cala — kolejny etap po Bluetooth

Decyzja użytkownika z 03.11.2026: zakończyć próby wbudowanego BT, uruchomić TFT; Bluetooth docelowo przez HC-05. Użytkownik ma multimetr. Na razie nie włączamy żadnych GPIO LCD ani ADC.

## Co wynika ze zdjęć

- [Spód shielda](photos/lcd_shield_back.jpg): złącza w układzie shielda UNO, U2/U3 w obudowach 20-pin, regulator U1, gniazdo SD, nadruk 2.4 TFT LCD SHIELD. Przewody częściowo zasłaniają ścieżki i opisy.
- [Oznaczenie bufora](photos/lcd_buffer_marking.jpg): widoczna rodzina HC245. Nie potwierdzono pomiarem napięcia, DIR/OE ani przebiegu linii przez układy.
- [Styki panelu](photos/touch_4wire_contacts.jpg): X+/X−/Y+/Y−; brak potwierdzonego przejścia do złączy shielda.
- [Taśmy](photos/lcd_flex_cables.jpg): oznaczenie FPC nie jest identyfikatorem kontrolera.

**Nie ustalono** modelu kontrolera, mapowania DB0–DB7/WR/RS/CS/RD/RST, zasilania HC245 ani możliwości odczytu przez bufory. Nie identyfikujemy tego egzemplarza jako ILI9341 na podstawie rozmiaru lub nadruku PCB. Oprogramowanie [MCUFRIEND_kbv autora biblioteki](https://github.com/prenticedavid/MCUFRIEND_kbv/blob/master/extras/mcufriend_how_to.txt) pokazuje, że podobne shieldy mają różne kontrolery; jest źródłem porównania procedur identyfikacji, bez kopiowania pinów UNO i bez wprowadzania Arduino do projektu.

## Pierwsza partia pomiarów — bez zasilania

Odłączyć wszystkie źródła zasilania shielda i ESP32, następnie przewody między nimi. Mierzyć odłączony shield, bez karty SD. Zanotować rezystancję zwartych sond, aby odróżniać ścieżkę od większego oporu. Zapisywać liczby w omach, nie tylko sygnał brzęczyka.

Najpierw ustalić pin 1 U2 i U3 według znacznika obudowy. W referencyjnej karcie **SN74HC245 w obudowie 20-pin z dwoma rzędami wyprowadzeń**: VCC=20, GND=10, DIR=1, /OE=19. Na zdjęciu widoczne jest oznaczenie HC245; producent egzemplarza nie jest potwierdzony. Numerację i zgodność wyprowadzeń ustalić według oznaczenia i znacznika obudowy, nie według obrotu zdjęcia. [Pinout TI, strona 3](https://www.ti.com/lit/ds/symlink/sn74hc245.pdf#page=3) jest źródłem odniesienia.

Pierwsze cztery wyniki:

| Punkt początkowy | Punkt końcowy na złączu shielda | Wynik |
|---|---|---|
| U2 VCC / pin 20 | 3V3 | do pomiaru Ω |
| U2 VCC / pin 20 | 5V | do pomiaru Ω |
| U3 VCC / pin 20 | 3V3 | do pomiaru Ω |
| U3 VCC / pin 20 | 5V | do pomiaru Ω |

Odczyt zmienny wskutek ładowania kondensatora zapisać jako zmienny; nie uznawać chwilowego brzęczenia za zwarcie ścieżki. Ten pomiar ustala połączenie szyn, nie rzeczywiste napięcie pracy. Jeśli znacznik pinu 1 lub oznaczenie układu jest nieczytelne, najpierw je ustalić zamiast zgadywać pin 20.

Po tych wynikach:

1. Sprawdzić GND U2/U3 → GND złącza oraz powiązanie szyn 5V/3V3 z wejściem/wyjściem regulatora U1, po identyfikacji U1.
2. Prześledzić DIR i /OE obu buforów: stała szyna czy sterowanie złącza. Ustalić, która strona A/B prowadzi do złącza, a która do LCD. Sam HC245 jest dwukierunkowy, lecz jego połączenie na PCB może umożliwiać tylko zapis.
3. Zmapować oznaczone na shieldzie DB0–DB7 i pięć sygnałów sterujących przez U2/U3 do LCD. Nie utożsamiać numeru pinu UNO z bitem DBn.
4. Zmierzyć rezystancję obu osi panelu oraz ciągłość każdego styku X+/X−/Y+/Y− do złączy i LCD. ADC musi mieć dostęp do sieci analogowej panelu; aktywny bufor cyfrowy nie przeniesie wartości analogowej.

Wyniki wpisujemy do [TFT_MEASUREMENTS.csv](TFT_MEASUREMENTS.csv), potwierdzone połączenia do [PINOUT_TODO.csv](PINOUT_TODO.csv). Ten etap nie wymaga uruchamiania ESP32 ani kolejnych uploadów.

## Pomiary pod napięciem i odczyt ID

Dopiero po ustaleniu szyn/regulatora zasilić sam shield z właściwego wejścia, bez sygnałów z ESP32. Zmierzyć VCC U2/U3, napięcie logiki LCD oraz poziomy i podciągnięcia na złączu. Nie podłączać równocześnie 5V i 3V3 bez ustalenia ich roli.

HC245 zasilany 5 V nie jest automatycznym konwerterem zgodnym z GPIO ESP32. Trzeba sprawdzić progi wejściowe i poziom wyjść konkretnego układu. Przy odczycie żadne wyjście nie może podać 5 V na ESP32. Dopiero po określeniu napięć, DIR i /OE dobrać konwersję poziomów lub bezpośrednie połączenie 3,3 V.

Odczyt ID wymaga przełączenia DB0–DB7 w wejścia, poprawnego kierunku buforów oraz wykluczenia równoczesnego sterowania linią przez ESP32 i LCD. Powtarzające się 0x0000/0xFFFF lub echo komendy nie identyfikują kontrolera. Najpierw uzyskujemy powtarzalny surowy odczyt i dobieramy sterownik do wyniku; przy torze wyłącznie do zapisu potrzebna jest inna potwierdzona identyfikacja LCD.

## ESP-IDF 6.0.1 i następny patch

Sprawdzono lokalny SDK: klasyczny ESP32 ma `SOC_I2S_I80_LCD_SUPPORTED=1`, a komponent `esp_lcd` wybiera `i80/esp_lcd_panel_io_i2s.c`. Publiczny interfejs pozostaje I80. Nie kopiować bez sprawdzenia szczegółów DMA z ESP32-S3/P4. Backend wymaga INTERNAL DMA RAM i odrzuca bufor rysowania z PSRAM. To potwierdzenie kodu SDK, nie test naszego LCD.

Ten backend nie implementuje `rx_param`; ogólny `esp_lcd_panel_io_rx_param()` zwróci dla niego `ESP_ERR_NOT_SUPPORTED`. Odczytu ID nie można zatem obiecać samym wywołaniem tego API. Po sprawdzeniu elektrycznej ścieżki odczytu potrzebna będzie osobna, kontrolowana obsługa GPIO przed uruchomieniem magistrali DMA. Źródła lokalnego SDK: `components/esp_lcd/i80/esp_lcd_panel_io_i2s.c` i `components/esp_lcd/src/esp_lcd_panel_io.c`.

Po zamknięciu pomiarów: moduł magistrali `lcd_bus.c/.h`, następnie `lcd_panel.c/.h` dla zidentyfikowanego kontrolera. DMA przesyła porcje z buforów INTERNAL; opcjonalny framebuffer i historia terminala mogą być w PSRAM. Kolejka transferów z callbackiem zakończenia, bez ciągłego pełnoekranowego odświeżania i blokowania UART. Dotyk przejmuje wspólne linie dopiero po zakończeniu transferu LCD i odłączeniu jego wyjść/buforów.

Kryterium pierwszego testu obrazu: powtarzalny reset/inicjalizacja, jednolite kolory i wzór, poprawny wymiar/obrót, brak resetów ESP32. Karta SD i UI dotyku są kolejnymi etapami. Przed przypisaniem GPIO trzeba uwzględnić [UART dla HC-05](HC05_INTEGRATION.md); stara tabela samego LCD nie jest gotowym schematem całego terminala.
