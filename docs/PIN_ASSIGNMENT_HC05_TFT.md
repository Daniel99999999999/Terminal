# Przydział GPIO: HC-05 + TFT 8-bit, bez dotyku — 13.11.2026

**Aktualizacja kodu v0.1.11:** [most HC-05 i renderer ILI9341 są zaimplementowane](HC05_TFT_v0.1.11.md). UART1 ma potwierdzone 115200; TFT/EN/STATE pozostają domyślnie wyłączone do pomiarów. Poniższy przydział GPIO jest zachowany, uwagi o dopiero planowanym kodzie odnoszą się do wcześniejszego v0.1.10. Wyniki końcowych kontroli są w STATUS.

Decyzja użytkownika: najpierw HC-05 z RXD/TXD/GND/zasilaniem, później EN do konfiguracji AT i STATE do wskazania połączenia. TFT na tym etapie bez obsługi dotyku, z ośmioma liniami danych i wszystkimi pięcioma liniami sterującymi. Konsola USB, UART2 i debug LED pozostają.

**Potwierdzenie użytkownika:** nośnik **ZS-040**, wejście w tryb AT przez **EN sprawdzone na jego egzemplarzu**. Nie jest to jeszcze test sterowania EN z GPIO12 ESP32 ani pomiar STATE.

To **plan numerów GPIO po stronie ESP32**, zastępujący wcześniejszą tabelę samego LCD i rezerwacje ADC dla dotyku. Nie jest wynikiem pomiaru złączy shielda, identyfikacji kontrolera ani napięć. Nie aktywowano tych GPIO w firmware v0.1.10. Przed połączeniem LCD obowiązuje [weryfikacja PCB/HC245 i poziomów](TFT_BRINGUP.md).

## HC-05 na UART1

| Wyprowadzenie HC-05 | Połączenie po stronie ESP32 | Etap / uwagi |
|---|---|---|
| RXD | **GPIO32 — UART1 TX** | Początkowe połączenie; ESP32 nadaje do HC-05 |
| TXD | **GPIO34 — UART1 RX** | Początkowe połączenie; ESP32 odbiera z HC-05 |
| GND | **GND** | Wspólna masa ESP32, HC-05, LCD i urządzenia UART2 |
| VCC | **5 V — wejście VCC nośnika ZS-040** | Przez jego regulator; nie z GPIO i nie na pad 3V3 małego modułu |
| STATE | **GPIO35** | Rezerwacja na później; najpierw pomiar poziomu i zachowania |
| EN / KEY | **GPIO12, warunkowo** | Rezerwacja na później; na razie pozostawić niepodłączone |

UART1 zostanie skierowany przez GPIO Matrix; jego domyślne piny nie obowiązują. GPIO34/35 są wyłącznie wejściami i nie mają wewnętrznych rezystorów pull-up/pull-down. Ewentualne rezystory zewnętrzne dobiera się do rzeczywistego interfejsu modułu. [Dokumentacja GPIO ESP-IDF 6.0.1](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-reference/peripherals/gpio.html).

### EN / KEY i tryb AT

**GPIO12 nie jest jeszcze zatwierdzone do fizycznego połączenia.** Jest pinem strapującym MTDI, związanym z wyborem napięcia VDD_SDIO pamięci przy resecie. Trzeba zachować fabryczny stan startowy konkretnego modułu WROVER. Nie dodawać w ciemno pull-up, pull-down ani połączenia z przyciskiem lub EN nośnika HC-05. Rozwiązaniem może być bufor o wysokiej impedancji wejściowej, który nie pozwoli stronie HC-05 wymuszać poziomu GPIO12 podczas resetu, po ustaleniu obu układów. Nie zmieniamy eFuse.

Dla tego egzemplarza **funkcja EN do wejścia AT jest potwierdzona przez użytkownika**. Pozostają do ustalenia poziomy i obciążenie połączenia z ESP32 oraz dokładna sekwencja sterowania. W trybach AT wybieranych stanem KEY przy uruchamianiu samo przełączenie GPIO podczas pracy nie wystarczy; potrzebny będzie również restart lub cykl zasilania HC-05. Ten plan nie rezerwuje osobnego wyjścia do automatycznego odcinania jego zasilania. Baud konfiguracji i transmisji ustalamy na rzeczywistym module; nie przenosimy ustawień innych rewizji ZS-040.

Jeżeli nie można bezpiecznie wykorzystać GPIO12, dalsze sterowanie EN wymaga zmiany sprzętowej/przydziału, np. ekspandera dla wolnych sygnałów sterujących LCD i KEY. Nie ma tu innego wolnego wyjścia zastępczego.

## TFT: równoległe 8 bitów

Nazwy w lewej kolumnie oznaczają sygnały podane przez użytkownika, **nie numery pinów Arduino UNO**. Ich rzeczywisty przebieg przez shield i bufory pozostaje do potwierdzenia.

| Sygnał TFT | GPIO ESP32 | Kierunek z punktu widzenia ESP32 |
|---|---|---|
| LCD_D0 | **13** | OUT przy zapisie / IN przy bezpiecznym odczycie |
| LCD_D1 | **14** | OUT / IN |
| LCD_D2 | **18** | OUT / IN |
| LCD_D3 | **19** | OUT / IN |
| LCD_D4 | **21** | OUT / IN |
| LCD_D5 | **22** | OUT / IN |
| LCD_D6 | **23** | OUT / IN |
| LCD_D7 | **27** | OUT / IN |
| LCD_WR | **33** | OUT; zapis aktywny LOW |
| LCD_RS / DC | **15** | OUT; LOW komenda / HIGH dane; pin strapujący |
| LCD_CS | **5** | OUT; aktywny LOW; pin strapujący |
| LCD_RD | **0** | OUT; odczyt aktywny LOW; pin strapujący / BOOT |
| LCD_RST | **4** | OUT; reset aktywny LOW |
| GND | **GND** | Wspólna masa |
| Zasilanie | Właściwe wejście shielda po pomiarach | Nie z GPIO; nie łączyć jednocześnie 5V i 3V3 bez ustalenia sieci |

LCD_RST jest na GPIO4, a nie na pinie strapującym, aby ewentualny obwód RC resetu shielda nie obciążał strapa. SD, sterowanie jasnością podświetlenia i dotyk nie mają przydziału. Nie podłączać osobnych X+/X−/Y+/Y−; jeśli panel jest fabrycznie połączony z magistralą LCD, to połączenie nadal istnieje, choć nie wykonujemy pomiarów dotyku. Zostawić SD bez karty i upewnić się, że jego nieużywane CS jest nieaktywne.

### Poziomy przy resecie i BOOT

GPIO0 musi być HIGH podczas normalnego startu; GPIO5 i GPIO15 powinny zachować HIGH. Po potwierdzeniu poziomów shielda przewidzieć podciągnięcie **RD, CS i RS do 3,3 V po stronie ESP32**, typowo 10 kΩ z uwzględnieniem rezystorów już na obu PCB. WR także powinno pozostać nieaktywne HIGH, gdy ESP32 jest w resecie. Podciągnięcie do 5 V jest niedopuszczalne. Oprogramowanie nie zastąpi poprawnych stanów przed swoim uruchomieniem.

BOOT i układ automatycznego programowania ściągają GPIO0 do LOW, co w tym planie aktywuje RD. Dlatego **CS musi w tym czasie pozostawać HIGH i rzeczywiście odłączać wyjścia LCD/buforów**. Jeżeli shield tego nie zapewnia, połączenie RD→GPIO0 wymaga zmiany lub separacji przed podłączeniem. Sprawdzić zarówno zwykły start, jak i upload USB z zasilonym shieldem. Zasady strapów: [Espressif, wybór trybu startu](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html), [konfiguracja WROVER-E, rozdział 4](https://documentation.espressif.com/esp32-wrover-e_esp32-wrover-ie_datasheet_en.pdf#page=14). Nie oznacza to identyfikacji dokładnego wariantu modułu użytkownika.

Przy odczycie ID dane muszą przechodzić przez poprawnie przełączone HC245, bez zwarcia dwóch nadajników i bez 5 V na ESP32. Przydział RD nie jest dowodem, że shield obsługuje odczyt. Dotychczasowy warunek identyfikacji kontrolera i pomiarów napięć pozostaje w mocy.

## Pozostałe interfejsy i bilans

| Rola | GPIO / stan |
|---|---|
| Konsola USB / UART0 | **TX1 / RX3**, bez zmian |
| UART2 urządzenia docelowego | **TX25 / RX26**, bez zmian |
| Debug LED na PCB | **GPIO2**, active HIGH według schematu odniesienia; osobna od wskaźnika połączenia |
| PSRAM | **GPIO16/17 — zajęte, nie podłączać** |
| Flash | **GPIO6–11 — zajęte, nie podłączać** |
| Wolne wejścia na typowej płytce | **GPIO36 / VP i GPIO39 / VN**, jeśli wyprowadzone; nie mogą sterować LCD ani LED |

Po odjęciu konsoli 1/3, UART2 25/26 i debug LED na GPIO2 pozostaje **15 wyjść**: 0/4/5/12/13/14/15/18/19/21/22/23/27/32/33. LCD zajmuje 13, UART1 TX jedno, przyszłe EN/KEY ostatnie (warunkowe GPIO12). Oznacza to brak zapasu wyjść. RX UART1 i STATE korzystają z wejść 34/35. Poprzednia rezerwacja GPIO32/33 na ADC dotyku w tym wariancie nie obowiązuje; późniejszy dotyk wymaga ponownej analizy lub zewnętrznego kontrolera.

## Ikona BT i dodatkowa niebieska LED

STATE na GPIO35 pozwoli aplikacji aktualizować ikonę połączenia po ustaleniu polaryzacji i filtracji sygnału. Brak ruchu UART nie oznacza rozłączenia. Dodatkową niebieską LED można sterować z tego samego STATE **przez bufor lub tranzystor i rezystor LED**, bez kolejnego GPIO i bez obciążania wyjścia modułu prądem diody. Wskazanie będzie wtedy zależało bezpośrednio od STATE, a nie od dowolnego polecenia ESP32.

GPIO2 pozostaje debug LED. W v0.1.11 most UART1 jest zaimplementowany, a po włączeniu zweryfikowanego STATE rytm jest oparty na jego sygnale. Bez STATE stan jest nieznany i LED miga szybko. Wewnętrzny SPP usunięto z bieżącego firmware.

## Zasilanie i pierwszy etap

1. Dla wskazanego przez użytkownika **ZS-040: VCC → 5 V**, do wejścia płytki nośnej z regulatorem, ze źródła o odpowiedniej wydajności. Nie mylić go z padem zasilania małego modułu. Typowy ZS-040 przyjmuje 3,6–6 V na VCC, ale jego UART nadal ma logikę 3,3 V. [Opis i oględziny ZS-040 autora pomiarów](https://www.martyncurrey.com/hc-05-and-hc-06-zs-040-bluetooth-modules-first-look/). Sprawdzić zgodność oznaczeń własnego nośnika; nie podawać 5 V na RXD/EN/STATE.
2. Potwierdzić poziomy UART zgodne z 3,3 V. Połączyć RXD→GPIO32, TXD→GPIO34, GND i właściwe VCC. EN/KEY i STATE na razie nie podłączać.
3. Dla TFT najpierw pomiary zasilania, HC245, stanów startowych i złączy według [TFT_BRINGUP](TFT_BRINGUP.md); dopiero potem połączenia z powyższej tabeli. Kontroler nadal nie jest potwierdzony jako ILI9341.
4. Po ustaleniu baud HC-05 dodać transport UART1, bez prób wbudowanego BT. Osobno sprawdzić RX/TX, parowanie, później STATE i sterowanie AT z ESP32 oraz start/upload z podłączonym LCD.

Pierwotna tabela powstała dla v0.1.10. Implementacja jest już w v0.1.11; nie wykonano testu elektrycznego ani uploadu. Końcowe kontrole kodu są zapisywane oddzielnie w STATUS.
