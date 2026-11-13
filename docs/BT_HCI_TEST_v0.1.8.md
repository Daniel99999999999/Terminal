# Test HCI Scan Enable — v0.1.8

## Cel i zakres

Próby v0.1.7 z Computer CoD oraz jawnym EIR nie poprawiły wykrywania na Samsungu. Sprawdzamy teraz, czy kontroler otrzymuje `Write Scan Enable` (opcode **0x0C1A**) z wartością **0x03** i zwraca Command Complete **status=0**. Bity 0x01/0x02 włączają odpowiednio Inquiry Scan / Page Scan, czyli nasłuch zapytań wykrywania i żądań połączenia. To nadal nie jest potwierdzenie odbioru przez telefon ani sprawności anteny.

Wersja **0.1.8**, `identity_test=2`, `hci_scan_diag=1`. Pozostawiono nazwę, CoD/EIR, SSP Just Works, ERTM, moc RF, UART, LED oraz konfigurację SDK z ostatniej próby. Nie kasujemy NVS/bondów ani nie zmieniamy pinów.

## Jak działa obserwacja

- `bt_hci_diag.c` korzysta z linkerowego `--wrap` dla dwóch publicznych funkcji VHCI. Obserwuje istniejące wywołania Bluedroid i przekazuje oryginalne pakiety, callbacki oraz wyniki do dotychczasowej obsługi. Nie uruchamia drugiego hosta i nie wstrzykuje własnych poleceń HCI.
- Filtr `bt_hci_scan.c` kopiuje tylko opcode, status oraz wartość Scan_Enable dla RESET / READ_SCAN_ENABLE / WRITE_SCAN_ENABLE. Nie kopiuje całych pakietów, kluczy, PIN-ów, adresów ani danych UART/SPP.
- Callback wykonuje ograniczony parser i `xQueueSend(..., 0)` do kolejki 32 elementów. Drukowanie odbywa się z istniejącego zadania UART RX co 5 s; znaczniki czasu konsoli dotyczą **wydruku**, nie momentu przejścia pakietu. Nie dodano nowego zadania aplikacji. Krótkie sekcje krytyczne chronią wyłącznie liczniki, bez wywołań API BT/kolejek w sekcji.
- Odpowiedzi WRITE są dopasowane FIFO do żądań (maks. 16 w toku). RESET, błąd, brak żądania lub utrata śladu unieważniają potwierdzenie. Zgubione zdarzenie daje `drops>0` i `trace_complete=0`; stan pozostaje niepotwierdzony do resetu aplikacji.
- Obsługujemy pasywnie również READ_SCAN_ENABLE (0x0C19), ale **nie wysyłamy odczytu** przez dodatkowego nadawcę. Brak takiej linii jest spodziewany. Wynik tego testu opiera się na odpowiedzi kontrolera na WRITE, nie na niezależnym odczycie konfiguracji.

Obserwator dodaje niewielką pracę w ścieżce HCI; jego wpływ czasowy na prawdziwym kontrolerze wymaga próby. Testy hostowe nie modelują czasu FreeRTOS ani RF.

## Test użytkownika

1. Zakończyć monitor COM10. W VS Code uruchomić **PIO: Upload**, następnie **PIO: Monitor (115200)**. Resetować ESP32 dopiero przy uruchomionym monitorze, aby zebrać cały boot.
2. Potwierdzić **v0.1.8 / identity_test=2 / hci_scan_diag=1**. Poczekać przynajmniej 7 s na pierwszy wydruk `HCI_SCAN`. Przykład oczekiwanego fragmentu:

   ```text
   HCI_SCAN: TX opcode=0x0C1A WRITE_SCAN_ENABLE value=0x03
   HCI_SCAN: RX COMMAND_COMPLETE opcode=0x0C1A status=0x00 requested=0x03
   HCI_SCAN: observer attached=1 drops=0 trace_complete=1 pending=0 confirmed=1 scan=0x03 (last observation; RF unverified)
   ```

   Wcześniejsze zapisy 0x00/0x01/0x02 podczas startu mogą być normalne. Oceniamy **ostatni potwierdzony zapis** oraz kolejne podsumowania. Sam TX z 0x03, bez odpowiedzi kontrolera, nie zalicza testu.

3. Przy wyłączonym BT Windows wykonać świeży skan w systemowym ekranie Galaxy A55. Zbierać log od resetu przez około **60 s** i podać wynik widoczności, ewentualnego parowania oraz nazwę/adres widziane przez telefon. Nie wystarczy wkleić samych liczników IO.
4. Windows testować oddzielnie, z wyłączonym BT telefonu. Jeśli podjęto próbę parowania, logować min. 45 s po ACL connect; wcześniejszy timeout LMP Windows pozostaje nierozwiązany.

## Interpretacja

| Wynik | Wniosek / dalszy krok |
|---|---|
| attached=1, drops=0, trace_complete=1, pending=0, confirmed=1, scan=0x03; RX WRITE status=0 | Kontroler potwierdził włączenie obu rodzajów nasłuchu w obserwowanym momencie. Jeśli Samsung nadal nie widzi ESP32, porównać z referencyjnym akceptorem SPP / odbiornikiem zewnętrznym i rozważyć diagnostykę RF popartą pomiarami. |
| Ostatni potwierdzony scan=0x00/0x01/0x02 | Nie są włączone oba rodzaje nasłuchu. Sprawdzić kolejność zapisów i ostatniego nadawcę ustawienia w stosie. |
| RX status != 0 | Zachować kod błędu i pełny fragment; kontroler nie potwierdził żądanego zapisu. |
| confirmed=0 lub pending != 0 przez kolejne wydruki | Brak wystarczającego potwierdzenia; sprawdzić brakujące odpowiedzi i reset kontrolera. Pole scan może zawierać starą wartość — nie traktować jej jako bieżącej. |
| attached=0, drops>0 lub trace_complete=0 | Ślad nie pozwala potwierdzić stanu; najpierw diagnozować obserwator/utracone zdarzenia. Nie wnioskować o RF. |

## Kontrole i powrót

Pełny build: **SUCCESS**, ESP-IDF 6.0.1 / espressif32 7.0.1, Flash 695 164 B, statyczny RAM 46 076 B. Dodatkowa kolejka zajmuje również heap w czasie działania; nie zmierzono jeszcze zużycia na płytce. Testy: 16 BT + 10 LED + 13 HCI PASS, testy przekazywania callbacków/pakietów adaptera w wariantach 1 i 0 PASS, Cppcheck dla 7 plików C bez zgłoszeń w sprawdzanych kategoriach. Mapa końcowego linkowania potwierdza odwołania Bluedroid do wrapperów oraz wrapperów do rzeczywistych funkcji kontrolera.

`BT_HCI_SCAN_DIAG=0` w `src/bt_hci_diag.h` wyłącza kolejkę i obserwację; wrappery tylko przekazują wywołania. Ten wariant sprawdzono hostowo i kompilatorem Xtensa (`-fsyntax-only`), bez osobnego pełnego linkowania. Dokładna kopia v0.1.7 / tryb 2 wraz z konfiguracją jest w [baseline/v0.1.7](../baseline/v0.1.7/README.md); v0.1.4 i v0.1.6 także pozostają zachowane. Nie wykonano uploadu ani testu radiowego v0.1.8 przez agenta.

Źródła: [specyfikacja poleceń HCI](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html) oraz lokalne ESP-IDF 6.0.1: `esp_bt.h`, `esp_bluedroid_hci.c`, `hci_hal_h4.c`, `bt.c`. Publiczny adapter `esp_bluedroid_attach_hci_driver` jest przeznaczony dla host-only i w naszym buildzie z wewnętrznym kontrolerem nie przechwytuje tych wywołań; dlatego zastosowano pasywne wrappery.
