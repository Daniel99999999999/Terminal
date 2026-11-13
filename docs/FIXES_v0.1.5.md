# Naprawy P1/P2 — firmware v0.1.5

## Zakres

Zmiana obejmuje pięć ustaleń A1–A5 z [audytu v0.1.4](AUDIT_2026-11-01.md). Zachowano C, ESP-IDF, trzy zadania komunikacyjne, konfigurację PSRAM, tryb testowego echa oraz UART2 TX GPIO25 / RX GPIO26. LCD, dotyk i tor RF pozostają bez zmian. Pełna kopia źródła i konfiguracji bazy jest w [baseline/v0.1.4](../baseline/v0.1.4/README.md).

| Ustalenie | Naprawa | Weryfikacja bez sprzętu |
|---|---|---|
| A1 / P1: UART czeka na 128 B | Kolejka zdarzeń UART; `uart_read_bytes(..., 128, 0)` odczytuje dostępne dane. Ograniczona liczba porcji i kontrolne oczekiwanie 100 ms zabezpieczają utratę zdarzenia DATA. | Kompilacja ESP32; test długości wiadomości wymaga UART i odbiornika BT. |
| A2 / P1: zagubione zakończenie zapisu SPP | Trwały stan zapisu zamiast semafora WRITE/CONG. Jeden zapis naraz; błędy z uchwytem 0 trafiają do oczekującego zapisu. Timeout 3 s i restart profilu przez UNINIT/INIT/START. | Testy produkcyjnego modułu stanu: błąd 0, CLOSE po rezerwacji, kolejność CONG/WRITE, odzyskanie po resecie. |
| A3 / P2: dane poprzedniej sesji w nowej | Każdy pakiet ma parę uchwyt + 64-bitowa generacja; sprawdzenie przed rezerwacją zapisu. Zamknięcie po rezerwacji blokuje nowych klientów do zakończenia restartu SPP. | Odrzucanie pakietu pobranego przed reconnect, także po ponownym użyciu tego samego uchwytu. |
| A4 / P2: brak diagnostyki BT | Wersja SDK, własny adres BT, zwroty name/scan/get_name/start/security, odczyt nazwy, statusy SPP i GAP/ACL. Komunikat o zleceniu inicjalizacji zamiast przedwczesnego `Ready`. | Kompilacja z lokalnymi nagłówkami 6.0.1; rzeczywiste statusy i RF wymagają logu urządzenia. |
| A5 / P2: brak diagnostyki UART | Obsługa FIFO_OVF, BUFFER_FULL, FRAME_ERR, PARITY_ERR, błędów odczytu i liczników odrzuceń. Zbiorczy raport IO co 5 s. | Kompilacja; wywołanie rzeczywistych błędów wymaga urządzenia. |

`src/bt_tx_state.c/.h` to mały moduł logiki bez zależności od ESP-IDF. Adapter w `main.c` chroni jego stan przez `portMUX`; nie wywołuje API UART/BT, kolejek ani logowania pod tą blokadą. Powiadomienia zadania jedynie budzą nadajnik — ich utrata lub scalenie nie zmienia stanu zakończenia zapisu.

## Rozłączenie, błędy i congestion

- `CONG=false` samo nie kończy zapisu: trzeba również odebrać poprawny WRITE o oczekiwanej długości. WRITE z congestion wymaga późniejszego zwolnienia congestion.
- CLOSE bez zapisu w toku pozwala normalnie przyjąć kolejną sesję. CLOSE podczas zarezerwowanego zapisu, błąd lub timeout blokują nadawanie i nowych klientów, po czym zadanie TX restartuje **profil SPP**, pozostawiając Bluedroid/kontroler, NVS i UART.
- Kolejny klient może połączyć się po udanym `SPP START`. Android może wymagać ręcznego ponowienia połączenia po takim błędzie.
- Każda faza restartu ma timeout 5 s. Brak potwierdzenia pozostawia TX wyłączony i daje log błędu; zadanie ponawia próbę, a UART i raportowanie nadal pracują. Nie wysyła następnego pakietu przez niepewny profil i nie resetuje automatycznie całego ESP32.
- Callbacki INIT/START są sprawdzane względem fazy cyklu życia; spóźniony START podczas zatrzymywania nie włącza nadajnika.
- Wiadomość powitalna jest wysyłana tylko w trybie echa. `SPP_TEST_ECHO=0` nie dopisuje bannera do strumienia mostu.

## Granice sesji UART i liczniki

Zadanie RX opróżnia UART także bez telefonu, odrzucając dane offline. Przy zmianie generacji wykonuje **jednorazowe** `uart_flush_input()`, aby stare bajty nie weszły do nowej sesji. Callback budzi je markerem w kolejce zdarzeń. Nie jest to rutynowe czyszczenie przy przepełnieniu; kolejka zdarzeń błędów nie jest resetowana.

To świadomy kompromis bez sterowania przepływem: bajty nadane równocześnie ze zmianą sesji mogą zostać odrzucone. Testy stabilnej transmisji należy zaczynać po logu `UART RX session boundary: handle=<aktywny uchwyt>`. Danych już przyjętych przez TX UART przed rozłączeniem nie można wycofać z przewodu.

Raporty `IO` zawierają narastające liczniki 32-bitowe od startu. `boundary_discard` to liczba bajtów widocznych w buforze sterownika przed czyszczeniem, nie dokładny pomiar straty w sprzętowym FIFO. Liczba utraconych bajtów przy FIFO_OVF i samych błędach ramek jest nieznana. Pełna kolejka zdarzeń może również zgubić zdarzenie błędu; liczniki opisują zdarzenia dostarczone aplikacji. `stale` obejmuje pakiety starych sesji i odrzucone podczas restartu. Licznik odrzucenia oczekującego zapisu oznacza niepewne dostarczenie, nie dowód, że odbiornik nie otrzymał żadnego bajtu.

Brak RTS/CTS i mechanizmu ograniczania nadawcy nadal wyklucza gwarancję bezstratności przy dowolnym przeciążeniu. Przy zwykłym przepełnieniu nie są kasowane wszystkie dane RX — odczyt pozwala sterownikowi odzyskać miejsce.

## Parowanie i wykrywalność

Zachowano `IO_CAP_NONE` i SSP Just Works. W lokalnym ESP-IDF 6.0.1 `btc_dm_sp_cfm_req_evt()` potwierdza Just Works wewnątrz stosu bez callbacku CFM aplikacji. Nieobsługiwane żądania PIN, wpisania passkey lub porównania liczby są jawnie odrzucane z logiem; kody PIN/passkey nie są wypisywane.

`ESP_OK` z ustawienia nazwy/scan mode oznacza przyjęcie żądania. W tej wersji GAP nie udostępnia osobnego zdarzenia potwierdzającego własny tryb discoverable. Log inquiry dotyczy wyszukiwania inicjowanego przez ESP32, a MODE_CHG — trybu energetycznego połączenia. Przyczyna niewykrywalności przez Androida pozostaje do sprawdzenia na sprzęcie.

## Testy

```powershell
python tests/run_host_tests.py
```

Runner kompiluje i uruchamia rzeczywisty `src/bt_tx_state.c`, wymaga kompilatora C i zapisuje artefakty w `.pio/host-tests`. Na tym komputerze wykrywa Embarcadero `bcc64x`; inny kompilator można wskazać przez `--cc`. Wykonano 16 scenariuszy, wynik `PASS`; kompilacja testu używa C11 oraz `-Wall -Wextra -Werror`. To test logiki, bez modelowania planisty FreeRTOS, sterownika UART i radia.

Pełne wyniki kompilacji, analizy oraz procedura testów na urządzeniu są w [TESTS.md](TESTS.md) i [STATUS.md](STATUS.md).
