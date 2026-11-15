# Reakcja dotyku i diagnostyka — v0.3.2

W pierwszej fizycznej próbie v0.3.1 użytkownik zgłosił ignorowane naciśnięcia punktów kalibracji, także przytrzymania 0,5–1 s. W przechwyconych fragmentach logu były dwa poprawnie przyjęte punkty, lecz nie było pomiarów powodów odrzucenia pozostałych próbek. Nie ustalono, czy problem pochodzi z IRQ, kontaktu panelu, czasu ustalania ADC czy filtru. Brak odpowiedniej reakcji nie dowodzi braku rezystora ani uszkodzenia panelu.

## Podciąganie IRQ

GPIO36 ESP32 nie ma wewnętrznego pull-up/pull-down; kod ustawia je jako wejście bez podciągania. [Dokumentacja Espressif](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-reference/peripherals/gpio.html).

Osobno, producent XPT2046 opisuje wewnętrzne podciąganie w torze PENIRQ, nominalnie około 50 kΩ. Nie jest to rezystor GPIO ESP32 ani potwierdzenie schematu konkretnego modułu. Rzeczywiste połączenie T_IRQ i ewentualne dodatkowe rezystory trzeba oceniać na module; nie dokładamy rezystora jako niesprawdzonej diagnozy. [Karta XPT2046, s.24–25](https://spinmodules.com/wp-content/uploads/2025/01/XPT2046-datasheet.pdf#page=24).

## Zmiana odczytu

Dotychczas każde z dziesięciu odczytań ADC było osobną transakcją SPI z PD0=0 i powrotem do detekcji PENIRQ. v0.3.2 przesyła jedną 30-bajtową serię przy CS=LOW. Pomiary Z1/Z2 oraz odrzucone i trzy używane pomiary każdej osi mają PD0=1; ostatni pomiar Y ma PD0=0 i przywraca PENIRQ. Nadal jest mediana trzech i odrzucenie pierwszego odczytu po zmianie osi. Próg względnego nacisku 300, granice surowych współrzędnych 50..4045, zegar SPI1 MHz, przypisanie osi/pinów i orientacja pozostają takie same.

Dobór trybu zasilania ADC i końcowego przywrócenia PENIRQ porównano z [implementacją autora biblioteki XPT2046](https://github.com/PaulStoffregen/XPT2046_Touchscreen/blob/master/XPT2046_Touchscreen.cpp) oraz kartą producenta. To ograniczona zmiana sposobu pomiaru, nie potwierdzenie przyczyny zgłoszonego problemu. Sterownik pozostaje w C/ESP-IDF.

Jeden descriptor i wyrównane, dopełnione bufory TX/RX są statyczne w RAM wewnętrznym, bez DMA. Po timeout nie są ponownie używane ani zwalniane. Transfer ma kontrolowany timeout 20 ms. Także pomiar odrzucony jako słaby lub poza zakresem kończy się komendą przywracającą detekcję PENIRQ.

## Zmiana filtru kalibracji

Pojedyncza odrzucona próbka nie zeruje całego zbierania. Filtr toleruje najwyżej dwie kolejne odrzucone próbki, pod warunkiem że od ostatniej poprawnej upłynęło mniej niż 50 ms. Trzy odrzucenia lub przerwa co najmniej 50 ms rozpoczynają zbieranie od nowa. Nadal wymagane jest co najmniej osiem poprawnych próbek i 100 ms od początku stabilnego pomiaru. Nie łączymy krótkich stuknięć rozdzielonych pełnym puszczeniem.

Zachowano okno stabilności surowych osi 120 jednostek, dopasowanie pięciu punktów, niezależny punkt kontrolny, wymagane puszczenie pomiędzy punktami i kontrolę NVS. Ekran pokazuje postęp „POMIAR …% — TRZYMAJ”. Zapisanej kalibracji nie kasujemy; niedokończona kalibracja w RAM po uploadzie zacznie się ponownie.

## Liczniki

Co 2 s log `TOUCH_RAW` podaje liczniki narastające:

| Pole | Znaczenie |
|---|---|
| poll / irq_low | Wszystkie odpytywania i wejścia w odczyt przy IRQ=LOW. |
| valid | Poprawna próbka po filtrze nacisku/zakresu/końcowego IRQ. |
| weak | Z1=0 lub nacisk poniżej progu. |
| bounds | Mediana osi poza dopuszczalnym zakresem. |
| end_high | IRQ wróciło HIGH po odczycie; próbkę potraktowano jako puszczoną. |
| io | Błąd/timeout transportu SPI. |
| raw / z | Ostatnie surowe mediany i wskaźnik nacisku z odczytanej serii. |

Liczniki nie zastępują pomiaru napięcia lub przebiegu IRQ. Pojedyncze odrzucenia przy puszczaniu ekranu są spodziewane; istotne są przyrosty podczas trwającego przytrzymania. Nie uznajemy poprzednich wartości raw z innego momentu za współrzędne aktualnego dotknięcia.

## Weryfikacja

Test protokołu sprawdza pojedynczą transakcję i zakończenie PENIRQ także przy słabej/odrzuconej próbce, medianę i odrzucenie początkowego odczytu, liczniki powodów odrzucenia i zachowanie buforów po timeout. Regresja UI dodaje stabilny pomiar z pojedynczą brakującą próbką (po żadnej stronie nie ma ośmiu kolejnych poprawnych) oraz osobne krótkie stuknięcia rozdzielone puszczeniem. Dotychczasowe kontrole dopasowania/NVS/walidacji pozostają.

Kolejność: kompilacja obu wariantów, publikacja wersji, testy, upload i ponowna próba fizyczna. Wyniki są w STATUS. Ocena poprawy reakcji, ukończenie kalibracji i odtworzenie po resecie wymagają próby na sprzęcie.

## Wynik fizyczny

Wersja wgrana, start stabilny, kalibracja została odtworzona z NVS. Przy trzech zgłoszonych przez użytkownika przytrzymaniach GPIO36 pozostawało odczytywane jako HIGH; irq_low i valid pozostały zerowe. v0.3.2 nie potwierdziła poprawy reakcji. [Pełny zapis wyników](logs/2026-11-15_v0.3.2_validation.json) i [log próby](logs/2026-11-15_v0.3.2_touch_press_watch.txt). Przyczyna stanu IRQ wymaga rozróżnienia od działania ADC; następna zmiana nie może opierać całej diagnostyki na tym wejściu.
