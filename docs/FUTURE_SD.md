# Przyszła funkcja: zapis na karcie SD

Wymaganie użytkownika z **15.11.2026**: zachować możliwość późniejszego dodania zapisu na karcie SD. **Nie jest zaimplementowany w v0.1.12**; obecnie firmware nie montuje karty ani nie konfiguruje jej CS.

## Proponowany zakres

- Rejestrowanie odebranych danych terminala bez modyfikacji bajtów mostu UART/HC-05. Do ustalenia: tylko UART2 RX czy oba kierunki w osobnych plikach lub rekordach z oznaczeniem kierunku.
- Opcjonalny zapis czytelnego tekstu i zrzutu ekranu terminala obok surowego strumienia; sekwencje ANSI zachować w surowym logu.
- Metadane pliku: wersja firmware, baud/format UART, kierunek, czas względny, a po dodaniu zegara także rzeczywisty czas.
- Włączanie/zatrzymywanie zapisu, rotacja plików, informacja o braku karty/pełnym nośniku/błędzie zapisu i kontrolowany flush przed odłączeniem.

## Warunki implementacji

Osobne zadanie zapisu oraz ograniczona kolejka/bufor; wolny lub uszkodzony nośnik nie może blokować komunikacji ani renderera. Zliczać utracone dane po przepełnieniu zamiast zatrzymywać most. Określić odstęp flush i ryzyko utraty ostatniej porcji przy zaniku zasilania. Buforowanie może korzystać z PSRAM, lecz bufory przekazywane do DMA mają spełniać wymagania konkretnego sterownika.

Przewidywany interfejs to **SDSPI + FATFS**, bez zajmowania GPIO16/17 PSRAM. Dla nowego LCD SPI można rozważyć współdzielenie SPI3 (18/23/19) i osobny CS karty. **CS SD nie przydzielono**; wybór ma uwzględnić HC-05, opcjonalne EN/STATE/driver LED, strapy i wyprowadzenia faktycznego modułu. Przed wspólnym MISO sprawdzić Hi-Z wszystkich urządzeń przy nieaktywnym CS. Pinów gniazda SD starego shielda nie utożsamiać z pinami nowego modułu SPI.

Odbiór przyszłej funkcji: równoległy zapis i ruch UART w obu kierunkach, długie pliki, pełna karta, brak/wyjęcie karty, opóźnienia nośnika, reset/zanik zasilania i poprawność plików. To plan, nie wynik wykonanych testów.
