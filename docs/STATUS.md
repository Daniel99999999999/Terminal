# Faktyczny stan projektu / handoff

## 15.11.2026 — v0.3.5: gest CAL i pojedyncze zaniki próbek

[Zakres](TOUCH_GESTURES_v0.3.5.md). Usunięto natychmiastowe puszczanie gestu po pojedynczej odrzuconej próbce TEST/TERMINAL; limit80 ms, przerwa dłuższa kończy gest. CAL startuje na przycisku, ma histerezę po uzbrojeniu i postęp na pasku, logowane trafienia raw→screen. Pomiary SPI, progi kalibracji, piny i NVS nie zmienione. Kompilacje internal/touch SUCCESS (5,530/13,664 s; po poprawie ostrzeżenia o zakresie procentu w snprintf). Przed publikacją i testami; na ESP32 nadal v0.3.4 touch.

## 15.11.2026 — v0.3.4: powrót do krótkich ramek XPT2046

**Wynik CAL:** użytkownik zgłosił brak reakcji po około4 s przytrzymania napisu. [Log po zgłoszeniu](logs/2026-11-15_v0.3.4_cal_no_response.txt): runtime działa, licznik valid1393, bez restartu lub błędu API SPI w tym oknie. Przegląd UI wykazał, że każda próbka pressed=false zeruje s_holding i s_down; ten błąd przerywa CAL oraz może wielokrotnie przełączać PAUZA przy jednym kontakcie. Brak jeszcze logu współrzędnych trafienia; nie wykluczamy dodatkowo złego trafienia/mapy. Następna poprawka obejmie gest i jego diagnostykę.

**Po publikacji `541035d`:** [kontrole](logs/2026-11-15_v0.3.4_validation.json), test protokołu obu wariantów IRQ PASS, Cppcheck11 C internal/touch: kody0 bez zgłoszeń. Wgrano touch przez COM5 (tylko aplikacja, hash zweryfikowany). [Start15 s](logs/2026-11-15_v0.3.4_touch_boot.txt) bez resetów, powitanie3008 ms, mapa NVS wczytana. ADC ma teraz także niezerowe wartości Z1/Z2; w oknie bez nacisku valid0.

**Trzy przytrzymania wykonane przez użytkownika:** [log](logs/2026-11-15_v0.3.4_touch_press_watch.txt): irq_low620, valid356 w trzech oknach nacisku, bez błędów API SPI. Odczyt wrócił, lecz odrzucane próbki nadal występują; nie uznawać stabilności kalibracji ani gestów za potwierdzoną. [UART przy aktywnym dotyku](logs/2026-11-15_v0.3.4_touch_uart_result.json):17894 B, CRC40cad4e7,13 komend, bez strat i błędów. Próba fizyczna CAL/punktów/walidacji/GOTOWE została zlecona, oczekuje wyniku. Na ESP32 v0.3.4 touch, NVS zachowane.

[Zakres](TOUCH_FRAMES_v0.3.4.md). Wycofano niepotwierdzoną poprawę serii SPI na rzecz osobnych24-taktowych ramek z PD0=0 i buforami inline, jak w v0.3.1. Zachowano diagnostykę, filtr kalibracji, piny i mapę NVS. Kompilacje internal/touch SUCCESS (19,086/18,617 s), przed publikacją i testami. Przed tą próbą na ESP32 działała potwierdzona obrazem v0.3.3 internal bez dotyku. Nie przypisywać jeszcze przyczyny zerowego ADC konkretnemu formatowi SPI ani sprzętowi.

## 15.11.2026 — v0.3.3: pomiar nacisku niezależny od IRQ

**Najnowsza próba sprzętowa:** [porównanie i ograniczenia](TFT_TOUCH_TEST_2026-11-15.md). Przy trzech przytrzymaniach irq_low wzrósł do896, ale valid0, wszystkie obserwowane osie/Z1/Z2 zerowe, bez błędu API SPI. IRQ potrafi reagować — wcześniejszy stan HIGH nie jest dowodem jego stałej awarii. Użytkownik zgłosił biały, lekko migoczący LCD, także po pierwszym wgraniu profilu bez dotyku. Odbiór/model w obu profilach: po17894 B bez strat i błędów.

**Porównanie zachowanych plików:** v0.3.1 internal wyświetliła wzór poprawnie. Następnie **ten sam zachowany plik v0.3.3 internal także wyświetlił wzór poprawnie**, co potwierdził użytkownik. Brak potwierdzonej przyczyny wcześniejszego bielenia; nie ogłaszać naprawy ani regresji konkretnej wersji. Wgrana obecnie **v0.3.3 internal, dotyk OFF**, UART2/native BT/Wi-Fi nieaktywne, NVS zachowane; porty zamknięte. Dotyk wymaga dalszej diagnozy odczytu, kalibracja i pauza nie są jeszcze odebrane fizycznie.

**Po publikacji `1580d1a`:** [kontrole](logs/2026-11-15_v0.3.3_validation.json), [host XPT obu trybów](logs/2026-11-15_v0.3.3_host_xpt.txt) PASS. Cppcheck 11 C internal/touch: kody0, bez zgłoszeń. Upload touch COM5 SUCCESS, hash zweryfikowany. [Start15 s](logs/2026-11-15_v0.3.3_touch_boot.txt): bez restartów, powitanie3008 ms, mapa NVS wczytana. Do1060 odczytów po powitaniu: valid0, weak1060, io0, wszystkie obserwowane osie i Z1/Z2 zerowe, IRQ=HIGH. To brak fałszywych przyjęć w tym oknie, nie dowód poprawnego toru ADC. Powtórna próba z naciskiem została zlecona; oczekuje wyniku.

[Opis](TOUCH_POLLING_v0.3.3.md). Domyślnie RETRO_TOUCH_REQUIRE_IRQ=0: SPI/ADC jest odczytywane przy każdym odpytywaniu, a poprawny nacisk/zakres ustala dotknięcie niezależnie od GPIO36. Jawna opcja1 zachowuje poprzedni tryb. Dodatkowo Z1/Z2 i stan IRQ w diagnostyce. Bez zmian kalibracji/NVS i fizycznego połączenia. Kompilacje internal/touch SUCCESS (26,486/22,099 s), przed publikacją i testami. Oczekuje testów/uploadu; nie ogłaszać rozwiązania fizycznej przyczyny.

## 15.11.2026 — v0.3.2: poprawka odczytu i ciągłości kalibracji

**Wyniki po publikacji `52bf077`:** [kontrole i próba fizyczna](logs/2026-11-15_v0.3.2_validation.json). Test XPT2046 i kalibracji/UI PASS, Cppcheck 11 C w obu wariantach bez zgłoszeń. Upload touch COM5 SUCCESS, hash zweryfikowany. [Start 15 s](logs/2026-11-15_v0.3.2_touch_boot.txt) bez resetów, powitanie3006 ms; **wczytano poprawną kalibrację z NVS**. To potwierdza zapis/odtworzenie mapy, ale nie dokładność znacznika w próbie użytkownika.

**Próba trzech przytrzymań środka:** użytkownik potwierdził wykonanie; [log TOUCH_RAW](logs/2026-11-15_v0.3.2_touch_press_watch.txt) ma cały czas irq_low=0/valid=0. Wejście GPIO36 jest odczytywane jako HIGH i warunek w sterowniku blokuje wszystkie kolejne pomiary ADC. Nie znamy jeszcze powodu stałego HIGH; wynik nie dowodzi braku podciągania, awarii panelu ani nieprawidłowego progu nacisku. Kolejna kontrola ma odczytać SPI/ADC niezależnie od IRQ. Poprawa reakcji i PAUZA/WZNÓW nie są potwierdzone.

[Opis i ograniczenia](TOUCH_RESPONSE_v0.3.2.md). Zmieniono odczyty XPT2046 na jedną serię SPI z aktywnym ADC i końcowym przywróceniem PENIRQ. Filtr toleruje pojedynczy krótki zanik próbki, zachowuje wymagane osiem poprawnych/100 ms i pełne puszczenie. Dodano postęp na LCD oraz TOUCH_RAW z powodami odrzucenia co2 s. Progi nacisku/zakresu i piny nie zmienione. Kompilacje internal/touch SUCCESS (20,98/21,73 s), przed publikacją i nowymi testami. Dotychczasowy fizyczny objaw przytrzymań nadal wymaga weryfikacji; nie przypisywać go jeszcze brakowi pull-up ani konkretnemu podzespołowi.

## 15.11.2026 — XPT2046 podłączony, wgrana v0.3.1 touch

**Wynik próby:** w logu przyjęto punkt1 raw3536,611→24,24 i punkt2 raw3546,3669→295,24. [Zapis fragmentów kalibracji](logs/2026-11-15_v0.3.1_touch_calibration_partial.txt). Użytkownik zgłosił konieczność wielokrotnych naciśnięć i potwierdził, że nawet przytrzymanie 0,5–1 s bywa ignorowane. Hipoteza samych zbyt krótkich stuknięć nie wyjaśnia tego zgłoszenia; kalibracja i NVS nie są jeszcze potwierdzone. Przygotowywana jest ograniczona poprawka odczytu i filtru z licznikami powodów odrzucenia; przyczyna fizyczna nieustalona.

Odbiór przy aktywnym profilu dotyku: [COM14 17894 B, CRC zgodne, bez strat i błędów](logs/2026-11-15_v0.3.1_touch_uart_result.json), [log](logs/2026-11-15_v0.3.1_touch_uart_stream.txt). Brak dowodu pokrycia tego strumienia z faktycznym zapisem NVS — zapis jeszcze nie został potwierdzony.

Użytkownik potwierdził podłączenie pinów dotyku według wcześniej podanego planu SPI2: T_CLK14/T_DIN13/T_DO39/T_CS33/T_IRQ36. Wgrano opublikowaną v0.3.1 `esp32dev_touch` przez COM5, SUCCESS, hash zweryfikowany. [Log startu](logs/2026-11-15_v0.3.1_touch_boot.txt): bez restartów w 15 s, powitanie 3004 ms (511→3515), komunikat kalibracji pięciu punktów przy545 ms, internal_free221096 B, PSRAM_free0, UART2 nieaktywny. Nie ma błędu inicjalizacji dotyku; samo uruchomienie sterownika nie potwierdza jeszcze poprawnych współrzędnych.

Przekazano procedurę pięciu punktów, niezależnego punktu kontrolnego, testu zielonego znacznika i GOTOWE. Odczyt COM5 podczas kalibracji zbiera zdarzenia punktów i wyniku zapisu. Fizyczny wynik kalibracji, zapis/odtworzenie NVS i PAUZA/WZNÓW pozostają do potwierdzenia. Firmware nie zmieniono; poprzednie wyniki v0.3.1 pozostają ważne w opisanych zakresach, ale próby UART przy dotyku i zapisie Flash wymagają osobnego wyniku.

## 15.11.2026 — zlecone próby VT100 na v0.3.1

**Wykonano: 20/20 PASS w testowanym zakresie.** [Raport](VT100_TEST_2026-11-15.md), [zbiorcze wyniki](logs/2026-11-15_v0.3.1_vt100_summary.json). Testy `864838c`, istniejący firmware `0805f57`, bez nowego uploadu. Host: 239 jawnych porównań wybranych komórek (w części kolorów/atrybutów), strumień po jednym bajcie. ESP32 przez COM14: 9672 B, 1420 komend, dokładne pozycje kursora zgodne po każdym przypadku, CRC32 `ccf707e1`, brak strat/błędów UART i restartów/panic w logu. Jeden przypadek to test ujemny pominięcia DSR — odpowiedzi nadal brak, nie oznacza obsługi tej funkcji.

**Potwierdzenie fizyczne użytkownika:** „Tak, obraz i miganie są poprawne” po pytaniu o końcowy wzór: napisy, kolory RGB, ciągła ramka DEC i BLINK. [Obraz referencyjny](logs/2026-11-15_v0.3.1_vt100_expected.png). Dotyczy końcowego wzoru, nie wszystkich klatek pośrednich. COM5/COM14 wolne, na ESP32 pozostaje v0.3.1 internal, CR=NL, dotyk OFF. Nie deklarować pełnej zgodności VT100/xterm. Podłączenie i fizyczna kalibracja XPT2046 pozostają osobnym następnym etapem.

Przygotowanie: użytkownik potwierdził „Działa” po poprawce Enter i zlecił testy VT100. Opublikowano odtwarzalny test 20 przypadków, z jawnymi oczekiwaniami komórek/kolorów na hoście oraz cursor/controls/bytes/CRC z działającego ESP32. Aplikacja nie jest zmieniana ani ponownie wgrywana. Istniejące 23 scenariusze parsera i raster PASS, porty COM5/COM14 były dostępne. Wyniki nowych prób powyżej. DSR pozostaje niezaimplementowane i ma osobny test ujemny.

## 15.11.2026 — v0.3.1: Enter, pauza i pasek dotykowy

**Wyniki po publikacji źródeł `0805f57`:** [zapis kontroli](logs/2026-11-15_v0.3.1_validation.json). Parser 23 scenariusze PASS, kalibracja/UI (w tym pauza/wznowienie i wybór CR) PASS, raster PASS. Cppcheck warning/performance/portability: 11 aktywnych C osobno internal/touch z jawnymi makrami profilu i dotyku, bez zgłoszeń, oba kody 0.

**Wgrana wersja: v0.3.1 `esp32dev_internal`, dotyk OFF.** Upload COM5 SUCCESS, hash zweryfikowany. [15 s startu](logs/2026-11-15_v0.3.1_internal_boot.txt): bez resetów, powitanie 3001 ms, internal_free=224356 B, CR=NL, UART2 nieaktywny. [Test COM14](logs/2026-11-15_v0.3.1_enter_result.json), [surowy log](logs/2026-11-15_v0.3.1_enter_stream.txt): **20119 B** wysłane/odebrane/przetworzone, zgodne CRC, brak strat i błędów. Sam CR: kursor modelu (10,1), CRLF: (12,1), duży strumień i CRLF podzielone 200 ms przerwą: (18,3), wszystkie pozycje zgodne z oczekiwaniem, liczone od zera. Potwierdza to semantykę na modelu uruchomionym w ESP32; nie zastępuje obserwacji obrazu ani przechwycenia klawisza z konkretnej sesji PuTTY. Pozostawiono czytelny czterowierszowy test na LCD. COM5 i COM14 zamknięte po próbie.

**Pozostaje do wykonania:** podłączenie XPT2046 przez użytkownika, upload `esp32dev_touch`, fizyczne pięć punktów + walidacja/test, zapis i odtworzenie po resecie oraz PAUZA/WZNÓW przy napływie logów. Kod i kompilacje są gotowe, ale tych wyników sprzętowych jeszcze nie ma. Ustawienie CR=NL bez dotyku działa już na obecnie wgranym wariancie. PSRAM pozostaje wyłączona w obu wariantach internal/touch; przyczyna jej problemu startowego nadal nieustalona.

[Opis trybu CR i sterowania](DISPLAY_CONTROLS_v0.3.1.md). Po zgłoszeniu Enter z PuTTY dodano domyślną opcję CR→nowy wiersz; CRLF nadal przechodzi raz, także na granicach porcji. Wybierany CR=CR zachowuje standardowy VT i linie postępu. Na pasku dotykowym: PAUZA/WZNÓW po lewej, wybór CR pośrodku, CAL przytrzymane 2 s po prawej. Pauza dotyczy obrazu, odbiór/model pracują; scrollback nie jest jeszcze dodany. Pozycja kursora i ustawienia widoku w logu SCREEN. Kompilacje internal/touch SUCCESS (53,57/57,12 s), przed publikacją i testami. Dotyk nadal czeka na podłączenie przez użytkownika; nie deklarować fizycznego sprawdzenia pauzy/kalibracji.

## 15.11.2026 — v0.3.0: wybrany etap XPT2046 i kalibracji

**Kontrole po publikacji `c527b2e`:** kalibracja/UI z symulacją NVS, protokół XPT2046, trzy profile GPIO z odrzuceniem błędnych konfiguracji i raster — PASS. Cppcheck 11 aktywnych C osobno internal/touch, z jawnym makrem RETRO_TOUCH_ENABLE=0/1: oba kody 0, bez zgłoszeń warning/performance/portability. Fizycznej kalibracji nie wykonano: użytkownik dopiero podłączy dotyk.

**Test COM14 na nadal wgranej v0.2.3:** użytkownik zgłosił nadpisywanie wiersza po Enter w PuTTY i zwolnił COM14. Wysłano znane próbki CR/LF/CRLF: [227 B, dokładne liczniki i CRC UART1/parsera, brak strat i błędów](logs/2026-11-15_v0.2.3_enter_result.json), [log](logs/2026-11-15_v0.2.3_enter_stream.txt). Potwierdza to transport, nie odczyt pozycji liter ze zdjęcia. Przegląd modelu wykazał celowe standardowe CR→kolumna0, LF→następny wiersz. Następna zmiana doda wybierany tryb CR jako nowej linii oraz pauzę widoku dotykiem, bez zatrzymywania odbioru.

Użytkownik wybrał dotyk i kalibrację zamiast scrollback. [Instrukcja i implementacja](TOUCH_CALIBRATION_v0.3.0.md). Dodano dopasowanie pięciu punktów z niezależną walidacją, ekran testu, zapis NVS oraz profil `esp32dev_touch` z wewnętrznym RAM i ISR UART w IRAM. Wariant bez dotyku zachowany. Kompilacje internal/touch SUCCESS (65,57/66,66 s), przed publikacją i testami. Nowe testy nie zostały jeszcze uruchomione. Użytkownik potwierdził: **dotyk jeszcze niepodłączony, podłączy według planu T_CLK14/T_DIN13/T_DO39/T_CS33/T_IRQ36**. Upload i kalibracja fizyczna czekają na gotowe okablowanie; na ESP32 pozostaje v0.2.3 bez dotyku. [Wynik poprzedniej próby](RESULTS_v0.2.3.md).

## 15.11.2026 — v0.2.3: obrót o180° na podstawie nowego zdjęcia

Zdjęcie `20261115_170535.jpg` przedstawia faktycznie wgrany wzór v0.2.2: LEWY GORNY jest na dole po prawej, PRAWY DOL na górze po lewej, litery do góry nogami. To obrót o180°, co trafnie wskazał użytkownik. Pierwsze zdjęcie przedstawiało poprzedni wzór; nie należało traktować go jako obrazu po wgraniu v0.2.2. Zmieniono wyłącznie domyślny RETRO_TFT_ROTATION z1 na3 oraz wersję. Rozmiar320×240, tor UART1, piny i wariant RAM wewnętrznego zachowane. Build SUCCESS,19,83 s. Użytkownik zaakceptował ten obrót jako poprawne ustawienie i polecił pominąć dalsze opisy narożników oraz przejść do następnego etapu. Publikacja źródeł poprzedza upload/kontrolę startu; nie wymagać kolejnego testu narożników.

## 15.11.2026 — raport bootloopu i wynik próby orientacji v0.2.2

**[Szczegółowy raport bootloopu](BOOTLOOP_REPORT_2026-11-15.md):** etap i objaw potwierdzone, pierwotna przyczyna nieustalona. Nowe porównanie zachowanych ELF/BIN v0.2.0 z panic dump wykazało różne słowa kodu pod prawidłowym PC; [dowód GDB](logs/2026-11-15_v0.2.0_elf_comparison.txt). PSRAM nadal nie jest naprawiona, bieżący wariant używa RAM wewnętrznego.

v0.2.2 źródła `8808233` opublikowane po buildzie i przed testami. Upload COM5 SUCCESS, weryfikacja hash poprawna. [Odczyt15 s](logs/2026-11-15_v0.2.2_internal_boot.txt): brak resetów, powitanie3001 ms, internal_free224408 B. Test rastra PASS, Cppcheck9 C bez zgłoszeń. Wysłany przez COM14 asymetryczny wzór z opisami narożników i ABC123: [176 B, zgodne CRC, brak strat/błędów](logs/2026-11-15_v0.2.2_orientation_result.json), [log](logs/2026-11-15_v0.2.2_orientation_stream.txt). **Użytkownik nadal zgłasza zamianę lewej/prawej strony i zapowiedział nowe zdjęcie.** Nie uznawać orientacji za naprawioną; kolejne zmiany wstrzymane do obejrzenia aktualnego obrazu.

## 15.11.2026 — v0.2.2: korekta odbicia lewo–prawo

Zdjęcie użytkownika `20261115_165654.jpg` pokazuje tekst testu COM14 przy prawej krawędzi z odwróconymi literami. Wizualnie potwierdza pojawienie się tekstu i kolorów na LCD, ale z błędną orientacją. W tabeli obrotów MX było odwrócone względem standardowego mapowania ILI9341; poprawiono wszystkie cztery wpisy. Domyślne ROTATION1: MV=1, MX=0, MY=0, BGR=1 → MADCTL0x28, wcześniej0x68. [Referencja sterownika Adafruit](https://github.com/adafruit/Adafruit_ILI9341/blob/master/Adafruit_ILI9341.cpp#L209). Nie zmieniono pinów, fontu ani kolejności bajtów RGB565. Build `esp32dev_internal` SUCCESS, 24,62 s, przed publikacją i testami. Poprawna orientacja nowego obrazu nie jest jeszcze potwierdzona.

## 15.11.2026 — v0.2.1: terminal działa w RAM wewnętrznym, COM14 PASS

- **Potwierdzenie użytkownika na sprzęcie:** LCD jest podłączony, powitanie jest widoczne i dioda GPIO2 miga. Potwierdza to fizyczne wyświetlanie powitania oraz sygnalizację pracy w wariancie `esp32dev_internal`. Użytkownik nie potwierdził jeszcze kolorów i treści końcowego ekranu testu COM14.
- Źródła `c81849d` opublikowane po buildach i przed testami. Upload `esp32dev_internal` przez COM5 SUCCESS, hash zapisu zweryfikowany. **Ten wariant jest obecnie na ESP32.** Domyślny `esp32dev` nadal inicjalizuje PSRAM i nie jest potwierdzony jako działający; użycie zwykłego Upload może przywrócić bootloop. Do sprawdzonego wariantu służy osobne zadanie `PIO: Upload SPI TFT (internal RAM diagnostic)`.
- [Odczyt 15 s](logs/2026-11-15_v0.2.1_internal_boot.txt): brak restartów, regularne HEALTH/UART1/SCREEN, stabilne internal_free=224392 B. Odczyt zaczął się już po wczesnych logach app_main; powitanie zakończyło się po **3001 ms** (500→3501 ms). Czas zmierzono w logu; widoczność powitania i podłączenie LCD użytkownik potwierdził osobno.
- **COM14 → HC-05 master/slave → UART1 → parser PASS:** [wynik JSON](logs/2026-11-15_v0.2.1_uart1_result.json), [log](logs/2026-11-15_v0.2.1_uart1_stream.txt). Wysłane/odebrane/przetworzone **17894 B**, zgodne CRC32 odbioru i parsera, **0 błędów UART, 0 utraconych bajtów**, 13 sekwencji sterujących. Tekst zwykły, rozdrobnione ANSI/UTF-8 i ciągły dłuższy strumień. UART2 nieaktywny. Końcowe dane zawierają tekst i polecenia kolorów przeznaczone do wizualnego sprawdzenia na TFT.
- Cppcheck 9 aktywnych C osobno dla PSRAM i RAM wewnętrznego: bez zgłoszeń, oba kody0. VS Code tasks JSON poprawny. Regresje niezmienionego parsera/fontu/profili/XPT2046 opisane w wyniku v0.2.0 poniżej.
- **Otwarte:** wizualne sprawdzenie treści/kolorów testowego strumienia na LCD oraz przyczyna błędu standardowego startu PSRAM. Wariant RAM wewnętrznego omija jej inicjalizację, nie stanowi naprawy PSRAM. Nie uruchamiano natywnych BT/Wi-Fi ani zmian AT HC-05.

## 15.11.2026 — v0.2.1: jawna alternatywa RAM wewnętrznego

Dodano `esp32dev_internal`, bez inicjalizacji PSRAM, z tym samym nowym odbiornikiem HC-05 UART1 i rendererem. Domyślny `esp32dev` nadal włącza PSRAM; piny16/17 pozostają zarezerwowane. Oba buildy SUCCESS (45,11 s i61,99 s), przed publikacją. Próba sprzętowa tego wariantu jeszcze niewykonana. Nowe zadanie VS Code: `PIO: Upload SPI TFT (internal RAM diagnostic)`.

## 15.11.2026 — v0.2.0: wynik czystego rdzenia

Źródła `a738b45` opublikowano po 4 udanych buildach i przed testami. Upload esp32dev_core COM5 SUCCESS, hash zweryfikowany. [Log](logs/2026-11-15_v0.2.0_core.txt): nadal 63 restarty/15 s, brak app_main/READY. IllegalInstruction w standardowym `psram_cmd_end:414`, PC400d2cdc; stos s_psram_chip_init → esp_psram_chip_init → mspi_init. Żaden nowy moduł LCD/UART nie został uruchomiony. Sama przebudowa runtime i świeże defaults nie naprawiły startu PSRAM; nie utożsamiać tego z usterką fizycznej pamięci.

Po publikacji: parser22 scenariusze PASS, raster PASS, 3 profile i 3 nieprawidłowe konfiguracje PASS, XPT2046 symulacja PASS; py_compile serial_smoke PASS. Cppcheck 9 aktywnych C bez zgłoszeń, kod0. Test COM14 niewykonany, bo program jeszcze nie wystartował. Kolejny krok: jawny alternatywny wariant RAM wewnętrznego, bez dalszych zmian rejestrów/cache; domyślny profil PSRAM zachowany.

## 15.11.2026 — przebudowa v0.2.0 na polecenie użytkownika

[Nowa implementacja](REWRITE_v0.2.0.md). Aktualny tor: badane urządzenie → HC-05 master ⇄ HC-05 slave → **UART1 TX32/RX34 → TFT**. Moduły są połączone według użytkownika. Telefonu nie ma. UART2 nie jest inicjalizowany, most usunięty. COM14 jest wejściem testowego strumienia dla TFT. Usunięto eksperymentalne modyfikacje startu pamięci; świeża konfiguracja SDK. Końcowe kompilacje SPI, I80, headless i core: **4/4 SUCCESS**. Próby nowej wersji jeszcze niewykonane; commit/push źródeł ma je poprzedzać.

**Sprostowanie:** użytkownik jednoznacznie potwierdził fizyczne odłączenie LCD na czas wcześniejszej próby, także GND/LED z Arduino. Krótki napis przy przełączaniu USB Arduino mógł być przypadkiem; nie stanowi diagnozy zasilania ani potwierdzenia app_main. Aktualnego okablowania nie wywodzić z opisu poprzedniej próby.

## 15.11.2026 — v0.1.20: CS przywrócone, bootloop pozostaje

- Build SUCCESS, publikacja `4ebe112`, potem upload COM5 SUCCESS. Odczyt bez dodatkowego ręcznego resetu także pokazał bootloop (54 cykle / 15 s).
- [Log](logs/2026-11-15_v0.1.20_boot.txt): CTRL2 przed PSRAM=0x10, po SDK=0, po przywróceniu=0x10. Nadal uszkodzone końce linii instrukcji przy powrocie do s_psram_chip_init, PC 0x400db921. Same czasy CS nie są wystarczającą poprawką.
- Użytkownik zauważył wcześniej na chwilę napis na LCD; poproszono o treść/moment i aktualny stan podłączenia. Nie utożsamiać tego z potwierdzonym stabilnym startem — logi nie wykazały app_main.

## 15.11.2026 — v0.1.19: standardowy SDK/QIO też nie startuje

- Build SUCCESS 54,61 s, Flash 250976 B, publikacja `5745445`, potem upload COM5 SUCCESS. Cppcheck 12 C bez zgłoszeń (0). ELF SHA256 fac8b607927584174ef2fd540b2be89e66503e2e82d2c0e4993dfe9e1305c08d.
- Fizyczny LCD nadal odłączony. [Log](logs/2026-11-15_v0.1.19_boot.txt): bootloader QIO/40 MHz, IllegalInstruction w psram_read_id:456, PC 0x400de2c0; brak app_main. Sama konfiguracja QIO ze standardowym SDK nie jest rozwiązaniem.
- Usunięte eksperymentalne wrappery/linker fragmenty pozostają w historii Git, nie w aktywnych źródłach tej wersji. Następna kontrola ma dotyczyć czasów CS Flash, które SDK zmienia podczas konfiguracji PSRAM, a nie GPIO LCD.

## 15.11.2026 — korekta fizycznej izolacji / v0.1.18

- Użytkownik doprecyzował: **LCD wcześniej nigdy nie był odłączany**. Starsze sformułowania o bootloopie z fizycznie odłączonym LCD były błędnym odczytaniem wcześniejszej odpowiedzi; nie traktować ich jako wyniku testu. Profil headless v0.1.17 wyłączał obsługę w kodzie, ale LCD pozostawał fizycznie podłączony.
- Dopiero teraz użytkownik odłączył wszystkie przewody LCD przy wyłączonym zasilaniu, następnie włączył ESP32 i zwolnił COM5. [Bezpośredni odczyt bez LCD](logs/2026-11-15_v0.1.18_lcd_physically_removed.txt): ten sam IllegalInstruction / esp_clk_tree_initialize: PC 0x400e3390. 5 cykli w 6 s. Ta próba potwierdza występowanie obecnego błędu bez podłączonego LCD.
- v0.1.18: build SUCCESS i publikacja `3e9806e` przed uploadem; bootloader potwierdził **QIO / 40 MHz**. [Log z LCD](logs/2026-11-15_v0.1.18_boot.txt). PSRAM test PASS, potem nadal wyjątek. Sama zmiana QIO w połączeniu z eksperymentalnymi obejściami v0.1.17 nie naprawiła startu.
- Kolejny wariant usuwa nieskuteczne wrappery/zmiany linkowania, pozostawiając standardowy start SDK z QIO i powitanie. Nie deklarować żadnej dotychczasowej próby jako naprawy.

## 15.11.2026 — v0.1.17: bariera niewystarczająca, kontrola bez LCD

- Źródła `9e7a922` opublikowano po kompilacji i przed testem. SPI i headless skompilowane SUCCESS, kontrola IRAM rozszerzonych zależności PASS. Upload obu profili COM5 SUCCESS. Oba przechodzą test PSRAM, oba następnie wpadają w IllegalInstruction przed app_main. [SPI](logs/2026-11-15_v0.1.17_boot.txt), [bez LCD](logs/2026-11-15_v0.1.17_headless_boot.txt). Bariera cache nie usunęła problemu.
- Cppcheck aktywnych 13 C zgłosił 3 ostrzeżenia w makrze SDK REG_READ (asercja DPORT w diagnostyce); nie są to nowe błędy kompilacji. Nie deklarować analizy jako PASS. Wcześniejsze 12 C i pełny zestaw hosta przeszły.
- Flash odczytany przez esptool: producent 5E, device 4016, 4 MB, strap 3,3 V, status 0x100000 (**QE=0**). Sprawdzamy tryb Flash QIO i rolę HOLD/IO3 współdzielonego z PSRAM. Dla tego ID dokumentacja rodziny Zbit opisuje QE jako przełącznik funkcji HOLD/WP i Quad IO. To hipoteza do sprawdzenia, nie diagnoza zamknięta.
- Zweryfikowano kod i SDK: LCD SPI3 GPIO18/23/19/21/22/27, PSRAM CLK17/CS16, Flash6..11. Brak wspólnego pinu; awaria występuje przed funkcjami inicjalizacji LCD i także bez LCD. W logu MMU 205/206 również nie opisuje używanego zakresu kodu 400d..; właściwe wpisy to 77/78. Nie wyciągać wniosków o uszkodzeniu mapowania z poprzednich etykiet diagnostycznych.

## 15.11.2026 — v0.1.16: fizyczny odczyt Flash poprawny

- Build SUCCESS, publikacja `3241c93`, upload COM5 SUCCESS. [Log](logs/2026-11-15_v0.1.16_flash_read.txt): ROM read=0, poprawny nagłówek Flash `200206e9,400846c0,000000ee,00000000`; DROM magic `abcd5432` przed/po. PSRAM 8 MB, 4 MB mapowane, test pamięci PASS.
- Nadal wyjątek przed app_main: `esp_clk_tree_initialize`, PC `0x400e340c`, dump Flash ffffffff. Nie potwierdzono naprawy. Pole diagnostyczne opisane jako irom wypisywało wpisy 64..66; dla właściwego zakresu IROM0 trzeba użyć indeksów 205/206, więc wcześniejsze wartości 100 nie oznaczają braku mapowania kodu.
- Odczyt fizyczny jest poprawny; kolejnym krokiem jest bariera cache wokół rekonfiguracji PSRAM wraz z pełnym sprawdzeniem zależności IRAM/DRAM. Inicjalizacja LCD SPI3 nadal nie została wywołana, bo awaria występuje przed app_main.

## 15.11.2026 — v0.1.15: pomiar rejestrów MSPI

- Build SUCCESS (Flash 251060 B / RAM 31924 B), publikacja `e066c63`, potem upload COM5 SUCCESS. Wrapper tylko odczytuje rejestry. [Log](logs/2026-11-15_v0.1.15_mspi_registers.txt).
- Przed/po PSRAM: CTRL=00a02000, USER=f0000070, USER1=6c000002, CLOCK=00001001, MUX6=00001b00 bez zmian; PIN zmienia 1e→1c (włączenie CS1 dla PSRAM). Nadal IllegalInstruction z kodem Flash ffffffff. Brak app_main/TFT/transferu COM14.
- Pomiar nie wykazał zmiany trybu/dummy/zegara Flash. Kolejna diagnoza rozdzieli odczyt fizyczny Flash od mapowania cache/MMU, zamiast dodawać kolejne obiekty noflash.

## 15.11.2026 — v0.1.14: powitanie dodane, start nadal nieudany

- Kompilacja przed publikacją SUCCESS: Flash 250876 B, RAM 31856 B. Źródła `79f6b35` opublikowano przed testami (pierwszy push miał przejściowy błąd sieci, ponowienie udane). Pełne testy hosta PASS, Cppcheck 12 C bez zgłoszeń, rozszerzona kontrola IRAM PASS.
- Wgranie COM5 i hashe zapisu SUCCESS. PSRAM rozpoznany jako 8 MB / 40 MHz. Następnie IllegalInstruction przy `esp_mmu_map_get_max_consecutive_free_block_size`, PC `0x400e1159`; dump kodu Flash zawiera `ffffffff`. [Cykl logu](logs/2026-11-15_v0.1.14_psram_bootloop.txt). 51 startów w 15 s, brak app_main, więc powitanie jeszcze nie jest wykonywane.
- Zachowano dopasowane ELF/bin/map lokalnie. Nie rozszerzać noflash mechanicznie: trzeba ustalić, dlaczego odczyt Flash po konfiguracji PSRAM zwraca złe dane. Test COM14 nie był jeszcze uruchomiony. Użytkownik potwierdził radio COM14 do UART1 GPIO32/34; TFT otrzymuje UART2 RX.

## 15.11.2026 ? v0.1.13: kontrole i negatywny test COM5

- ?r?d?a `caf6bde` opublikowano przed testami. Cztery buildy SUCCESS: esp32dev 250488/30592 B Flash/RAM, SPI 250372/30592, I80 232660/30156, headless 221160/30068. Wszystkie zestawy hosta PASS; Cppcheck 12 C ? 3 profile bez zg?osze?; kontrola helper?w IRAM/stanu DRAM 4/4 PASS. PSRAM 40 MHz i brak natywnych komponent?w RF potwierdzone.
- Po wyra?nej zgodzie u?ytkownika wgrano przez COM5 obraz esp32dev v0.1.13 (ELF SHA256 `29b3e738d64de42354e5cfb089ae468f0b652a8ea2001538d3f8cf6c3c0e4960`). Esptool: zapis i weryfikacja hash SUCCESS.
- **Bootloop nadal wyst?puje.** 15 s logu zawiera 56 start?w, brak app_main. Nowy PC `0x400db83f` / `s_psram_chip_init`, esp_psram.c:214, po powrocie z impl_enable. EXCVADDR `0x800db96e`. [Pierwszy cykl](logs/2026-11-15_v0.1.13_psram_bootloop.txt). Zachowano lokalnie dopasowane ELF/bin/map w `.pio/audit/v0113_failed_*`.
- Kontrola rozmieszczenia samych helper?w nie obejmowa?a wywo?uj?cego kodu i jego litera??w w Flash. Nie deklarowa? v0.1.13 jako naprawy; nast?pna zmiana rozszerzy zakres startu bez dost?pu do Flash. Fizyczny TFT nadal nie zosta? uruchomiony.

## 15.11.2026 — bootloop v0.1.12 i przygotowanie v0.1.13

- Użytkownik: LCD SPI biały, debug LED nie miga, dotyk niepodłączony. Bootloop utrzymuje się po odłączeniu wszystkich przewodów LCD. Użytkownik wybrał w `display_profile.h` domyślny SPI; tę zmianę zachowano.
- Po zwolnieniu COM5 przechwycono 94 208 B logu przez 10 s: powtarzalny **LoadProhibited, EXCVADDR=0xffffffff**, `rst:0xc`, przed app_main. Ostatni zwykły log to start PSRAM przy ok. 200 ms. [Jeden cykl i dekodowanie adresów](PSRAM_BOOT_FIX_v0.1.13.md); segmenty odpowiadają obrazowi SPI v0.1.12. Nie jest to dowód uszkodzenia LCD/PSRAM, nie ma komunikatu brownout.
- Lokalny `psram_startup.lf` kieruje obiekty SDK wczesnego startu PSRAM i rezerwacji GPIO do noflash, bez edycji SDK/pinów/zegarów. W oryginalnym ELF helper wywoływany podczas rekonfiguracji kontrolera był w Flash. Poprawkę rozmieszczenia należy potwierdzić kontrolą ELF i uruchomieniem nowego obrazu.
- Domyślny SPI pozostał, dotyk OFF; dodano jawny wariant headless oraz logi etapów app_main/TFT. Przed publikacją źródeł skompilowano v0.1.13: `esp32dev` (domyślny SPI bez dotyku) **SUCCESS**, 64,32 s, Flash 250 488 B / statyczny RAM 30 592 B; I80 **SUCCESS**, 100,81 s, Flash 232 660 B / RAM 30 156 B. Pozostałe kompilacje i testy zostaną opisane osobno. Testów nowej wersji jeszcze nie uruchomiono. Nie wgrywano nowej wersji; COM5 udostępniono do odczytu.

## 15.11.2026 — v0.1.12: wyniki kontroli po publikacji źródeł

- Zgodnie z poleceniem: po udanej kompilacji trzech wariantów opublikowano źródła [2c1806e](https://github.com/Daniel99999999999/Terminal/commit/2c1806e), **następnie** uruchomiono testy. Wszystkie kontrole poniżej dotyczą tych samych C/H; po publikacji nie poprawiano kodu.
- **Host PASS:** 6 dotychczasowych zestawów (terminal 22 scenariusze, LED 11, HC-05 policy/GPIO, raster z dodatkowym RGB565 SPI oraz archiwalne BT 16 / HCI 13), 2 warianty archiwalnego adaptera HCI, **3 profile GPIO**, test rzeczywistego modułu XPT2046 z symulacją SPI (24-bitowe ramki/PD=00, IRQ, mediana/outlier, nacisk, zwolnienie dotyku i żywotność deskryptora po timeoutcie) oraz **3 odrzucone nieprawidłowe konfiguracje**. Kompilator hosta bcc64x/C11. Symulacja nie jest próbą fizyczną SPI.
- **Cppcheck: PASS**, 12 plików C aplikacji, osobno NONE/I80/SPI, kategorie warning/performance/portability, bez zgłoszeń, kody 0. CMake `compile_commands.json` nie zawiera makra dodawanego przez PlatformIO/SCons, więc do projektów analizy jawnie dodano odpowiednie `RETRO_TFT_PROFILE=0/1/2`. Wybór transportu potwierdzono niezależnie w rzeczywistych plikach obiektowych przez symbole I80/SPI i kolejki XPT2046.
- We wszystkich trzech wynikach: wersja **0.1.12**, PSRAM włączony/40 MHz, HC-05 115200; brak komponentów `bt`, `esp_wifi`, `esp_phy`, `esp_coex`, `wpa_supplicant`, ich archiwów w mapach i implementacji natywnych API radiowych w ELF (pominięto absolutne adresy funkcji ROM).

| Profil | Flash / statyczny RAM | Plik bin | ELF SHA256 |
|---|---|---|---|
| NONE / `esp32dev` | 221 192 / 14 144 B | 221 616 B | `78c51f4daf7773a869e47683ab0422e5cfe1fb7ecc9de34d9b5e9d6de0bb74df` |
| I80 / `esp32dev_i80` | 232 536 / 14 216 B | 232 960 B | `c0a706e6161e59659f0a1931badd31159298ff762969d62aac25b113d96f7c70` |
| SPI + XPT2046 / `esp32dev_spi` | 251 556 / 14 728 B | 251 984 B | `1ef5fd13baaefbc43eb6fc4ec85b0517a076d2f3748c5d714f8852b4e9939fa3` |

- Artefakty lokalne: `.pio/audit/tft_v0112_build.txt`, `v0112_build_audit.json`, `v0112_compile_*.json`, `v0112_cppcheck_*.txt`, `.pio/host-tests/result.txt`. Sprawdzono także poprawność JSON zadań VS Code i brak błędów whitespace w diffie.
- **Bez uploadu i próby sprzętowej.** Nie potwierdzono fizycznego obrazu, surowych odczytów XPT2046 ani wydajności szyny 3V3 z podświetleniem. Brak kalibracji/UI dotyku, SD pozostaje planem. Wybierać jeden pinout; starego I80 bez podświetlenia nie uznano za naprawiony ani definitywnie uszkodzony.

## 15.11.2026 — v0.1.12: alternatywne TFT i plan SD

- Na polecenie użytkownika przygotowano profile `#define`: NONE / ILI9341 I80 / ILI9341 SPI + XPT2046. Pełny [pinout i konfiguracja](TFT_PROFILES_v0.1.12.md), [przyszły zapis na SD](FUTURE_SD.md). Dotychczasowy shield i nowy moduł są alternatywami, nie wspólną magistralą.
- Użytkownik potwierdził VCC i LED nowego modułu SPI **3,3 V**. LCD SPI3 18/23/19, CS21/DC22/RST27; XPT2046 SPI2 14/13/39, CS33/IRQ36. LED na 3V3; opcjonalne GPIO4 tylko do EN zewnętrznego drivera. GPIO16/17, UART-y i debug LED2 zachowane. Nowy dotyk RAW ma log DOWN/UP; nie ma kalibracji ani UI. Natywne radio nadal wyłączone.
- **Kompilacja przed publikacją: 3/3 SUCCESS**, espressif32 7.0.1 / ESP-IDF 6.0.1: NONE 50,66 s, Flash 221 192 B / statyczny RAM 14 144 B; I80 77,21 s, Flash 232 536 B / RAM 14 216 B; SPI + XPT2046 75,73 s, Flash 251 556 B / RAM 14 728 B. Log: `.pio/audit/tft_v0112_build.txt`.
- Kolejność wymagana przez użytkownika: udana kompilacja, commit i publikacja nowszej wersji przed testami. W chwili przygotowania commita źródeł **testów hosta i analizy statycznej tej wersji jeszcze nie uruchamiano**; ich wyniki zostaną zapisane osobno. Nie wykonywano uploadu ani próby sprzętowej.

## 15.11.2026 — TFT: brak podświetlenia przy samym zasilaniu

- Użytkownik zgłosił zasilanie shielda 5 V i poprawną pracę wewnętrznego stabilizatora 3,3 V. Nie podał osobnych odczytów napięcia na LED ani na HC245.
- Test użytkownika: po odłączeniu linii sygnałowych ESP32, zasileniu samego shielda wyłącznie przez 5V/GND i pozostawieniu pinu 3V3 niepodłączonego nadal **brak podświetlenia**, także w ciemności. Objaw występuje bez sterowania z ESP32; nie jest to jeszcze dowód uszkodzenia LED lub całego LCD.
- Następny krok: zidentyfikować na rzeczywistym PCB/taśmie zasilanie i powrót prądu podświetlenia oraz ewentualny rezystor/tranzystor sterujący. Dotychczasowe zdjęcia nie wystarczają do pewnego wskazania padów LED+/LED−. Potrzebne ostre zbliżenie tego obszaru; styki X+/X−/Y+/Y− są stykami dotyku, nie podświetlenia. Nie podawać 5 V bezpośrednio na nieustalone pady LED i nie omijać ograniczenia prądu.
- Firmware pozostaje v0.1.11, TFT domyślnie OFF. Nie zmieniano kodu, nie wykonywano uploadu ani nowej próby radia. Nie ma logu potwierdzającego wersję aktualnie wgraną na ESP32.

## 13.11.2026 — końcowe kontrole v0.1.11 po pushu źródeł

- Kod opublikowano **przed końcowymi testami**, zgodnie z poleceniem: [46ea0fa](https://github.com/Daniel99999999999/Terminal/commit/46ea0fae02ec845b2c58fccabec2426289e6fb40). Po tym commicie nie zmieniano C/H ani logiki testów; drugi commit zapisuje wyniki i wygenerowany `sdkconfig.esp32dev` bez nieużywanych komponentów.
- Testy hosta **PASS**: 6 zestawów (terminal 22 scenariusze, LED 11, HC-05 policy/GPIO, raster oraz archiwalne BT 16 / HCI 13) i 2 warianty archiwalnego adaptera HCI. Archiwalne testy używają `baseline/v0.1.10/src`, nie kodu linkowanego do nowego firmware. To nie jest test współbieżności UART/FreeRTOS ani RF.
- Domyślny `esp32dev`: **SUCCESS**, kod 0, 57,18 s, espressif32 7.0.1 / ESP-IDF 6.0.1. Flash **232 284 B**, statyczny RAM **14 216 B**, bin **232 720 B**, ELF SHA256 `fbacee5765fc5543c51d407920d9681c7d6748cff85346a63cfe2f74175e20ff`. HC-05 115200; TFT/STATE/KEY OFF.
- Osobny wariant testowy: **SUCCESS**, kod 0, 58,35 s. TFT, STATE, KEY, wzór kolorów i inwersja ON, obrót 1. Flash **233 784 B**, statyczny RAM **14 232 B**, bin **234 208 B**, ELF SHA256 `ed75d89d9369bfac112d0051ba1b71d53cd355c40e67c24d27fadab5d77838a1`. Potwierdzono efektywne flagi i symbole I80/ILI9341/rastera/przełączania AT. Konfiguracja i build są izolowane w `.pio/`; nie włączono tych opcji w domyślnym firmware.
- Cppcheck: **10 plików C aplikacji, obie konfiguracje, bez zgłoszeń warning/performance/portability**, kod 0. W obu buildach brak komponentów `bt`, `esp_wifi`, `esp_phy`, `esp_coex`, `wpa_supplicant`, brak ich archiwów w `Terminal.map` i implementacji API radiowych w ELF (stałe adresy funkcji ROM nie są implementacjami dołączonymi do aplikacji).
- Sprawdzono 65 sum SHA256 manifestów baz, niezmienione bloby upstream ILI9341/licencji, niezmienione bajty glifów fontu oraz zachowanie parametrów PSRAM/Flash/konsoli. Artefakty lokalne: `.pio/audit/hc05_tft_default.json`, `hc05_tft_features.json`, logi buildów/Cppcheck oraz `.pio/host-tests/result.txt`.
- **Bez uploadu i bez próby sprzętowej.** Płytka nie została tym zadaniem przeprogramowana; ostatni znany obraz na niej nadal to stara referencja `ref-fullcal-6.0.1`. Wykluczenie natywnego radia dotyczy nowego firmware. Nie potwierdzono obrazu TFT, transferu fizycznego HC-05 ani elektrycznej poprawności shielda.

## 13.11.2026 — źródła v0.1.11: HC-05, I80/ILI9341 i usunięcie natywnego radia

- Użytkownik zlecił dodanie HC-05 i TFT, doprecyzował klon ILI9341 oraz potwierdził UART HC-05 **115200**. Nowy most i renderer są opisane w [instrukcji](HC05_TFT_v0.1.11.md). STATE/KEY/TFT są opcjonalne i domyślnie nie konfigurują swoich GPIO.
- Z bieżącego main/src usunięto natywne SPP/GAP/HCI; usunięto wrappery i zależności, adapter kontrolera nie jest dołączany. Minimalny build ogranicza komponenty do aplikacji i zależności. Poprzedni kod zachowano w `baseline/v0.1.10` (26 plików) i Git. Nie uruchomiono nowej próby natywnego BT.
- Pierwszy draft przeszedł testy hosta; pierwszy build wykrył brak definicji wersji w lokalnie dołączonym sterowniku Espressif, co uzupełniono w CMake. Przerwano kolejną kompilację po doprecyzowaniu usunięcia komponentów radiowych. To nie są wyniki końcowego kodu.
- **Kolejność na polecenie użytkownika:** zakończyć źródła, podnieść wersję do 0.1.11, commit + push, następnie końcowe kontrole i osobny commit wyników. W chwili przygotowania tej notatki końcowe testy nowego kodu i minimalnego buildu pozostają do wykonania.
- Nie wykonywano uploadu ani testu fizycznego HC-05/UART1/TFT. Opis ILI9341 jest deklaracją użytkownika; poziomy i HC245 wymagają pomiarów. FabGL sprawdzono w źródłach autora: gotowy ILI9341 jest SPI/C++, więc użyto natywnego `esp_lcd` I80 i obecnego modelu w C.

## 13.11.2026 — przydział HC-05 i TFT bez dotyku

- Użytkownik zlecił pierwsze połączenie HC-05 przez RXD/TXD/GND/VCC i rezerwacje EN/STATE na później; TFT z D0–D7 i RST/CS/RS/WR/RD, na razie bez dotyku. [Pełna tabela](PIN_ASSIGNMENT_HC05_TFT.md).
- Plan ESP32: UART1 TX32/RX34, STATE35, EN/KEY warunkowo12. LCD: D0–D7=13/14/18/19/21/22/23/27, WR33, RS15, CS5, RD0, RST4. Zachowano debug LED na GPIO2, UART2 TX25/RX26, konsolę1/3 i PSRAM16/17. Wszystkie 15 wyjść są zajęte po doliczeniu przyszłego KEY; GPIO36/39 pozostają tylko wejściami. Osobna LED połączenia może korzystać z STATE przez bufor/tranzystor.
- To plan po stronie ESP32, nie pomiar sieci shielda. GPIO0/5/15 wymagają właściwych stanów startowych; RD0 przy BOOT=LOW wymaga rzeczywiście nieaktywnego CS. GPIO12 zostaje teraz niepodłączone: zachować fabryczny strap VDD_SDIO, ustalić obciążenie i poziomy EN oraz sekwencję AT z ESP32. Dawny plan ADC dotyku nie obowiązuje.
- Użytkownik potwierdził **ZS-040 i sprawdzone wejście AT przez EN**. Plan zasilania: 5 V na VCC nośnika z regulatorem; logika 3,3 V. Nie wykonano pomiaru STATE ani testu połączenia z ESP32. Zasilanie TFT, HC245, kierunki i kontroler nadal wymagają pomiarów. Zmieniono wyłącznie dokumentację, firmware nadal v0.1.10, bez uploadu i bez implementacji UART1.

## 13.11.2026 — v0.1.10: automatyczny model ANSI/VT100

- Nowe wymaganie: domyślnie biały tekst na czarnym tle, automatyczna interpretacja sterowania terminalowego. Dodano parser strumienia `ansi_parser`, czysty model `terminal` i adapter kolejki `terminal_view`. Obsługiwany podzbiór obejmuje kolory 16/256/RGB, atrybuty, pozycję kursora, czyszczenie/edycję, regiony przewijania, ramki DEC i ekran alternatywny ?1049. UTF-8 i komendy zachowują stan między porcjami. [Zakres i ograniczenia](TERMINAL_EMULATION.md).
- UART2 RX wysyła kopię danych do modelu przed warunkiem aktywnego BT; kolejka send=0, parser w osobnym zadaniu. Dwie strony logiczne 80×24 zajmują 46 080 B PSRAM. Renderer ma API kopiowania wierszy; nie dodano sterownika ani GPIO LCD. Błędy/utraty są liczone, a częściowy parser resetuje się przy przerwie strumienia. Nowe zadanie jest objęte heartbeat LED 2 s, również przy pustej kolejce.
- Główny firmware ma wersję **0.1.10**. Zapisano bazę v0.1.9 z nazwami użytkownika i COM5, manifest 7 plików; wcześniejsze bazy zachowane. UART2 TX25/RX26, konfiguracja SDK, transport i ustawienia BT pozostają zachowane. Nie wykonywano nowego eksperymentu BT. Autobaud potwierdzony w sprzęcie/API, lecz nie jest jeszcze włączony w aplikacji.
- Końcowy build **SUCCESS**, kod 0, 11,24 s po pełnym buildzie 204,38 s; platform 7.0.1 / ESP-IDF 6.0.1. Flash **702 260 B**, statyczny RAM **46 692 B**, bin **702 688 B**, ELF `b83766c96663abc8b7922043fbe3cbe978cf4c59d529dcdb905a9ec8f6b8cc79`. W końcowym ELF są funkcje modelu/parsera; hooki LED/abort/TWDT nadal w IRAM.
- Testy hosta: **16 BT + 11 LED + 13 HCI + 22 terminal = 62 scenariusze PASS**, plus 2 warianty adaptera HCI PASS. Terminal: wszystkie punkty podziału strumienia, UTF-8, sekwencje błędne/długie, kolory, scroll/alt, bufory z wartownikami i 20 000 bajtów strumienia adversarial. Cppcheck 10 C projektu, unix32/C11 z bazą kompilacji: bez zgłoszeń warning/performance/portability, kod 0.
- **Nie wykonano uploadu ani testu na TFT/UART2.** To wdrożony model ekranu w pamięci, nie potwierdzony obraz na shieldzie. Ostatni potwierdzony program na płytce to ref-fullcal-6.0.1 z logu COM5. Pomiary TFT/HC245, identyfikacja kontrolera, font/viewport, renderer i integracja HC-05 pozostają do wykonania. Artefakty: `.pio/audit/terminal_v0110.json`, `terminal_v0110_cppcheck.txt`, `.pio/host-tests/result.txt`.

## 12.11.2026 — ponowny log połączenia z komputerem, COM5

- Użytkownik wyjaśnił, że [nowy log](logs/2026-11-12_reference_fullcal_windows_excerpt.txt) dotyczy **połączenia z komputerem**. **Android nadal nie wykrywa ESP32**; nie przypisywać mu zdarzeń ACL z tego logu.
- Nadal uruchomiona referencja `ref-fullcal-6.0.1`, ELF `3769a91ad...`, kontroler `a5d37fe`, zgodny adres BT. COM5 / 115200; główny platformio.ini użytkownika ma już COM5.
- ACL status=0 / handle128 przy 20964 ms; błąd odczytu remote extended features/status=0x02 przy 50994 ms, No Connection przy 51004 ms; disconnect 0x22 przy 51114 ms. Od zestawienia ACL do rozłączenia **30,15 s**, brak AUTH success i SRV_OPEN w dostarczonym logu. Wynik ponawia wcześniejsze niepowodzenie, nie ustala przyczyny.
- Bez nowych prób BT, uploadu lub zmian firmware. Priorytet nadal TFT i zewnętrzny HC-05. Pierwsze pomiary zasilania U2/U3 pozostają do uzyskania.

## Decyzja użytkownika — koniec prób wbudowanego BT; priorytet TFT i HC-05

- Użytkownik dopuścił jedną ostatnią próbę i po niej wybrał **TFT LCD** oraz **zewnętrzny HC-05** do Bluetooth. Ostatnią próbę wykonano; **Samsung nadal nie widzi urządzenia**. Nie kontynuować eksperymentów z wbudowanym BT bez nowego polecenia użytkownika. Przyczyna nie została rozpoznana; nie deklarować awarii sprzętu.
- Ostatni wariant: `rf_fullcal`, oryginalne C Espressif i SDK 6.0.1, wyłączone ładowanie/zapis kalibracji PHY z NVS. SDK wykonuje pełną kalibrację przy starcie. Bez kasowania NVS, bez zmiany GPIO, SSP, mocy, ERTM lub kontrolera. Jedyna dodatkowa różnica konfiguracji to wersja aplikacji i jej źródło w Kconfig.
- Build **SUCCESS**, kod 0, 142,57 s; Flash **653 824 B**, RAM **45 812 B**, bin **654 256 B**. SHA źródeł upstream oraz głównego kodu/config i globalnego kontrolera sprawdzone. Efektywna konfiguracja potwierdzona w wygenerowanym sdkconfig.json. Audyt: `.pio/audit/fullcal_reference_build.json`.
- Upload na COM10 **SUCCESS**, kod 0, 18,92 s; hash flash zgodny. Boot odczytany: **ref-fullcal-6.0.1**, ELF **3769a91ad...**, INIT/START. [Odczyt UART](logs/2026-11-03_reference_fullcal_boot.txt). Użytkownik następnie potwierdził niewidoczność. Monitor agenta zamknięto, COM10 zwolniono. Ten wariant pozostaje na płytce do czasu następnego uploadu.
- **Windows na ostatniej referencji także nie paruje**, komunikat użytkownika o braku odpowiedzi. [Wyciąg z jego logu](logs/2026-11-03_reference_fullcal_windows_excerpt.txt): ACL handle=128/status=0 przy 197984 ms; błąd odczytu remote extended features/status=0x02 przy 227994 ms, No Connection przy 228004 ms; rozłączenie tego samego handle z 0x22 przy 228114 ms. Czas ACL → disconnect **30,13 s**. Brak AUTH success i SRV_OPEN w dostarczonym logu. Sekwencja nie ustala konkretnej przyczyny timeoutu. To oryginalny kontroler `a5d37fe`, nie wariant poprawki Realtek z głównego v0.1.9. Nie otwierać kolejnej serii prób wbudowanego BT.
- **TFT:** przejrzano istniejące zdjęcia shielda, HC245 i styków dotyku. Brak pomiarów sieci/napięć i ID kontrolera. Użytkownik ma multimetr. Pierwsza partia to ciągłość zasilania U2/U3 do szyn 3V3/5V przy całkowicie odłączonym zasilaniu i ESP32. [Procedura](TFT_BRINGUP.md), [arkusz pomiarów](TFT_MEASUREMENTS.csv). Nie przypisano nowych pinów LCD ani nie uruchomiono jego GPIO.
- **HC-05:** docelowo osobny UART1; UART2 TX25/RX26 oraz konsola USB zostają. Potrzebne dodatkowe wyjście oznacza, że dotychczasowa tabela samego LCD nie mieści już całego zestawu bez zmiany sterowania. [Plan integracji](HC05_INTEGRATION.md). Dokładny wariant modułu, zasilanie, baud i STATE niepotwierdzone.

## 03.11.2026 — referencja wgrana i potwierdzona na COM10

- Ostatni negatywny wynik Androida użytkownika dotyczył **Terminal v0.1.9 / ESP32-Terminal**, ELF `371e4d797...`, kontroler `003f89d`, marker `btfix=83ede08`. Log potwierdza INIT/START i HCI 0x03, bez ACL/AUTH/SPP. Nie był to wynik oryginalnego przykładu Espressif. Zmienione przez użytkownika nazwy w głównym kodzie zachowano.
- Agent wykonał upload z jawnym `-d diagnostics/esp_idf_spp_acceptor` na COM10: **SUCCESS**, kod 0, 17,04 s; zapis 655 888 B aplikacji, hash flash zweryfikowany. Bez kasowania całej flash/NVS.
- Odczyt UART potwierdza **bt_spp_acceptor_demo / ref-idf-6.0.1**, ELF `f54c58dd7...`, oryginalny kontroler `a5d37fe`, ten sam adres BT oraz `ESP_SPP_INIT_EVT` / `ESP_SPP_START_EVT`. [Wyciąg z odczytu](logs/2026-11-03_reference_boot_excerpt.txt). Monitor agenta zamknięto, COM10 zwolniono.
- **Wynik zwykłej referencji: nadal niewidoczna na Samsungu**, według odpowiedzi użytkownika po potwierdzonym uploadzie i starcie. Brak potwierdzonego parowania/transferu. To wynik odrębny od wcześniejszej próby Terminal v0.1.9. Późniejsza ostatnia próba pełnej kalibracji także negatywna, jak opisano wyżej.

## 03.11.2026 — porównanie publicznego kodu, referencja Androida, poprawka Realtek

- Użytkownik żąda porównania z opublikowanym kodem i diagnozy Androida, nie kolejnych samych logów ustawień. Przeczytano oficjalny akceptor SPP v6.0.1 oraz `nopnop2002/esp-idf-uart2bt`, commit `397a1138bfa2f5680be18f4e5f9d85ab2a422751` (Classic C, instrukcja Androida ze zrzutami). [Raport i linki do kodu](BT_CODE_COMPARISON_2026-11-03.md).
- **Android — następny konkretny krok:** [niezmieniony akceptor Espressif](../diagnostics/esp_idf_spp_acceptor/README.md) w osobnym projekcie. Zadanie **BT reference: Upload**, nazwa **ESP_SPP_ACCEPTOR**, app version **ref-idf-6.0.1**. Kod C, komponent CMake i Kconfig są identyczne bajtowo z przykładem; wrapper PlatformIO i config dopasowano do WROVER. Używa oryginalnego kontrolera 6.0.1, tak jak przetestowany v0.1.8; nie dołącza poprawki Realtek ani naszych modułów UART2/LED/HCI.
- Build referencji: **SUCCESS**, kod 0, 177,63 s; Flash **655 460 B**, statyczny RAM **45 812 B**. W chwili przygotowania bez uploadu; późniejsze wgranie i boot opisano wyżej. Kryteria: widoczność, AUTH success i SRV_OPEN; oryginalny przykład nie implementuje echa. Nie kopiowano GPIO16/17 z projektu społecznościowego.
- **Główny terminal v0.1.9 — Windows:** znaleziono znaną regresję biblioteki kontrolera (#18145) i brak oficjalnej poprawki 83ede08 w zainstalowanym SDK. Git blob starego archiwum `c010e165274ddbeca2ec45030cf273cc227676ac` potwierdzono z wszystkich bajtów i porównano z GitHub API. Lokalny adapter komputera: Realtek Bluetooth Adapter, VID_0BDA/PID_4853. Zgodny producent jest poszlaką, nie dowodem identycznej przyczyny.
- Do projektu dodano oryginalne archiwum `esp32-bt-lib@10f4e9ad6260788765863ed7f05e0e649b288e36`, blob `4600c8f40b4ec887570ccf7defc40241ea306f12`, licencję i metadane. CMake po `project()` zmienia tylko IMPORTED_LOCATION targetu `bt_btdm_app`, z weryfikacją SDK 6.0.1 oraz obu hashy. Globalna instalacja nie została nadpisana; jej archiwum nadal ma SHA256 `9552f29e...29e7db`.
- Build v0.1.9: **SUCCESS**, kod 0, 63,93 s; Flash **695 916 B**, statyczny RAM **46 076 B**. Mapa linkowania zawiera tylko poprawione archiwum z vendor. Sprawdzono 18 hashy bazy v0.1.8, niezmienność SDK/config/modułów i fakt, że pierwotny diff main.c ograniczał się do wersji/markera btfix. Nie powtarzano testów hostowych dla podmiany binarnego kontrolera. Później użytkownik zmienił nazwy na Terminal / ESP32-Terminal i uruchomił program; Android nadal niewidoczny. [Pełny log](logs/2026-11-03_v0.1.9_android_not_reference.txt). Wyniku Windows po poprawce nie dostarczono.
- **Granica wniosków:** poprawka upstream dotyczy części problemów Realtek/Windows; nie rozpoznano nią przyczyny niewidoczności Samsunga. Poważnego błędu aplikacyjnego C wyjaśniającego oba objawy nie dowiedziono. Różnice START/nazwa/scan i obsługa interakcji parowania są opisane w raporcie; nie zmieniano ich razem z biblioteką.

## 03.11.2026 — otrzymane wyniki v0.1.8

- [Pełny log Androida](logs/2026-11-03_v0.1.8_android_hci.txt) ma właściwy ELF `492fe63bf...`, v0.1.8 / identity_test=2 / hci_scan_diag=1. Kontroler potwierdził WRITE_SCAN_ENABLE=0x01, potem **0x03**, oba status=0. Do 126757 ms attached=1/drops=0/trace_complete=1/pending=0/confirmed=1/scan=0x03, bez kolejnych zmian/resetu/ACL/AUTH/SRV_OPEN. To pozytywny wynik konfiguracji HCI, nie odbioru RF ani niezależnego READ.
- Telefon zgłasza „Nie znaleziono żadnych urządzeń”, ale użytkownik potwierdził w osobnym teście kontrolnym, że inne znane urządzenie Bluetooth jest widoczne. Nie proponować ponownego sprawdzania samego Scan_Enable ani zakładać ogólnej niesprawności skanowania telefonu.
- [Wyciąg Windows](logs/2026-11-03_v0.1.8_windows_excerpt.txt): timeout handle=128 przy 405017 ms; początku tego łącza nie pokazano. Nowe ACL status=0 / handle=129 przy 412847 ms; koniec wycinka 422347 ms, dalszy wynik nieznany. Nie scalać dwóch handle ani nie wyliczać z nich czasu jednego połączenia. Windows zgłasza brak odpowiedzi przy dodawaniu/parowaniu, nie przy otwieraniu COM.
- Udane ACL potwierdza przynajmniej chwilową wymianę radiową; poprzednie sugestie uszkodzenia płytki były zbyt mocne. Nadal nie dowiedziono sprawności stabilnego transferu, przyczyny LMP 0x22 ani zakończenia parowania. UART RX=41/offline=41, błędy=0; brak wiedzy o źródle/payloadzie nie pozwala zaliczyć testu UART.

## Przygotowany v0.1.8 — pasywny test HCI, 02.11.2026

- Dodano `bt_hci_diag.c/.h` oraz testowalny filtr/FIFO `bt_hci_scan.c/.h`. Linkerowe wrappery obserwują istniejącą ścieżkę Bluedroid ↔ kontroler. Nie dodają nadawcy HCI, nie modyfikują pakietów ani ustawień parowania. `identity_test=2` pozostaje taki jak w ostatnim teście.
- Obserwujemy RESET / READ_SCAN_ENABLE / WRITE_SCAN_ENABLE i ich odpowiedzi, bez kluczy/payloadów. Kolejka 32 elementów, send timeout=0; wydruk z istniejącego UART RX co 5 s. Dopasowanie WRITE FIFO do 16 żądań. Brak pasującego żądania, overflow lub błąd unieważniają potwierdzenie; utrata śladu pozostaje widoczna do resetu. Cisza na HCI w stanie bezczynności nie kasuje ostatniej obserwacji.
- Build końcowego kodu: **SUCCESS**, kod 0, 15,87 s po wcześniejszym pełnym buildzie bibliotek; Flash **695 164 B**, statyczny RAM **46 076 B**, ESP-IDF **6.0.1**, espressif32 **7.0.1**. Pierwsza próba buildu wykryła nieobsługiwane założenie o atomowym RMW; liczniki obserwatora chronią teraz krótkie sekcje portMUX, bez wywołań API pod blokadą.
- Testy: **16 BT + 10 LED + 13 HCI PASS**, adapter produkcyjny ze stubami hosta/RTOS w trybach 1 i 0 PASS (przekazywanie pakietów/callbacków/zwrotów, brak logów w transporcie, pełna kolejka, błąd rejestracji). To nie jest test współbieżności kontrolera/FreeRTOS ani RF. Wariant wyłączony sprawdzono także Xtensą `-fsyntax-only`; Cppcheck 7 plików C bez zgłoszeń warning/performance/portability.
- Końcowa mapa linkowania potwierdza referencje Bluedroid do obu wrapperów i wrapperów do realnych API kontrolera. Zachowano hooki LED w IRAM. SDK/config/GPIO oraz moduły BT TX/LED i CoD/EIR pozostały zachowane. Dokładna kopia poprzedniej wersji jest w `baseline/v0.1.7`, manifest SHA256 zawiera 14 plików.
- W chwili przygotowania agent nie wykonał uploadu ani testu sprzętowego v0.1.8. Późniejszy wynik użytkownika z 03.11 opisano wyżej; kryterium HCI z [procedury](BT_HCI_TEST_v0.1.8.md) zostało spełnione. Nie jest to niezależny READ ani test anteny.

## Najnowszy wynik v0.1.7 / tryb 2 — 02.11.2026

- Użytkownik potwierdza: **BT nadal niewidoczne na Androidzie**. [Pełny log trybu 2](logs/2026-11-02_v0.1.7_identity2_samsung.txt) zawiera `identity_test=2`, INIT status=0 przy 2156 ms, START status=0 / SCN=1 przy 2236 ms, CoD Computer 0x000100 przy 2316 ms.
- Żądanie `scan_mode connectable/general` otrzymało ESP_OK przy 2166 ms. SSP=1, IO_CAP_NONE request=ESP_OK, zapisanych bondów=0. Kod oczekuje na przychodzące połączenia i SSP Just Works, bez przycisku/parowania ograniczonego czasowo w aplikacji. W lokalnym SDK Just Works jest potwierdzane automatycznie w `btc_dm_sp_cfm_req_evt`; nie należy mylić braku CFM w logu aplikacji z wyłączonym parowaniem.
- Jawne `config_eir` otrzymało ESP_OK przy 2266 ms; późniejszy CONFIG_EIR status=0 przy 2286 ms zawiera 0x09/0x03/0x05/0x07/0x01. Ostatni typ to pole flags dodane w tej próbie. Nie ma dowodu zawartości UUID16 ani odbioru przez telefon.
- Do 326456 ms brak ACL/AUTH/SRV_OPEN, brak resetu/panic/heartbeat stale, dane/błędy/recovery=0. Tryby CoD 1 i jawnego EIR 2 nie usunęły niewidoczności. Wcześniejszy Windows ACL/timeout dotyczy v0.1.6, nie tej sesji.
- **Potwierdzone:** ustawienia aplikacji i lokalne zdarzenia. **Niepotwierdzone:** rzeczywisty Scan_Enable kontrolera i odbiór RF przez telefon. ESP_OK z GAP jest akceptacją żądania do kolejki, nie odczytem HCI ani testem radiowym.
- Następny test: ślad HCI z `Write Scan Enable` (0x0C1A), ostatnim parametrem 0x03 i Command Complete status=0; docelowo również `Read Scan Enable` (0x0C19), status=0 / wartość=0x03. To włączenie Inquiry Scan + Page Scan, a nie dowód działania anteny. Nie ma publicznego `esp_bt_gap_get_scan_mode()` w lokalnym 6.0.1; nie zastępować pomiaru ponownym logowaniem żądanych stałych. [Specyfikacja HCI](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html).
- W tej analizie zachowano log i zaktualizowano dokumentację. Nie zmieniono firmware/SDK, nie wykonano buildu ani uploadu. Przyczyna problemu pozostaje nieustalona.

## Wynik v0.1.7 / tryb 1 i przygotowanie trybu 2 — 02.11.2026

- Użytkownik dostarczył [pełny log v0.1.7 / identity_test=1](logs/2026-11-02_v0.1.7_identity1_samsung.txt) i potwierdził, że Samsung nadal nie widzi ESP32. CoD Computer nie usunął objawu w tej próbie.
- INIT status=0 przy 2266 ms, START status=0 / SCN=1 przy 2346 ms. Nazwa/adres są zgodne z poprzednim logiem; po zmianie klasy lokalny CoD=0x000100, major=1/minor=0 przy 2396 ms. Inicjalizacja nie pozostała w oczekiwaniu; linia `initialization requested` została wypisana już po callbackach.
- Automatyczny CONFIG_EIR status=0, typy 0x09/0x03/0x05/0x07. Typ pola UUID16 nie potwierdza jego zawartości 0x1101 ani odbioru przez telefon. Brak ACL/AUTH/SRV_OPEN w całym fragmencie do 261486 ms; IO, błędy i recovery pozostają zerowe, brak resetu/panic/heartbeat stale.
- W źródłach zmieniono wyłącznie domyślny wybór `BT_DISCOVERY_TEST` z 1 na **2**, uruchamiając wcześniej przygotowaną jawną konfigurację EIR po START. Nie zmieniono parametrów bezpieczeństwa, mocy RF, pinów ani danych NVS. To kolejna próba porównawcza, nie potwierdzona naprawa.
- Pełny build trybu 2: **SUCCESS**, kod 0; Flash 692 972 B, statyczny RAM 46 012 B; 9,66 s. Agent nie wykonał uploadu; późniejszy test użytkownika opisano wyżej. Testów hostowych nie powtarzano dla samej zmiany wyboru wariantu; wcześniejsze wyniki poniżej.
- Zaplanowany wtedy test trybu 2 jest już zakończony wynikiem negatywnym. [Instrukcja i wyniki](BT_DISCOVERY_TEST_v0.1.7.md). Wynik Windows z poprzedniej wersji pozostaje oddzielnym, nierozwiązanym problemem parowania.

## Przygotowanie v0.1.7 — test identyfikacji CoD/EIR, 02.11.2026

- Użytkownik potwierdził, że Samsung nadal nie widzi ESP32 przy wyłączonym Bluetooth Windows, po resecie ESP32 i skanowaniu z 30–50 cm. To eliminuje niedokończone połączenie PC jako wyjaśnienie tej konkretnej próby. Nie dostarczono nowego logu UART z izolowanego skanu.
- Przygotowano trzy tryby w `bt_discovery_diag.h`: 0 obserwacja, **1 pierwotnie domyślnie Computer CoD**, 2 Computer + jawne EIR z nazwą/UUID profili. To test hipotezy, nie dowód przyczyny niewidoczności i nie potwierdzona poprawka timeoutu LMP.
- CoD jest ustawiany po SPP START; log pokazuje klasę przed zmianą i późniejszy odczyt lokalnego stosu. Dodano wynik konfiguracji EIR/typy pól, zdarzenia szyfrowania, jawny kod HCI przy rozłączeniu, flagi SSP/SC i odczyt liczby bondów bez ich modyfikacji.
- SSP/Just Works, SC, ERTM, moc RF, nazwa/MAC, piny, LED, UART i konfiguracja SDK pozostają takie jak w bazie v0.1.6. Pełną kopię tej bazy zapisano w `baseline/v0.1.6`; v0.1.4 także jest zachowany.
- Pełny build trybu 1: **SUCCESS**, Flash 692 704 B, statyczny RAM 46 012 B. Kompilator Xtensa sprawdził tryby 0/1/2 (`-fsyntax-only`); 26 testów hostowych PASS, Cppcheck dla 5 plików C bez zgłoszeń w sprawdzanych kategoriach. Wtedy nie wykonano uploadu; późniejszy test użytkownika opisano wyżej.
- [Dokładny test v0.1.7](BT_DISCOVERY_TEST_v0.1.7.md). Pierwsza próba z trybem 1 jest zakończona; kolejną oddzielną próbą jest tryb 2.

## Najnowszy wynik sprzętowy v0.1.6 — log użytkownika, 02.11.2026

- Użytkownik uruchomił v0.1.6 na ESP32; log potwierdza SDK 6.0.1, pulę PSRAM 4 MB, inicjalizację UART2 i LED, własny adres **7C:87:CE:2F:DE:DA**, poprawny odczyt nazwy oraz SPP START na kanale 1. Sam log nie potwierdza fizycznego działania LED ani przesyłania danych UART.
- **Windows widzi urządzenie**, lecz zgłasza brak odpowiedzi podczas dodawania/parowania. Log pokazuje dwa udane ACL connect. Pierwsze łącze zerwano po ok. 30,09 s z HCI **0x22 — LMP Response Timeout**; GAP reason=290 to ten sam kod z offsetem 0x100. Druga próba nie ma w pokazanym fragmencie końcowego wyniku.
- Samsung **Galaxy A55 5G SM-A556B/DS, Android 16** nie widzi ESP32 w systemowym ekranie Bluetooth, nie tylko w aplikacji terminala.
- Brak AUTH completion, SRV_OPEN i jakichkolwiek DATA_IND/WRITE/CONG. Echo SPP i stabilna komunikacja nadal niepotwierdzone. Brak restartów / heartbeat stale w tym fragmencie; recovery=0.
- Udane ACL wskazuje na przynajmniej chwilową dwukierunkową wymianę radiową; nie rozstrzyga jakości RF ani źródła timeoutu LMP. Późniejszy izolowany skan Samsunga przy wyłączonym Windows BT i po resecie ESP32 także zakończył się niewidocznością, według odpowiedzi użytkownika.
- [Pełna analiza i plan testu](BT_LOG_ANALYSIS_2026-11-02.md), [oryginalny log](logs/2026-11-02_v0.1.6_windows_pairing.txt). W ramach analizy nie zmieniono źródeł firmware, ustawień SDK ani NVS.

## Firmware v0.1.6 — debug LED i plan GPIO, 02.11.2026

- Dodano debug LED domyślnie GPIO2 active HIGH według schematu referencyjnego NodeMCU-32 v1.3. Czerwona LED2 na schemacie jest PWR; niebieska LED1 jest sterowana GPIO2. Egzemplarz użytkownika nie został sprawdzony pomiarem ani nowym uploadem.
- Zmiana ON/OFF co 500 ms bez SPP, co 1000 ms z aktywnym SPP. Błąd/odzyskiwanie SPP daje ON do powrotu usługi; zawieszony heartbeat, błąd UART, TWDT i abort zatrzaskują ON do resetu. Po restarcie z przyczyny panic/watchdog sygnalizacja błędu pozostaje do ręcznego resetu. Całkowitego zatrzymania CPU nie można objąć gwarancją programową.
- Rezerwy dotyku: X+ GPIO32 / ADC1_CH4, Y+ GPIO33 / ADC1_CH5; X− GPIO13 i Y− GPIO14 warunkowo. Wymagają mapowania wspólnych sieci z LCD i toru analogowego przez/obok HC245. Robocza tabela LCD w [LED_AND_PIN_PLAN.md](LED_AND_PIN_PLAN.md) podlega permutacji po pomiarach; nie uruchomiono LCD ani ADC.
- Testy hostowe: 16 scenariuszy BT oraz 10 scenariuszy LED/heartbeat — PASS. Wynik kompilacji, sprawdzenie IRAM/wrappera i testy wymagające urządzenia zapisano w [TESTS.md](TESTS.md).
- Ustawiono jawne `PROJECT_VER=0.1.6` w CMake, ponieważ obecne puste repozytorium Git nie ma jeszcze commitów, a automatyczne wykrywanie wersji ESP-IDF zatrzymywało konfigurację. Nie zmieniano historii ani ustawień Git.
- Baza v0.1.4 i aktywne ustawienia SDK/UART2/PSRAM pozostały zachowane. W środowisku agenta nie wykonywano uploadu; późniejszy test użytkownika opisano wyżej. LCD i dotyk nie są uruchomione.

## Firmware v0.1.5 — naprawy P1/P2, 01.11.2026

- Wprowadzono naprawy pięciu ustaleń A1–A5: krótkie odczyty UART, trwały stan zapisu SPP z timeoutem i restartem profilu, generacje sesji, logi GAP/SPP oraz zdarzenia/liczniki UART. Szczegóły: [FIXES_v0.1.5.md](FIXES_v0.1.5.md).
- Zachowano oryginalną bazę wraz z konfiguracją w `baseline/v0.1.4`. Aktywny `sdkconfig.esp32dev`, `sdkconfig.defaults`, przypisania GPIO25/26 oraz ustawienia platformy pozostały bez zmian. LCD/dotyk/RF nie były modyfikowane.
- Testy na komputerze: **16/16 PASS** dla produkcyjnego modułu stanu nadajnika; C11, `-Wall -Wextra -Werror`. Cppcheck 2.11: kod 0 dla obu plików `.c`, bez zgłoszeń warning/performance/portability. To nie są testy współbieżności FreeRTOS ani sprzętu.
- Pełny build `SPP_TEST_ECHO=1`: **SUCCESS** na `espressif32 7.0.1` / ESP-IDF `6.0.1`. Końcowe rozmiary są zapisane w sekcji kontroli v0.1.5 w [TESTS.md](TESTS.md). Wariant `SPP_TEST_ECHO=0` dodatkowo sprawdzono kompilatorem Xtensa przez `-fsyntax-only`; nie wykonano osobnego pełnego linkowania tego wariantu.
- **Nie wgrano v0.1.5 na ESP32.** Wykrywalność Android, echo, rzeczywisty transfer UART i odzyskiwanie profilu SPP wymagają testów z TESTS. Nie stwierdzono przyczyny niewykrywalności ani uszkodzenia RF.

## Potwierdzone w logu użytkownika (v0.1.4, 18.10.2026)

- ESP-IDF **6.0.1**, PlatformIO `espressif32 7.0.1`, platform `esp32dev`.
- Układ ESP32-D0WD rev 3.0; bootloader zgłasza Flash **4 MB**.
- `esp_psram`: **Found 8MB PSRAM device**, `4MB is mapped`, `SPI SRAM memory test OK`.
- Po starcie aplikacji: internal heap total **280 447 B**, wolne **226 555 B**; PSRAM total **4 194 304 B**, wolne **4 192 024 B**.
- Po inicjalizacji Bluetooth: internal heap total **285 619 B**, wolne **177 579 B**; PSRAM wolne **4 186 564 B**, największy blok **4 128 768 B**.
- UART2 uruchomiony: `TX=GPIO25 RX=GPIO26, 115200 8N1`.
- Kontroler BT skonfigurowany w trybie Classic=2; serwer SPP: `SPP listening as 'ESP32-RetroTerminal'`.
- `main_task: Returned from app_main()` to normalny koniec zadania inicjującego — nie oznacza resetu.

## Potwierdzone przez użytkownika (stan po uruchomieniu v0.1.4)

- Android **nie widzi** `ESP32-RetroTerminal`. Widziany wcześniej MAC `60:D6:A9:E3:99:87` należy do **innego urządzenia**; nie przypisywać go ESP32.
- Wi-Fi tego modułu jest niesprawne według wcześniejszych doświadczeń użytkownika (przyczyna sprzętowa jeszcze nieustalona).
- Na płytce ESP32 widać czerwoną diodę zasilania; druga LED przypisywana do GPIO2 na podstawie schematu wersji NodeMCU-32 v1.3 — przed użyciem sprawdzić zgodność konkretnego egzemplarza.
- Shield TFT 2,4" (czerwona płytka, Arduino UNO-like), nadruk `2.4" TFT LCD SHIELD`, FPC `CK241 REV0.1 2009.03.27`, dwa układy z oznaczeniem rodziny `HC245`.
- Fotografie mikroskopowe pokazują cztery wyprowadzenia rezystancyjnego panelu: **X+, X−, Y+, Y−**. **Nie** potwierdzono mapowania na piny shielda.

## Nieustalone / nie testowane

- Fizyczny stan toru RF, anteny i skuteczność komunikacji BT na odległość.
- Faktyczny model kontrolera LCD: **ILI9341 to założenie, nie wynik odczytu ID**.
- Napięcia i poziomy kierunków buforów HC245 na shieldzie.
- Pinout wszystkich linii LCD oraz dotyku i ich zgodność z ESP32-WROVER.
- Fizyczne wyświetlanie na LCD i praca touch; model ANSI/VT100 dodano w v0.1.10, integracja renderera nadal nie jest wykonana.
- Dwukierunkowy transfer z rzeczywistym urządzeniem UART oraz Bluetooth echo — brak potwierdzenia połączenia telefonu.

## Historia

- v0.1.2: program restartował się na `esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)` z `ESP_ERR_INVALID_ARG`; nieodpowiednie GPIO16/17 dla WROVER.
- v0.1.4: poprawiono tryb inicjalizacji kontrolera i UART na GPIO25/26; boot i SPP listening działają, ale Android nie znajduje urządzenia.
- Ta paczka zachowuje źródło **v0.1.4**, bez nowej wersji firmware i bez przeprowadzenia buildu podczas pakowania.

## Audyt lokalny — 01.11.2026

- [Raport audytu](AUDIT_2026-11-01.md) zawiera ustalenia, propozycję małego patcha diagnostycznego BT oraz testy regresji. Firmware v0.1.4 i konfiguracja pozostały bez zmian.
- Potwierdzono zainstalowane PlatformIO `espressif32 7.0.1` i ESP-IDF `6.0.1`; kompilacja `run -e esp32dev` zakończyła się **SUCCESS**. Flash według PlatformIO: 677 104 B; statyczny RAM: 45 852 B. Nie są to pomiary heapu działającego urządzenia.
- Analiza kodu wykazała oczekiwanie UART na pełne 128 B oraz ryzyko zatrzymania TX i przekazania starych danych przy zmianie sesji SPP. Te ścieżki nie zostały odtworzone na sprzęcie.
- Nie wykonano nowego wgrywania ani testów UART/Android/RF. Przyczyna niewykrywalności Bluetooth nadal jest nieustalona; start SPP nie potwierdza sprawności radia.
