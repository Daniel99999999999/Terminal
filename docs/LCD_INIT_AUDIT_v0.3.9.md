# Audyt startu LCD i zmiany v0.3.9 — 16.11.2026

W v0.3.8 znaleziono miejsca z niewielkim zapasem czasowym i możliwość pokazania niepełnego obrazu podczas startu. v0.3.9 zwiększa marginesy i dodaje pomiary odpowiedzi kontrolera. **Nie ustalono, że te miejsca spowodowały wszystkie wcześniejsze białe ekrany i pasy.** Nie wykonano oscyloskopowych pomiarów ani identyfikacji rzeczywistego układu. Działający ostatni pokaz v0.3.8 nie dowodzi powtarzalnego startu.

Punktem odniesienia jest źródło v0.3.8 z commita081ca4c i zaakceptowane później cztery zdjęcia. Sterownik lokalny to niezmieniony Espressif esp_lcd_ili9341 2.1.0, commit93460b932beba7022a8a4ec4186e0d6b7533d05f. ESP-IDF6.0.1, piny i obroty pozostają dotychczasowe. Dokumentacja dotyczy obecnego modułu SPI, nie starego shielda równoległego z HC245.

## Porównanie wymagań i kodu

| Etap | Specyfikacja / referencja | v0.3.8 | v0.3.9 i uzasadnienie |
|---|---|---|---|
| RESX LOW | Minimum10 µs, DS §15.4 |10 ms upstream | Bez zmiany; duży zapas, nie ma uzasadnienia dalszego wydłużania impulsu |
| RESX HIGH → SLPOUT przy resecie aktywnego panelu |120 ms, DS §15.4 |10 ms upstream +150 ms aplikacji, nominalnie160 ms | Zachowane; wcześniejszy błąd10 ms poprawiono już w v0.3.7 |
| SLPOUT → dalsza konfiguracja / pełne wybudzenie | Minimum5 ms przed kolejną komendą; pełne wyjście ze snu120 ms, DS §8.2.12 |100 ms upstream, potem MADCTL/COLMOD i tablica zasilania/gamma | Monotoniczna bariera co najmniej150 ms przed następną komendą/danymi |
| CS przed/pod koniec transakcji | Setup/hold minimum40 ns, DS §18.3.4 | Konfiguracja0/0; SDK zamienia hold0 na1 cykl, setup pozostaje bez jawnego zapasu | Po2 cykle, nominalnie200 ns przy10 MHz; nie twierdzimy, że poprzedni hold był zerowy |
| Zapis SPI | Okres minimum100 ns, high/low minimum40 ns |10 MHz po v0.3.8 |10 MHz zachowane na życzenie użytkownika; to nominalna granica częstotliwości, nie dodatkowy zapas na przewody/klon |
| Odczyt SPI | Okres minimum150 ns, DS §18.3.4 | Brak odczytów | Rejestry przy1 MHz, poniżej limitu odczytu; nie czytamy przy10 MHz |
| DISPON / pierwsza zawartość GRAM | DISPON włącza wyświetlanie zawartości GRAM, DS §8.2.19 | DISPON przed rysowaniem powitania | DISPLAY OFF podczas wszystkich30 pasków powitania, dopiero potem DISPON |
| Stabilizacja po DISPON | DS nie narzuca tutaj uniwersalnych150 ms; Adafruit czeka150 ms | Bez osobnego oczekiwania |150 ms jako ostrożny zapas dla modułu, następnie diagnostyka i3 s widocznego powitania |

**Rozróżnienie:**100 ms w sterowniku Espressif nie narusza zasady5 ms między SLPOUT a kolejnym poleceniem. Jest krótsze od120 ms pełnego wybudzenia, zatem zwiększenie do150 ms usuwa niepotrzebną zależność od szybkości pozostałej konfiguracji i gotowości klona. Nie przedstawiamy120 ms jako wymogu przed dowolną komendą.

Adafruit konfiguruje zasilanie/gamma przed SLPOUT i czeka150 ms po SLPOUT oraz DISPON. TFT_eSPI czeka120 ms po SLPOUT; posiada alternatywę ILI9341_2_DRIVER z inną konfiguracją panelu. Espressif wysyła SLPOUT, odczekuje100 ms, ustawia MADCTL/COLMOD, następnie tablicę parametrów analogowych. Te różnice potwierdzają sens porównania wariantów, ale **nie wskazują właściwego VCOM/gamma dla naszego niezidentyfikowanego panelu**. Aktualnie poprawne kolory i obraz uzasadniają zachowanie tych nastaw, zamiast losowego mieszania tablic.

## Realizacja i diagnostyka

Aplikacja stosuje publiczny interfejs esp_lcd_panel_io_t jako stabilny uchwyt pośredni. Po rzeczywistym, poprawnie wysłanym SLPOUT zapamiętuje termin150 ms w esp_timer. Kolejna komenda/dane czekają do terminu; uwzględniono zaokrąglenie ticków RTOS. Brak edycji SDK, prywatnych struktur SPI i plików vendor. Oczekiwania dotyczą startu zadania ekranu; UART ma osobny odbiór i kolejkę.

Rzeczywisty transport SPI jest opróżniany/usuwany i tworzony ponownie przy zmianie częstotliwości, z zachowaniem uchwytu widzianego przez sterownik. Nie zmieniamy zegara podczas niezakończonego DMA. Po timeout bufor pozostaje zajęty, dalsze rysowanie jest zatrzymywane. Dodatkowo propagowany jest błąd tx_color, którego zwrot pomija upstream draw_bitmap.

Logi `LCD_REG` powstają dwukrotnie: `configured/off` po inicjalizacji/orientacji i `display/on` po pełnym powitaniu/DISPON/stabilizacji. Komendy i długości obejmują bajt pusty:

| Komenda | Znaczenie ILI9341 | Odebrane bajty |
|---|---|---|
| D3 | Read ID4 |4 |
|09 | Read Display Status |5 |
|0A | Read Display Power Mode |2 |
|0B | Read Display MADCTL |2 |
|0C | Read Display Pixel Format |2 |

Log podaje etap, komendę, długość, status API i pięć surowych pól hex; pola poza długością są wyzerowane. Nie interpretuje automatycznie ID ani nie zatrzymuje startu przy błędzie odczytu. `ESP_OK` świadczy o transakcji hosta; same00/FF nie dowodzą obecności kontrolera. Dla rzeczywistego ILI9341 oczekujemy sensownej identyfikacji i spójności MADCTL/pixel format/power mode. Klon może zwracać inne ID lub mieć ograniczone odczyty. Użytkownik potwierdził fizyczne LCD SDO→GPIO19, lecz poprawność odczytu nadal wymaga logu z płytki. Nie ma okresowego odpytywania LCD podczas normalnej pracy.

Kontroler po resecie nie potwierdza gotowości przez GPIO. Większe odstępy programowe nie wykluczają zakłóceń RESET/CS/SCK, nieprawidłowego narastania zasilania lub innego kontrolera. CPU uptime nie jest pomiarem stabilności szyny LCD. Jeśli objaw pozostanie, porównamy logi udanego/nieudanego startu i przebiegi elektryczne, zamiast deklarować naprawę na podstawie READY.

## Dotyk IRQ i przewijanie

Profil `esp32dev_touch` włącza XPT2046 na dotychczasowym SPI2 i GPIO36. `RETRO_TOUCH_REQUIRE_IRQ` musi wynosić1. Przerwanie obu zboczy wybudza zadanie ekranu przez powiadomienie RTOS; ISR nie wykonuje SPI, alokacji ani renderowania. GPIO36/SVP nie ma wewnętrznego podciągania. Wymagany jest prawidłowy poziom HIGH zwolnionego PENIRQ z modułu lub jego zewnętrznego podciągania do3,3 V.

**IRQ wykrywa kontakt/puszczenie, a nie każdy ruch palca.** Dlatego po zboczu LOW zadanie pobiera kolejne współrzędne, dopóki kontakt trwa; w spoczynku nie odczytuje ani ADC, ani poziomu GPIO w pętli. Jedna startowa seria uzbraja PENIRQ i obsłużony jest panel przytrzymany już podczas startu. Impulsy samego PENIRQ podczas konwersji są pomijane; po serii sprawdzane jest puszczenie. Nie ma awaryjnego powrotu do pollingu przy niesprawnym IRQ. `TOUCH_RAW mode=IRQ` pokazuje liczbę aktywnych serii, poprawnych/odrzuconych próbek oraz zboczy wybudzających; cykliczny log nie oznacza cyklicznego pomiaru bez dotyku.

Pionowy ruch minimum24 px w dowolnym miejscu widoku terminala, także nad dolnym paskiem, włącza PAUZĘ i przewija co8 px o wiersz. Palec w dół odsłania starsze wiersze, w górę nowsze. Przesunięcie do offsetu0 nadal zachowuje pauzę; stuknięcie WZNOW wraca do LIVE. PAUZA/CR wykonują się po puszczeniu, więc rozpoczęcie gestu na przycisku nie przełącza go przypadkowo. CAL nadal wymaga2 s nieruchomego przytrzymania; pionowy drag anuluje CAL. Kalibracja i test punktów mają własną obsługę i nie przewijają logów.

`RETRO_SCROLLBACK_LINES=128` określa pojemność historii. Zachowujemy pełne komórki z kolorami/atrybutami oraz nieruchomą kopię29 wierszy widocznych przy pauzie: dodatkowo75360 B przy40 kolumnach i12 B/komórkę. Bufory przydzielane raz przy starcie, tylko w profilu z dotykiem; brak alokacji podczas gestów. Licznik `history=offset/liczba` jest w SCREEN, offset także przy WZNOW. To bufor ulotny, bez SD i bez trwałego zapisu.

Historia otrzymuje tylko wiersze wychodzące przez górę podczas przewijania całego podstawowego ekranu. Nie jest rejestratorem dowolnych zmian CUP ani archiwum pełnych ekranów aplikacji. Częściowy region, przewijanie w dół i ekran alternatywny nie dopisują historii. Pauza ekranu alternatywnego pokazuje jego nieruchomą kopię bez mieszania z historią podstawową. Napływ danych trwa podczas pauzy; nadpisanie najstarszych pozycji ogranicza dostępny zakres i może przesunąć przeglądany widok do najstarszego zachowanego wiersza. Kopia strony z chwili pauzy pozostaje niezmienna. RIS czyści historię; powrót do LIVE pokazuje bieżący model.

## Weryfikacja i późniejszy odbiór

Źródła opublikowane przed testami: `e94b1b2`, końcowa korekta CAL `53dca96`. [Wyniki i hashe](logs/2026-11-16_v0.3.9_validation.json): oba buildy SUCCESS, testy host PASS, Cppcheck12 C w każdym profilu bez zgłoszeń. Regresje obejmują rzeczywisty driver Espressif z wirtualnym SPI/czasem, brak odczytów dotyku w spoczynku, puszczenie/odrzucony ADC/timeout, gesty rozpoczynane nad każdym przyciskiem, kalibrację i przepełnienie historii. screen.c:634 porównania pełnego obrazu przy30 zdarzeniach; model VT10020 przypadków, pokaz5 ramek/5800 komórek PASS. To nie potwierdza poziomów elektrycznych ani fizycznego obrazu.

Dodatkowy test CAL pokazał konieczność rozróżnienia ciągłego kontaktu od ciągłej obserwacji pozycji. W końcowej wersji brak poprawnych współrzędnych przez80 ms anuluje przytrzymanie CAL nawet przy IRQ LOW lub pierwszej poprawnej próbce po długiej przerwie zadania. Kontakt/gest pozostaje ten sam aż do puszczenia; nie uzbrajamy nowego CAL tylko dlatego, że ADC wznowił pomiar. Regresja tej sytuacji i dotychczasowe testy gestów przechodzą.

**W tej zmianie nie otwieramy COM5/COM14, nie resetujemy i nie wgrywamy firmware.** Płytka pozostaje na v0.3.8. Później: zapis COM5 rozpoczęty przed resetem, porównanie obu zestawów LCD_REG, kilka EN i zimnych startów, identyczny pokaz VT100, następnie gesty nad całą powierzchnią podczas ciągłego strumienia HC-05. Kryteria: powtarzalny obraz, brak strat UART, brak samoczynnych PAUZA/CR/CAL, brak próbek ADC w spoczynku, poprawny powrót LIVE i zachowana kalibracja.

## Źródła pierwotne

- [ILI9341 DS V1.05](https://files.waveshare.com/upload/e/e3/ILI9341_DS.pdf): §8.2.12 s.101–102, §8.2.19 s.109, §12 s.213–214, §15.4 s.225, §18.3.4 s.237.
- [Espressif esp_lcd_ili9341, baza używana w projekcie](https://github.com/espressif/esp-bsp/blob/93460b932beba7022a8a4ec4186e0d6b7533d05f/components/lcd/esp_lcd_ili9341/esp_lcd_ili9341.c): reset, kolejność inicjalizacji,100 ms i zwrot draw_bitmap.
- [ESP-IDF6.0.1 esp_lcd SPI](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_lcd/spi/esp_lcd_panel_io_spi.c) oraz [SPI master](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_spi/src/gpspi/spi_master.c): transport, D/C i domyślna korekta hold0→1.
- [Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341/blob/master/Adafruit_ILI9341.cpp): initcmd, opóźnienia i begin.
- [TFT_eSPI ILI9341_Init.h](https://github.com/Bodmer/TFT_eSPI/blob/master/TFT_Drivers/ILI9341_Init.h): wariant podstawowy i ILI9341_2_DRIVER. [Zgłoszenie i odpowiedź autora o wariancie](https://github.com/Bodmer/TFT_eSPI/issues/1172) stanowią przykład różnic modułów, nie identyfikację naszego panelu.
- [TI ADS7846](https://www.ti.com/lit/ds/symlink/ads7846.pdf): referencyjny opis PENIRQ/power-down dla tej rodziny interfejsu. Moduł użytkownika to XPT2046; nie utożsamiamy go z oryginalnym układem TI.
