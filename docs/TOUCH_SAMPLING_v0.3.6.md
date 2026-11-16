# Pomiar punktów kalibracyjnych — v0.3.6

## Dowody i ich granice

Użytkownik zgłosił postęp do70%, wielokrotne zerowanie i przejście po odczekaniu bez dotykania. Później potwierdził ukończenie wszystkich pięciu punktów, punktu kontrolnego i GOTOWE. Dostarczony log v0.3.5 kończy się wcześniej: pokazuje przyjęcie CAL po2021 ms, punktów1/2, następnie próby punktu3 (295,215). Surowy załącznik pozostaje lokalnie w `.pio/audit/v0305_user_calibration.txt`.

Między56399 a64437 ms naliczono324 odpytywania i324 IRQ LOW:206 poprawnych,105 odrzuconych z powodu nacisku/Z1 i13 z powodu zakresu osi. Bez błędów API SPI. To40,3 odczytu/s i36,4% odrzuceń przy ciągłym LOW w próbkach IRQ. LOW nie gwarantuje poprawnych współrzędnych. Odrzucenia ADC nadal wymagają odrębnej diagnozy; nie traktujemy tolerancji w UI jako naprawy odczytu elektrycznego.

W v0.3.5 filtr zerował zbieranie, gdy od ostatniej poprawnej próbki minęło co najmniej50 ms lub wystąpiły trzy kolejne braki. Przy25 ms na odczyt ciąg poprawna/brak/poprawna oznacza50 ms między poprawnymi — reset na trzecim odczycie. Dotychczasowe testy20 ms nie ujawniały tego problemu. Diagnostyczna pełna kalibracja przy25 ms z co trzecim odrzuconym odczytem nie przechodzi w opublikowanym kodzie v0.3.5 (`writes=0`, oczekiwano1); wyniki zachowane lokalnie w `.pio/audit/v0305_reproduce_25ms_result.txt`.

70% odpowiada m.in. sześciu z ośmiu próbek (75% zaokrąglone w dół do dziesiątek). Migawki co2 s nie pokazują kolejności wszystkich odrzuceń ani powodów każdego resetu, więc nie przypisujemy wszystkich przypadków wyłącznie przerwie. Przekroczenie rozrzutu120 jednostek ADC również zerowało pomiar.

## Czy czasy się sumowały?

Czas minimalny był mierzony od początku bieżącego stabilnego zbioru. Krótkie tolerowane braki nie zerowały tego czasu, ale nie dodawały współrzędnych ani liczby poprawnych próbek. Dłuższa przerwa lub odległe dotknięcie rozpoczynały nowy zbiór i zerowały sumy. Krótkie dotknięcie innego miejsca mogło przerwać postęp, lecz nie pozostawiało rosnącego czasu ani sum przenoszonych między punktami. Puszczenie po przyjęciu punktu jest wymagane osobno.

## Zmiana

CALIBRATE/VERIFY zachowuje pomiar przez brak krótszy niż80 ms od ostatniej poprawnej próbki, zgodnie z już używaną definicją kontaktu. Limit działa zarówno przy odrzuconym odczycie, jak i przy pierwszym poprawnym po przerwie w odpytywaniu. Usunięto niezależny licznik trzech braków — przy różnym tempie odpytywania oznaczał różne czasy. Odstęp co najmniej80 ms resetuje pomiar. Krótka przerwa <80 ms jest traktowana jako ten sam kontakt; dwa fizyczne stuknięcia w tym samym miejscu rozdzielone krótszą przerwą nie są pewnie rozróżnialne z samych odrzuconych próbek ADC.

Ruch poza dotychczasowe okno120 jednostek ADC resetuje sumy/zakres/czas. Nadal wymagane co najmniej osiem rzeczywistych poprawnych próbek i100 ms od początku zbioru. Po przyjęciu punktu obowiązuje osobna bramka puszczenia: minimum trzy odrzucenia oraz80 ms bez poprawnego kontaktu. Dopasowanie pięciu punktów, niezależna walidacja, format NVS i istniejąca mapa pozostają zgodne. Błąd pojedynczego krótkiego dotknięcia może opóźnić przyjęcie, ale stabilne przytrzymanie rozpoczyna nowy pomiar.

Nowy TOUCH_CAL podaje point/verify/accepted, samples/peak, gap_resets/motion_resets oraz last_gap_ms. Liczniki są sumowane dla bieżącego punktu; raport resetu maksymalnie raz na sekundę, raport przyjęcia zawsze. Pozwala odróżnić zaniki pomiaru od ruchu/rozrzutu bez logowania każdej transakcji.

## Kontrole

Pełne procedury testowe:25 ms i30 ms z co trzecim odczytem brakującym;25 ms z dwoma kolejnymi brakami (75 ms między poprawnymi). Osobna regresja pełnych puszczeń, długiej przerwy bez odpytywania, odległego błędnego dotknięcia i siedmiu próbek — stare sumy nie mogą zaliczyć punktu. Dotychczasowe sprawdzenia gestów, weryfikacji, NVS i granic renderera pozostają.

Kolejność: build internal/touch → commit/push źródeł → regresje i analiza → ewentualny upload z odczytem zachowanej mapy → osobny zapis wyników. Pomyślna kompilacja/testy hosta nie potwierdzają wygody fizycznej kalibracji. Nie kasujemy zapisanej kalibracji ani nie rozpoczynamy jej automatycznie po aktualizacji.

## Wynik po publikacji

Źródła `dd02529` opublikowano po obu udanych buildach, przed regresjami. [Kontrole i start](logs/2026-11-16_v0.3.6_validation.json): testy kalibracji/UI PASS, Cppcheck obu profili bez zgłoszeń. Wgrano touch na COM5; zapis startu uruchomiony przed resetem potwierdził v0.3.6, odtworzenie mapy i15 s bez restartów. COM5 zamknięty. Nie wykonywano ponownej fizycznej kalibracji ani nowego testu UART.
