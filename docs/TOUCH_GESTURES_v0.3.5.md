# CAL i rozpoznawanie puszczenia — v0.3.5

## Wykryty błąd

Użytkownik zgłosił brak wejścia w CAL po około4 s. v0.3.4 przyjmowała356 próbek przy620 odpytywaniach LOW podczas wcześniejszych trzech naciśnięć, lecz odrzucenia wciąż występowały. `touch_ui_update` zerowała s_holding i s_down po każdej próbce pressed=false. To błąd gestu: pojedynczy brak pomiaru przerywał dwusekundowy czas i pozwalał ponownie wywołać PAUZA/CR przy tym samym kontakcie. Nie mamy jeszcze współrzędnych trafienia z nieudanej próby; mapa lub trafienie mogą być dodatkowym problemem.

## Zmiana

W TEST/TERMINAL krótszy niż80 ms brak poprawnego pomiaru nie tworzy nowego naciśnięcia ani puszczenia. Po80 ms gest jest kończony, także gdy następne wywołanie jest poprawną próbką po długiej przerwie w odpytywaniu. Nie dopisujemy brakujących współrzędnych do dopasowania kalibracji. Filtr CALIBRATE/VERIFY z ośmioma pomiarami/100 ms i własnym limitem50 ms pozostaje.

CAL trzeba rozpocząć nowym dotknięciem prawej części rzeczywistego dolnego paska8 px. Dopiero po uzbrojeniu dopuszczamy ruch do16 px ku wnętrzu ekranu i8 px w lewo. Większe przeciągnięcie anuluje gest; powrót bez puszczenia nie uzbraja go ponownie. Rozmiar terminala40×29 i wygląd pozostałych przycisków nie zmieniają się.

Pasek pokazuje CAL10%,20% itd. po kolejnych200 ms; po2 s otwiera kalibrację. Log UI press podaje surowe i przeliczone współrzędne. CAL hold started/cancelled/accepted rozróżniają trafienie i przebieg gestu. Logi powstają przy zdarzeniach, nie przy każdym odczycie ADC. Zapis NVS nie jest kasowany ani modyfikowany przez samo wejście w CAL.

## Weryfikacja

Regresje sprawdzają PAUZA bez podwójnej akcji po brakującej próbce, rzeczywisty ciąg ponad2 s z co trzecią próbką brakującą i ruchem wewnątrz histerezy, widoczny postęp, oddzielne1,2-sekundowe przytrzymania, przerwę w odpytywaniu i anulowanie przeciągnięcia. Poprzedni test długiego przytrzymania miał skok zegara2100 ms bez odpytywania; zastąpiono to rzeczywistym ciągiem oraz osobnym testem ujemnym takiej przerwy.

Kolejność: kompilacja obu profili → commit/push → test UI i analiza → upload/test fizyczny → osobny commit wyników. Na sprzęcie trzeba potwierdzić CAL, pięć punktów, walidację, GOTOWE i trafienia; samo poprawienie kodu nie potwierdza dokładności mapy. Odczyt SPI pozostaje w krótkich ramkach v0.3.4, parametry i GPIO nie zmienione.
