# XPT2046 bez bramkowania pomiaru przez IRQ — v0.3.3

## Dowód poprzedzający zmianę

Po próbie trzech przytrzymań środka ekranu użytkownik potwierdził wykonanie. v0.3.2 odpytywała dotyk, ale irq_low i valid pozostały zerowe. Kod kończył odczyt przy IRQ=HIGH, więc brak danych raw nie mówił nic o odpowiedzi ADC. [Wyniki i log v0.3.2](logs/2026-11-15_v0.3.2_validation.json). Ta wersja poprawnie wczytała kalibrację z NVS; nie kasujemy jej.

## Zakres

`RETRO_TOUCH_REQUIRE_IRQ=0` w `src/display_profile.h` jest domyślne. Przy każdym wywołaniu sterownika wykonywana jest istniejąca 30-bajtowa seria SPI. Zadanie ekranu odpytuje około co10 ms; 240 bitów przy1 MHz to nominalnie240 µs na magistrali. Nie dodano pętli oczekiwania ani opóźnień. Pozostaje timeout20 ms i zatrzymanie sterownika po błędzie transportu.

Dotknięcie rozpoznaje dotychczasowy próg Z1/Z2 oraz zakres osi. Stan IRQ nie odrzuca poprawnej próbki. Bez nacisku próbka musi być odrzucona także przy HIGH. `RETRO_TOUCH_REQUIRE_IRQ=1` przywraca wejściowe i końcowe bramkowanie przez GPIO36. Oba warianty mają osobną regresję. Nie oznacza to pełnej weryfikacji elektrycznej IRQ ani panelu.

Pomiar bez obowiązkowego IRQ jest przewidziany również przez [bibliotekę autora XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen/blob/master/XPT2046_Touchscreen.cpp): opcjonalny pin przerwania i detekcja z nacisku. W projekcie pozostaje własny sterownik C/ESP-IDF. [Karta XPT2046](https://spinmodules.com/wp-content/uploads/2025/01/XPT2046-datasheet.pdf) opisuje pomiary nacisku i ponowne uzbrojenie PENIRQ przy PD0=0; końcowa komenda nadal je realizuje.

Piny, zegar, mediana, progi, orientacja i zapis NVS są zachowane. GPIO36 nadal jest wejściem bez wewnętrznego podciągania. Nie wymagamy przepinania ani dokładania rezystora na podstawie dotychczasowego wyniku.

## Diagnostyka i odbiór

TOUCH_RAW dodaje `z1`, `z2`, `irq` (stan przed ostatnią serią) i `require_irq`. `end_high` liczy poprawne próbki ADC z HIGH po serii; przy REQUIRE_IRQ=0 nie powoduje odrzucenia. `valid` oznacza próbkę zaakceptowaną według wybranego trybu. Słaby nacisk i zakres nadal mają oddzielne liczniki.

Najpierw bez nacisku: sprawdzić brak przyrostu valid. Potem trzy przytrzymania środka: porównać nacisk, osie, IRQ i liczniki. Poprawne współrzędne przy stałym HIGH wskażą, że bramkowanie IRQ blokowało działający tor ADC; stałe/skrajne odczyty skierują diagnozę do SPI/zasilania/połączeń, bez przesądzania przyczyny. Dopiero poprawny wynik pozwala sprawdzać kalibrację i PAUZA/WZNÓW.

Źródła publikujemy po kompilacji, przed testami/uploadem; wyniki osobnym commitem w STATUS. Poprawa fizyczna pozostaje do potwierdzenia.
