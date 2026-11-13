# Debug LED i propozycja GPIO LCD / dotyku — 02.11.2026

## Dwie diody na PCB

Na schemacie **Ai-Thinker NodeMCU-32 v1.3**, strona 13, czerwona **LED2** jest wskaźnikiem zasilania, a niebieska **LED1** jest połączona GPIO2 → R8 470 Ω → LED → GND, czyli aktywna stanem wysokim. Nazwy LED1/LED2 są oznaczeniami schematu, nie kolejnością na zdjęciu. Zdjęcie spodu płytki użytkownika pokazuje NodeMCU-32 v1.3; zgodność konkretnego egzemplarza i polaryzację należy potwierdzić testem LED lub pomiarem. Nie jest to wynik pomiaru PCB.

Źródło: [schemat producenta w specyfikacji NodeMCU-32 v1.3, kopia PDF](https://www.es.co.th/Schemetic/PDF/NODEMCU32-V1.3.PDF#page=13). Dioda zasilania nie jest sterowana przez firmware. GPIO2 nie jest przeznaczony dla LCD ani dotyku.

## Zachowanie debug LED w v0.1.6

| Stan | Sygnalizacja |
|---|---|
| Brak połączenia **SPP** (również podczas poprawnego uruchamiania stosu) | 500 ms ON / 500 ms OFF |
| Aktywne połączenie SPP | 1000 ms ON / 1000 ms OFF |
| Błąd nadajnika / odzyskiwanie profilu SPP | stałe ON; po udanym odzyskaniu wraca miganie |
| Błąd sterownika UART, brak heartbeat zadania, TWDT lub kontrolowany abort | stałe ON, zatrzaśnięte do resetu |
| Start po resecie z przyczyną PANIC, INT_WDT, TASK_WDT lub WDT | stałe ON do ręcznego resetu / odłączenia zasilania |

„Co pół sekundy / sekundę” oznacza **odstęp zmiany stanu**; pełne cykle wynoszą odpowiednio 1 s i 2 s. Nie jest to wskaźnik samego sparowania ani wykrywalności radiowej. Utrata połączenia bez błędu zmienia rytm na szybszy. LED odzwierciedla stan SPP zaakceptowanego przez aplikację.

Moduł `src/debug_led.c` korzysta z istniejącego zadania `esp_timer`, callback co 20 ms, bez dodatkowego zadania i bez pętli z opóźnieniami. Czysta logika czasu i heartbeat jest w `src/debug_led_logic.c`. Stan BT jest odczytywany pod krótką blokadą, bez zmiany architektury transportu.

Zadania UART TX/RX zgłaszają heartbeat; brak przez 2 s jest błędem. BT TX ma limit 6 s i zgłasza heartbeat także pomiędzy legalnymi oczekiwaniami do 5 s na zdarzenia restartu SPP. Zadania są nadzorowane od pierwszego heartbeat, nie przed ich uruchomieniem. Oczekiwanie na pustą kolejkę UART TX ograniczono do 250 ms, żeby brak ruchu nie wyglądał jak zawieszenie.

`esp_task_wdt_isr_user_handler()` i wrapper `esp_system_abort` zatrzaskują LED bez logowania, blokad i alokacji. Ścieżka błędu jest w IRAM, jej znaczniki w DRAM, wyjście ustawiane jest bezpośrednio rejestrem GPIO. Timer ponownie sprawdza zatrzask po zapisie, aby nie pozostawić OFF po błędzie zgłoszonym z drugiego rdzenia. Domyślna konfiguracja watchdogów i automatyczny restart po panic pozostają bez zmian. Rozpoznanie przyczyny resetu zachowuje sygnalizację ostatniej awarii po ponownym uruchomieniu firmware.

**Granica możliwości:** przy całkowitym zatrzymaniu CPU, obsługi przerwań i timera LED może zatrzymać się w ostatnim stanie. Program nie może wtedy zagwarantować przełączenia na ON. Gwarancja dla takiej awarii wymaga niezależnego sprzętowego watchdoga / układu monitorującego heartbeat. LED nie rozpoznaje także każdej sytuacji logicznej, w której zadanie nadal wysyła heartbeat. [Dokumentacja watchdogów ESP-IDF 6.0.1](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-reference/system/wdts.html).

Konfiguracja: domyślnie `DEBUG_LED_GPIO=2`, `DEBUG_LED_ACTIVE_LEVEL=1` w `debug_led.h`. Można je nadpisać flagami kompilacji po potwierdzeniu rzeczywistego PCB. Nie dodawać pull-up do GPIO2 w ciemno — jest pinem strapującym. Przy kłopotach z programowaniem po awarii wykonać reset przy wciśniętym BOOT; firmware nie steruje GPIO podczas resetu i ROM bootloadera.

## Rezerwacja pinów dla panelu 4-wire

Po pytaniu użytkownika o ADC rezerwujemy następujące **docelowe role**, a nie zweryfikowane połączenia shielda:

| Panel | Docelowy GPIO | Powód |
|---|---|---|
| X+ | **GPIO32 / ADC1_CH4** | wyjście do polaryzacji osi X i wejście ADC przy pomiarze Y |
| Y+ | **GPIO33 / ADC1_CH5** | wyjście do polaryzacji osi Y i wejście ADC przy pomiarze X |
| X− | **GPIO13**, warunkowo | wyjście do polaryzacji X / Hi-Z przy pomiarze drugiej osi |
| Y− | **GPIO14**, warunkowo | wyjście do polaryzacji Y / Hi-Z przy pomiarze drugiej osi |

GPIO34/35/36/39 pozostają dostępne jako wejścia ADC1, ale nie zastępują samodzielnie tych czterech GPIO: do pomiaru panelu trzeba na przemian wymuszać napięcie na obu osiach. Bez dodatkowego przełączania zewnętrznego GPIO32/33 są wygodniejsze, bo obsługują ADC i wyjście. Używamy ADC1; nie opieramy planu na ADC2.

**Dotyk musi współdzielić piny z LCD**, jeśli chcemy zachować UART2, konsolę, LED i nie używać GPIO0/12. Konkretny sygnał LCD przypadający na GPIO32/33/13/14 zależy od rzeczywistych połączeń X+/X−/Y+/Y− na PCB. Nie należy łączyć ich do przypadkowych linii D0/D1/WR/RS tylko na podstawie poniższego przykładu.

## Roboczy przydział samego LCD — do permutacji po pomiarze dotyku

**Aktualizacja 03.11.2026:** użytkownik wybrał HC-05 na osobnym UART. Poniższa historyczna tabela zużywa wszystkie 13 dostępnych wyjść dla samego LCD i **nie obejmuje HC-05**. Nie stosować jej jako gotowego schematu. Dodatkowy TX wymaga zwolnienia co najmniej jednego wyjścia LCD lub zmiany sprzętowej po ustaleniu PCB. Aktualne etapy: [TFT](TFT_BRINGUP.md), [HC-05](HC05_INTEGRATION.md).

Tabela jest **propozycją numerów GPIO po stronie ESP32**, nie potwierdzeniem mapowania złączy UNO ani gotowym schematem LCD+touch. Nie włączono tych pinów w firmware. Kontroler opisany przez użytkownika jako odpowiednik ILI9341 pozostaje do potwierdzenia odczytem ID / oznaczeniem; nazwa magistrali nie identyfikuje kontrolera.

| Sygnał LCD | Wstępny GPIO ESP32 |
|---|---|
| LCD_D0 | 32 (*) |
| LCD_D1 | 33 (*) |
| LCD_D2 | 18 |
| LCD_D3 | 19 |
| LCD_D4 | 21 |
| LCD_D5 | 22 |
| LCD_D6 | 23 |
| LCD_D7 | 27 |
| LCD_WR | 13 (*) |
| LCD_RS / DC | 14 (*) |
| LCD_CS | 5 (**) |
| LCD_RD | 15 (**) |
| LCD_RST | 4 |

(*) GPIO32/33 są zarezerwowane dla X+/Y+, a 13/14 wstępnie dla X−/Y−. Po ustaleniu, **z którymi sygnałami LCD rzeczywiście współdzielą się osie**, trzeba przenieść te GPIO na właściwe wspólne sieci i odpowiednio przestawić pozostałe pozycje tabeli. Nie zakładamy, że panel jest podłączony akurat do D0/D1/WR/RS. Jeśli panel nie współdzieli czterech linii możliwych do bezpiecznego przełączania, ten wariant wymaga zmiany projektu, np. zewnętrznego kontrolera dotyku.

(**) GPIO5 i GPIO15 są strapujące. Zaplanowano je celowo na sygnały nieaktywne przy HIGH: CS i RD. Warunkiem użycia jest potwierdzenie, że shield i bufory nie wymuszają niewłaściwych poziomów przy resecie, a ewentualne podciągnięcie jest do **3,3 V po stronie ESP32**. Nie przenosić ich na inne sieci przy permutacji bez ponownego sprawdzenia strapów. Jeśli CS współdzieli się np. z jedną z osi, trzeba przenieść ADC na tę sieć i ponownie dobrać role pinów strapujących.

Budżet: po odjęciu GPIO6–11 (Flash), 16/17 (PSRAM), 1/3 (konsola), 25/26 (UART2), 2 (LED), GPIO0/5/12/15 (strapping) i wejść 34–39 pozostaje **11 wyjść**. Pełny LCD 8-bit z WR/RS/CS/RD/RST potrzebuje **13**. Dlatego nie da się przy tych założeniach uzyskać trzynastu całkowicie niezależnych wyjść bez dwóch strapów lub uproszczenia sterowania. [Ograniczenia GPIO ESP32](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/api-reference/peripherals/gpio.html).

Wariant wyłącznie do zapisu może mieć CS stale aktywne i RD stale nieaktywne, oszczędzając dwa GPIO, ale pozbawia bezpośredniego odczytu ID i utrudnia bezpieczne odłączanie LCD podczas pomiarów dotyku. Nie jest rekomendowany jako docelowy wspólny interfejs LCD+touch bez analizy PCB. GPIO34–39 nie nadają się na magistralę danych ani WR/CS/RST.

Karta SD nie jest objęta tym przydziałem. Nie przypisujemy teraz dodatkowych SD_SS/MOSI/SCK ani backlight; sam wolny pin MISO nie wystarcza. Dalsze współdzielenie wymaga osobnego planu wyboru urządzeń. GPIO0 i GPIO12 zostają niewykorzystane, a 16/17 nigdy nie są kandydatami.

## Pomiary przed wykonaniem połączeń

1. Bez zasilania spisać ciągłość X+/X−/Y+/Y− → oznaczone linie LCD / złącza UNO. Rezystancja samej osi nie potwierdza połączenia ścieżki. Uzupełnić `PINOUT_TODO.csv` wynikami i jednostkami.
2. Prześledzić VCC, DIR i OE obu buforów HC245. Do ADC potrzebny jest dostęp do rzeczywistej analogowej sieci panelu — pomiar przez aktywny bufor cyfrowy nie jest prawidłowym pomiarem dotyku. Przy zmianie kierunku zapewnić Hi-Z i brak jednoczesnego sterowania przez LCD/bufor/ESP32.
3. Zmierzyć napięcia i podciągnięcia. Sam HC245 nie gwarantuje konwersji 5 V ↔ 3,3 V; odczyt nie może podać 5 V na ESP32. Progi wejściowe także zależą od napięcia i dokładnej odmiany układu. [Przykładowa karta katalogowa TI HC245](https://www.ti.com/lit/ds/symlink/sn74hc245.pdf) opisuje DIR/OE i progi, nie identyfikuje układów tego shielda.
4. Ustalić przebieg szyn 5V/3V3 i regulatora. Opis złącza nie wystarcza do jednoczesnego łączenia obu szyn z ESP32. Zasilanie shielda i poziomy sygnałów to oddzielne kwestie.
5. Dopiero po pomiarach zatwierdzić permutację GPIO LCD+touch i piny strapujące. Odczyt ID jest możliwy tylko, jeśli kierunki i OE faktycznie pozwalają bezpiecznie czytać. Sam przydział RD nie gwarantuje fizycznej ścieżki odczytu.
6. Docelowy pomiar dotyku: zatrzymać / zakończyć transfer LCD i DMA, odłączyć LCD/bufory od wspólnych linii, wykonać polaryzację i ADC, odtworzyć kierunki przed następnym transferem LCD. Nie przełączać GPIO podczas aktywnego transferu I80.

Na tym etapie firmware steruje wyłącznie debug LED i dotychczasowym UART/BT; nie uruchamia LCD, ADC ani dotyku.
