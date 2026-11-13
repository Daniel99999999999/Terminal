# HC-05 w terminalu — decyzja i plan integracji

**Bieżąca implementacja v0.1.11:** [instrukcja i konfiguracja](HC05_TFT_v0.1.11.md). Most UART1↔UART2 oraz opcjonalne STATE/EN/AT są dodane. UART1: **115200**, potwierdzone przez użytkownika. Natywne BT/Wi-Fi usunięto z aktywnego kodu. TFT/STATE/EN domyślnie wyłączone; nadal wymagany test sprzętowy. Poniższy plan opisuje stan historyczny sprzed implementacji; aktualne kryteria i wyniki są w instrukcji i STATUS.

Użytkownik 03.11.2026 wybrał zewnętrzny HC-05 po zakończeniu ostatniej próby wbudowanego BT. 13.11.2026 zlecił przydział pinów: najpierw RXD/TXD/GND/VCC, później EN/KEY i STATE; TFT na razie bez dotyku. [Aktualny przydział ESP32 i warunki elektryczne](PIN_ASSIGNMENT_HC05_TFT.md). Główny kod v0.1.10 nadal zawiera wewnętrzne SPP; integracja HC-05 nie jest jeszcze zaimplementowana i nie potwierdzono fizycznego połączenia.

**Użytkownik potwierdził ZS-040 i sprawdził wejście AT przez EN.** Plan zasilania: 5 V na VCC nośnika z regulatorem; UART i EN pozostają sygnałami 3,3 V. Nie potwierdzono pomiarem STATE ani połączenia z ESP32. Funkcji EN w tym egzemplarzu nie trzeba ponownie ustalać na podstawie innych rewizji.

## Transport i poziomy

| Interfejs | Rola | Stan pinów |
|---|---|---|
| UART0 przez USB | konsola i programowanie ESP32 | GPIO1/3 zachowane |
| UART2 | urządzenie docelowe terminala | TX25/RX26 zachowane |
| UART1 | połączenie ESP32 ↔ HC-05 | plan: TX32 → RXD; RX34 ← TXD |
| HC-05 STATE | informacja o aktywnym połączeniu | rezerwacja GPIO35, wejście |
| HC-05 EN / KEY | konfiguracja / AT potwierdzone przez użytkownika | warunkowo GPIO12; teraz nie podłączać |

Połączenie logiczne: ESP32 TX → HC-05 RXD, HC-05 TXD → ESP32 RX, wspólna masa. Sygnały przy ESP32 mają poziomy 3,3 V; 5 V dotyczy tylko VCC wskazanego nośnika z regulatorem. Zasilanie nośnika nie jest napięciem UART/EN/STATE. [Opis zasilania i układu ZS-040](https://www.martyncurrey.com/hc-05-and-hc-06-zs-040-bluetooth-modules-first-look/).

Nie zakładać domyślnego baud, trybu AT ani roli przycisku/KEY dla każdego klona. Najpierw ustalić model, tryb transmisji i jego nastawy UART, potem ustawić UART1 ESP32. Zmiana baud HC-05 to osobna operacja na rzeczywistym module, nie zmiana UART2 urządzenia docelowego.

## Budżet GPIO razem z TFT

Nowy plan bez dotyku korzysta z 14 wyjść dla LCD i TX UART1, w tym strapów GPIO0/5/15; GPIO12 jest ostatnią, warunkową rezerwacją EN/KEY. RX=34 i STATE=35 nie zużywają wyjść. Nie używamy PSRAM16/17, Flash6–11, konsoli1/3, UART2 25/26 ani debug LED na GPIO2 do innych zadań. Pełny [bilans, tabela LCD i wymagania startowe](PIN_ASSIGNMENT_HC05_TFT.md) zastępują historyczną tabelę samego LCD.

GPIO12 nie wolno łączyć bez sprawdzenia obciążenia EN/przycisku HC-05: stan przy resecie wpływa na VDD_SDIO. Zachować fabryczny strap i w razie potrzeby zastosować bufor. Użytkownik potwierdził AT przez EN; nadal trzeba dobrać sekwencję sterowania z ESP32, ewentualnie cykl zasilania dla pełnego AT. Plan nie ma dodatkowego wyjścia do odcinania zasilania. Jeśli GPIO12 okaże się niedostępne, potrzeba zmiany sprzętowej/przydziału.

W tym wariancie LCD zachowuje osobne RD, CS i RST; dotyk i SD nie mają przydziału. GPIO0 użyte dla RD wymaga nieaktywnego CS także podczas BOOT/uploadu. Powrót do dotyku wymaga nowego planu po pomiarach, nie ponownego użycia dawnych rezerw ADC.

## Następny patch i kryteria

UART1 obsłuży kolejkę zdarzeń oraz kontrolowane bufory i timeouty, podobnie jak obecny UART2. Dopiero po potwierdzeniu HC-05 i jego nastaw wydzielić mały transport UART do modułu, zachowując bazy starego SPP. Samo przełączenie makra nie uruchamia HC-05. Nadzór i liczniki błędów/odrzuceń mają pozostać.

Wskazanie LED połączenia można oprzeć na zmierzonym sygnale STATE HC-05, jeżeli jego działanie jest odpowiednie. Brak bajtów na UART nie oznacza rozłączenia. Bez STATE lub innego potwierdzonego sygnału nie udawać wiarygodnej informacji o połączeniu BT na podstawie ruchu UART.

Odbiór: Android wykrywa i paruje HC-05, telefon ↔ UART2 przesyła krótkie i długie bloki w obu kierunkach, równolegle działa LCD. Osobno potwierdzić LED oraz zachowanie buforów przy rozłączeniu i przeciążeniu. Nie zakładać skuteczności HC-05 przed testem rzeczywistego egzemplarza.
