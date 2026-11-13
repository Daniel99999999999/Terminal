# HC-05 w terminalu — decyzja i plan integracji

Użytkownik 03.11.2026 wybrał zewnętrzny HC-05 po zakończeniu ostatniej próby wbudowanego BT. Priorytetem jest teraz TFT. Nie kupiono ani nie podłączono modułu przez agenta. Główny kod v0.1.9 nadal zawiera wewnętrzne SPP; integracja HC-05 nie jest jeszcze zaimplementowana.

## Transport i poziomy

| Interfejs | Rola | Stan pinów |
|---|---|---|
| UART0 przez USB | konsola i programowanie ESP32 | GPIO1/3 zachowane |
| UART2 | urządzenie docelowe terminala | TX25/RX26 zachowane |
| UART1 | połączenie ESP32 ↔ HC-05 | GPIO do ustalenia z LCD |

Połączenie logiczne: ESP32 TX → HC-05 RXD, HC-05 TXD → ESP32 RX, wspólna masa. Sygnały przy ESP32 mają poziomy 3,3 V. Nie zakładać, że napięcie zasilania płytki nośnej HC-05 jest napięciem jej UART. Dokładny wariant modułu/nośnika trzeba zidentyfikować przed doborem zasilania; nie podawać 5 V na niezweryfikowany RXD.

Nie zakładać domyślnego baud, trybu AT ani roli przycisku/KEY dla każdego klona. Najpierw ustalić model, tryb transmisji i jego nastawy UART, potem ustawić UART1 ESP32. Zmiana baud HC-05 to osobna operacja na rzeczywistym module, nie zmiana UART2 urządzenia docelowego.

## Budżet GPIO razem z TFT

Po odjęciu Flash, PSRAM16/17, konsoli1/3, UART2 25/26, LED2 i pozostawieniu GPIO0/12 wolnych, dla sterowania pozostaje 13 wyjść: **4/5/13/14/15/18/19/21/22/23/27/32/33**. GPIO5/15 wymagają oceny stanów strapujących. Pełny LCD 8-bit + WR/RS/CS/RD/RST zużywa wszystkie 13. HC-05 potrzebuje dodatkowego TX ESP32, więc poprzednia tabela LCD nie obejmuje całego urządzenia.

RX HC-05 można zaplanować na wolnym wejściu GPIO34/35/36/39 po potwierdzeniu wyprowadzeń płytki i poziomów; nie rozwiązuje to braku jednego wyjścia TX. STATE, jeśli rzeczywiście dostępne, wymaga kolejnego wejścia. GPIO16/17 nie są kandydatami.

Po identyfikacji LCD i mapowaniu dotyku wybierzemy sposób zwolnienia jednego wyjścia, np. stały nieaktywny RD w docelowym trybie zapisu po odczycie ID albo osobny układ resetu LCD. To możliwości do oceny po pomiarach, nie instrukcja obecnego łączenia pinów. Zachować niezależne CS, jeśli jest potrzebne do bezpiecznego pomiaru dotyku. Karta SD nie jest objęta tym budżetem.

## Następny patch i kryteria

UART1 obsłuży kolejkę zdarzeń oraz kontrolowane bufory i timeouty, podobnie jak obecny UART2. Dopiero po potwierdzeniu HC-05 i jego nastaw wydzielić mały transport UART do modułu, zachowując bazy starego SPP. Samo przełączenie makra nie uruchamia HC-05. Nadzór i liczniki błędów/odrzuceń mają pozostać.

Wskazanie LED połączenia można oprzeć na zmierzonym sygnale STATE HC-05, jeżeli jego działanie jest odpowiednie. Brak bajtów na UART nie oznacza rozłączenia. Bez STATE lub innego potwierdzonego sygnału nie udawać wiarygodnej informacji o połączeniu BT na podstawie ruchu UART.

Odbiór: Android wykrywa i paruje HC-05, telefon ↔ UART2 przesyła krótkie i długie bloki w obu kierunkach, równolegle działa LCD. Osobno potwierdzić LED oraz zachowanie buforów przy rozłączeniu i przeciążeniu. Nie zakładać skuteczności HC-05 przed testem rzeczywistego egzemplarza.
