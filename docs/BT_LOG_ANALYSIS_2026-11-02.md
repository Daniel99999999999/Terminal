# Analiza logu v0.1.6 — Windows / Galaxy A55, 02.11.2026

## Materiał i obserwacje użytkownika

[Pełny log UART](logs/2026-11-02_v0.1.6_windows_pairing.txt) zachowano bez zmian z załącznika użytkownika. Telefon: Samsung Galaxy A55 5G, SM-A556B/DS, Android 16. ESP32 jest niewidoczne w **systemowym** ekranie Bluetooth telefonu. Windows widzi urządzenie, ale zgłasza brak odpowiedzi **podczas dodawania/parowania**, a nie przy otwieraniu portu COM.

Nie wykonano zdalnie nowego parowania ani żadnych zmian sprzętowych. Rozdzielamy raport użytkownika o interfejsach Windows/Android od zdarzeń widocznych w logu ESP32.

## Potwierdzone przez log

| Czas od startu | Zdarzenie | Wniosek |
|---|---|---|
| 1,405 s | DEBUG_LED GPIO2 active=1, reset_reason=1, latched=0 | inicjalizacja programowa LED; nie potwierdza fizycznego świecenia |
| 1,415 s | v0.1.6 / SDK 6.0.1 / echo=1 | użytkownik uruchomił nowy firmware na ESP32 |
| 1,455 s | UART2 TX25 RX26, 115200 8N1 | sterownik uruchomiony; nadal brak testu przesyłania bajtów |
| 2,145 s | lokalny adres `7C:87:CE:2F:DE:DA` | właściwy adres tego ESP32; nie wcześniejszy adres innego urządzenia |
| 2,195 s | GET_DEV_NAME status=0, poprawna nazwa; SPP START status=0, scn=1 | nazwa odczytana ze stosu i serwer wystartował |
| 96,115 s | GAP ACL connect status=0, handle=128 | zestawiono podstawowe łącze Classic; nie jest to jeszcze zakończone parowanie ani SPP |
| 126,205 s | HCI disconnect rsn=0x22; GAP reason=290 | to samo łącze zerwano po ok. **30,09 s** |
| 131,105 s | GAP ACL connect status=0, handle=129 | ponownie zestawiono podstawowe łącze |
| 141,455 s | koniec fragmentu | wynik drugiej próby nie jest pokazany; obserwacja po connect trwa tylko ok. 10,35 s |

W całym dostarczonym fragmencie brak `GAP AUTH`, PIN_REQ, CFM_REQ, KEY_REQ oraz `SPP SRV_OPEN`. DATA_IND/WRITE/CONG i recovery pozostają 0. Nie ma zgłoszenia heartbeat stale, panic ani restartu po inicjalizacji. Raporty IO pojawiają się co 5 s. Pula PSRAM 4 MB jest dodana do heapu; log nie pokazuje nowego testu transmisji UART ani końcowego echa BT.

**Wniosek o RF:** udane zestawienie ACL jest mocniejszym dowodem niż samo `SPP listening` i wskazuje na przynajmniej chwilową dwukierunkową wymianę radiową. Nie potwierdza stabilnego zasięgu ani sprawności całego toru RF. Nie ma podstaw ani do uznania radia za całkowicie martwe, ani do uznania SPP za działające.

## Znaczenie 0x22 i 290

Bluetooth SIG określa `0x22` jako **LMP Response Timeout / LL Response Timeout**. Tutaj używany jest Classic BR/EDR, więc chodzi o timeout transakcji **LMP**, nie BLE LL. Kod nie wskazuje samodzielnie, która wymiana utknęła ani jaka jest fizyczna przyczyna. [Definicja Bluetooth SIG](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/architecture%2C-mixing%2C-and-conventions/controller-error-codes.html).

Lokalne źródła ESP-IDF 6.0.1 potwierdzają przeliczenie: `ESP_BT_STATUS_BASE_FOR_HCI_ERR = 0x100`, a `btc_hci_to_esp_status()` mapuje powód z HCI na status GAP. **290 = 0x122 = 0x100 + 0x22**. To dwa zapisy tego samego błędu, nie drugi błąd aplikacji. Sprawdzono `esp_bt_defs.h`, `hcidefs.h` i `btc_dm.c` w zainstalowanym pakiecie.

Nie należy interpretować tego jako potwierdzonego złego PIN-u, odrzucenia przez callback aplikacji, błędu UART ani timeoutu `esp_spp_write()`. Ten ostatni wynosi 3 s i nie miał tu okazji wystąpić: nie otwarto sesji SPP i nie było żadnego WRITE.

Brak CFM_REQ sam nie oznacza, że nie rozpoczęto parowania. W lokalnym Bluedroid Just Works jest automatycznie potwierdzane wewnątrz stosu bez callbacku CFM aplikacji. Z tego logu wiadomo, że **nie pokazano ukończenia uwierzytelniania**, nie zaś dokładnego momentu rozpoczęcia procedury.

## Co nadal jest hipotezą

- W chwili analizy pierwszego logu pozostawała hipoteza niedokończonego łącza Windows podczas skanu telefonu. **Późniejszy test użytkownika:** Windows BT wyłączony, reset ESP32, odległość 30–50 cm — Samsung nadal nie widzi ESP32. Niedokończone łącze PC nie wyjaśnia tej izolowanej próby.
- Historia parowania / zapisane klucze, negocjacja cech kontrolera, oszczędzanie energii, zachowanie sterownika Windows i jakość łącza radiowego pozostają możliwymi obszarami diagnozy. Log nie wybiera jednej z tych przyczyn.
- Niewidoczność w systemowym ekranie Samsunga nie jest tym samym co brak uprawnień aplikacji terminala. W pierwszej kolejności nie diagnozujemy uprawnień nieużywanej aplikacji.
- Domyślna klasa urządzenia Bluedroid to unclassified; wpływ CoD/EIR na filtrowanie Samsunga można sprawdzić później jako osobny wariant. Nie jest to ustalona przyczyna timeoutu LMP w Windows.
- Sam Android 16 nie stanowi dowodu niezgodności ze starym ESP32. W bieżącym SDK domyślnie `ssp_en=true`, `sc_en=false`. Nie należy włączać `sc_en` w ciemno: lokalny kod ESP-IDF odrzuca Secure Connections na klasycznym ESP32 przy wyłączonym `SC_MODE_INCLUDED`.

## Najbliższy test — bez zmiany firmware

Test izolowanego skanu opisany w punktach 1–3 został następnie wykonany przez użytkownika z wynikiem **nadal niewidoczny**. Kolejny wariant porównawczy CoD/EIR jest opisany w [BT_DISCOVERY_TEST_v0.1.7.md](BT_DISCOVERY_TEST_v0.1.7.md).

1. Wyłączyć Bluetooth **w Windows**, pozostawiając USB/monitor UART.
2. Zresetować ESP32 i poczekać na START/nazwę. Umieścić telefon ok. 30–50 cm od anteny, bez zasłaniania anteny metalem.
3. Skanować w systemowych ustawieniach Bluetooth Samsunga. Zapisać wynik i log UART od resetu przez cały skan. Windows pozostawić wyłączony przez ten test.
4. Następnie osobno sprawdzić Windows: usunąć wyłącznie zapis tego ESP32, jeżeli istnieje, zresetować ESP32 i wykonać pojedynczą próbę dodania urządzenia. Zebrać log co najmniej **45 s od ACL connect**, aby obejmował timeout. Nie usuwać wszystkich urządzeń ani całej NVS.

Kryteria: samo ACL nie wystarcza. Szukamy zakończenia AUTH, następnie — po otwarciu serialowego terminala / portu SPP — SRV_OPEN i echa. Samo zakończenie parowania w ustawieniach nie musi otwierać SPP. [Android: różnica między parowaniem i połączeniem RFCOMM](https://developer.android.com/develop/connectivity/bluetooth/find-bluetooth-devices).

Jeżeli izolowany test nadal daje ten sam wynik, następny krok to diagnostyczny ślad HCI/BTM lub porównanie z referencyjnym akceptorem SPP na tym samym SDK i sprzęcie. Zmieniać jeden parametr na próbę. Pełny ślad HCI może zawierać klucze parowania; przed udostępnieniem trzeba wykluczyć dane kluczy. Nie jest potrzebna ingerencja lutownicza w RF ani zmiana pinów LCD/UART.
