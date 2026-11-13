# Test klasy urządzenia / EIR — v0.1.7

## Dlaczego ten test

Użytkownik potwierdził: **Windows Bluetooth wyłączony, ESP32 zresetowany, Samsung Galaxy A55 SM-A556B/DS / Android 16 skanuje z ok. 30–50 cm, ESP32 nadal niewidoczne**. Niedokończone połączenie PC nie wyjaśnia więc tej obserwacji. Wcześniejszy log Windows nadal wskazuje osobny problem niedokończonego parowania: ACL sukces, po ok. 30 s LMP timeout 0x22, brak AUTH completion i SRV_OPEN.

To test hipotezy o identyfikacji / prezentacji urządzenia podczas wykrywania. **Nie ustalono**, że CoD lub EIR powodują niewidoczność Samsunga, ani że ich zmiana usunie timeout LMP Windows. Udane zestawienie ACL potwierdza tylko część wymiany radiowej; nie dowodzi stabilności całego toru RF.

## Wynik pierwszej próby — tryb 1

Użytkownik dostarczył [pełny log](logs/2026-11-02_v0.1.7_identity1_samsung.txt) z **v0.1.7 / identity_test=1** i potwierdził dalszą niewidoczność na Samsungu. INIT status=0 przy 2266 ms, START status=0 / SCN=1 przy 2346 ms; późniejszy CoD=0x000100, major=1/minor=0 przy 2396 ms. Zmiana klasy faktycznie nastąpiła lokalnie i nie usunęła objawu. Nie ma podstaw do rozpoznania zawieszonej inicjalizacji na podstawie wcześniejszego, uciętego wycinka: końcowa linia `initialization requested` została wypisana już po callbackach.

Automatyczny CONFIG_EIR status=0 zawiera typy 0x09, 0x03, 0x05 i 0x07. Oznacza to przygotowane pola nazwy i list UUID; nie potwierdza zawartości 0x1101 ani odbioru przez telefon. Do 261486 ms brak ACL, AUTH i SRV_OPEN, a liczniki danych/błędów/recovery są zerowe. W tym czasie nie ma resetu, panic ani zgłoszenia heartbeat stale. Brak błędów aplikacji nie rozstrzyga sprawności radia.

**Bieżące źródła wybierają teraz tryb 2.** Zmieniono tylko jedną definicję z 1 na 2; pozostały kod oraz konfiguracja SDK są zachowane. To kolejna próba porównawcza, a nie naprawa potwierdzona na sprzęcie. Wersja aplikacji nadal wynosi 0.1.7; wariant rozróżnia pole `identity_test`.

## Wynik drugiej próby — tryb 2

Użytkownik dostarczył [pełny log trybu 2](logs/2026-11-02_v0.1.7_identity2_samsung.txt) i potwierdził: **nadal niewidoczne na Androidzie**. Wersja 0.1.7 / identity_test=2, INIT status=0 przy 2156 ms, START status=0 przy 2236 ms, CoD Computer=0x000100 przy 2316 ms. Jawne config_eir request=ESP_OK przy 2266 ms; późniejszy CONFIG_EIR status=0 przy 2286 ms i pola 0x09/0x03/0x05/0x07/**0x01**. Pole flags zostało dodane. Do 326456 ms brak ACL/AUTH/SRV_OPEN, błędów/recovery/resetów.

**Oba testy zakończone bez poprawy wykrywania.** Kod ustawia connectable/general discoverable oraz SSP IO_CAP_NONE; w lokalnym SDK Just Works jest akceptowane automatycznie. Nie ma przycisku ani ograniczonego czasowo okna parowania w aplikacji. To konfiguracja i lokalne zdarzenia, a nie potwierdzenie działania Inquiry Scan/Page Scan w kontrolerze ani odpowiedzi odebranej przez telefon. Nie diagnozować uszkodzenia RF na tej podstawie.

**Kolejny etap:** ślad HCI z ostatnim `Write Scan Enable` (0x0C1A), parametrem 0x03 i Command Complete status=0, ewentualnie niezależne `Read Scan Enable` (0x0C19), status=0 / wartość=0x03. Te bity oznaczają Inquiry Scan + Page Scan. Sam podwyższony poziom logów HCI nie gwarantuje wydruku parametrów/statusu; zwykłe logi w lokalnym SDK wypisują część opcode/eventów bez pełnej zawartości. Trzeba wybrać ślad zawierający wymagane pola. Nie rejestrować niepotrzebnych kluczy/payloadów ani nie przejmować VHCI przez drugiego hosta równolegle z Bluedroid. [Specyfikacja poleceń HCI](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host-controller-interface/host-controller-interface-functional-specification.html).

W ramach analizy drugiego logu nie zmieniono firmware ani SDK, nie wykonano nowej kompilacji/uploadu. Dalszą alternatywą porównawczą pozostaje referencyjny akceptor SPP na tym samym SDK i sprzęcie.

## Trzy tryby porównawcze

W `src/bt_discovery_diag.h` jest jedna definicja:

```c
#define BT_DISCOVERY_TEST 2
```

| Wartość | Zachowanie |
|---|---|
| `0` | Tylko logowanie: dotychczasowa klasa i automatyczne EIR stosu. |
| `1` — pierwsza próba wykonana | Zmienia tylko major/minor CoD na Computer / uncategorized, po SPP START. Zachowuje bity usług wybrane przez profile. |
| **`2` — obecnie domyślnie** | To samo co 1, dodatkowo jawne EIR: nazwa, UUID zarejestrowanych profili, General Discoverable flag. |

Próby **1 i 2** są zakończone wynikiem negatywnym. W źródłach pozostaje 2; powrót do 1 wymaga tylko zmiany tej definicji. Tryb 0 daje porównanie bez jawnych zmian identyfikacji. Pełny oryginalny v0.1.6 z konfiguracją jest w [baseline/v0.1.6](../baseline/v0.1.6/README.md), a v0.1.4 nadal pozostaje zachowany.

Moduł `bt_discovery_diag.c/.h` wywołuje publiczne API ESP-IDF po udanym starcie profilu. Źródła lokalnego 6.0.1 mają domyślnie CoD unclassified, ale **stos już automatycznie tworzy EIR**, w tym domyślnie uwzględnia nazwę/UUID. Brak wywołania `esp_bt_gap_config_eir_data()` w v0.1.6 nie oznaczał braku EIR w radiu. Tryb 2 sprawdza konfigurację jawną, zamiast zakładać brak pakietu.

Computer jest klasą odpowiednią dla terminala. Nie są reklamowane profile audio, HID ani niezaimplementowane usługi przez sztuczne bity CoD. `include_uuid=true` prosi SDK o UUID z rzeczywiście zarejestrowanych profili. SPP ma standardowy UUID 0x1101; callback podaje jednak tylko **typy pól EIR**, nie cały payload, więc log `uuid16_type=1` sam nie jest odczytem 0x1101 ani potwierdzeniem odbioru przez telefon.

Zmiana nie dotyczy parowania SSP/Just Works, Secure Connections, ERTM, mocy nadajnika, trybu kontrolera, PIN-u ani obsługi LCD/ADC. Nazwa urządzenia i adres MAC pozostają te same. Odczyt liczby zapisanych bondów jest tylko diagnostyczny; kod nie usuwa kluczy ani NVS. Zmiana klasy/EIR następuje również po ewentualnym restarcie profilu SPP.

## Oczekiwane logi

```text
Starting ESP32 RetroTerminal v0.1.7 ... identity_test=2
Bluedroid config: SSP=1 SC=0
BT saved bonds=<liczba> (read only)
SPP START status=0 ...
BT_DISC: CoD[server-start] local=0x... major=... minor=... service=...
BT_DISC: identity_test=2 ...
BT_DISC: set_cod Computer request=ESP_OK
BT_DISC: identity readback request=ESP_OK
...
GAP GET_DEV_NAME status=0 name='ESP32-RetroTerminal'
BT_DISC: CoD[name-callback] local=0x... major=1 minor=0 service=...
```

Przy zerowych bitach usług Computer / uncategorized ma wartość `0x000100`; inne bity usług są zachowywane. W logu może też pojawić się wcześniejszy callback nazwy, jeszcze przed wykonaniem żądania zmiany klasy. `get_cod()` odczytuje stan **lokalnego stosu**, nie wynik z telefonu. W tej wersji nagłówek opisuje w komentarzu SET_COD_EVT, ale publiczne enum nie zawiera takiego zdarzenia; dlatego obserwujemy klasę także w późniejszym callbacku odczytu nazwy.

W bieżącym trybie 2 dodatkowo oczekujemy:

```text
BT_DISC: config_eir name+registered_UUIDs request=ESP_OK
BT_DISC: CONFIG_EIR status=0
BT_DISC: EIR types=<liczba>
BT_DISC: EIR type=0x09
...
BT_DISC: EIR local summary: name_type=1 uuid16_type=1; remote reception unverified
```

`0x09` oznacza pełną nazwę; `0x08` nazwę skróconą, a `0x02/0x03` listę UUID16. Kolejność i zestaw pól wynikają z konfiguracji SDK i zarejestrowanych profili. Status EIR != 0 albo brak oczekiwanych pól należy zgłosić, nie zaliczać jako poprawnego przygotowania metadanych. Automatyczne zdarzenie konfiguracji EIR może wystąpić także w trybach 0/1.

Przy ponownym błędzie Windows log rozróżnia status ESP i HCI:

```text
GAP ACL disconnect reason=290 (0x122) hci=0x22 LMP_RESPONSE_TIMEOUT handle=...
```

Pojawienie się `GAP encryption mode=...` pokazuje zmianę trybu szyfrowania; samo nie zastępuje AUTH i testu SPP. Nie logujemy kluczy, PIN-ów ani treści pakietów.

## Procedura odtworzenia testu CoD/EIR

Poniższe kroki dokumentują wykonaną próbę 2. Nie jest to prośba o ponowne wykonanie tego samego testu; następny etap dotyczy HCI, jak opisano wyżej.

1. Wgrać bieżący **tryb 2**. Tryb 1 został już przetestowany. Zakończyć monitor portu przed uploadem, potem uruchomić go ponownie na 115200. Użytkownik ma COM10 w `platformio.ini`.

   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32dev -t upload
   ```

2. Sprawdzić, że log mówi **v0.1.7 / identity_test=2**, oraz późniejsze CoD major=1/minor=0. Oczekiwać `config_eir name+registered_UUIDs request=ESP_OK` i późniejszego `CONFIG_EIR status=0` z typami pól. Wcześniejszy automatyczny CONFIG_EIR może nadal wystąpić; sam nie potwierdza wykonania jawnego żądania.
3. Pozostawić Bluetooth Windows wyłączony, zresetować ESP32 i wykonać świeży skan w systemowym ekranie Samsunga. Odnotować widoczność, ewentualną możliwość parowania oraz cały log od resetu. Ta próba dodaje wyłącznie jawną konfigurację EIR do poprzedniej próby.
4. Windows testować **osobno**, z wyłączonym BT telefonu. Usunąć tylko stary wpis tego ESP32, jeśli istnieje, wykonać pojedyncze dodawanie/parowanie i zbierać log do min. 45 s po ACL connect. Samo parowanie systemowe nie musi otwierać SPP; po sukcesie otworzyć terminal serialowy BT i sprawdzić SRV_OPEN/echo.
5. Jeśli nie ma poprawy w żadnym wariancie, wrócić do bazy / trybu 0 i przejść do śladu HCI/BTM lub referencyjnego przykładu SPP na tym samym SDK. Nie wyciągać wniosku o uszkodzeniu RF z samego braku poprawy CoD/EIR.

Kryterium testu wykrywania: Samsung faktycznie pokazuje ESP32, nie tylko poprawne logi lokalne. Kryterium parowania Windows: zakończone dodawanie i AUTH; kryterium terminala: SRV_OPEN oraz identyczne echo. Tryby 1 i 2 nie spełniły kryterium wykrywania. Wyniku Windows na v0.1.7 nie dostarczono.

## Kontrole i źródła

Wyniki kompilacji i regresji są zapisane w [TESTS.md](TESTS.md): pełne buildy trybów 1 i 2 zakończone SUCCESS. Dla pierwotnego przygotowania modułu wykonano 26 testów hostowych PASS, kontrolę składni trybów 0/1/2 i Cppcheck bez zgłoszeń w sprawdzanych kategoriach; tych kontroli nie powtarzano po samej zmianie wyboru wariantu na 2. Nie wykonano uploadu przez agenta. SDK 6.0.1 i konfiguracja PSRAM/UART2 są zachowane.

Publiczne API: [GAP ESP-IDF 6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/api-reference/bluetooth/esp_gap_bt.html). Szczegóły sprawdzono również w **lokalnych źródłach 6.0.1**: `esp_gap_bt_api.h`, `btc_gap_bt.c`, `bta_dm_act.c`, `bt_target.h`. Ograniczenia callbacku EIR i brak SET_COD_EVT wynikają z tych nagłówków, a nie z założenia o zachowaniu telefonu.
