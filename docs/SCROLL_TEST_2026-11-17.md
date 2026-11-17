# Przewijanie, pauza i nadpisanie historii — 17.11.2026

## Zakres i potwierdzone wyniki

Próba na działającym firmware z LCD CS4, przed zmianą UI v0.3.11. Zapis COM5 uruchomiony przed transmisją przez COM14/HC-05 do UART1. Oba porty115200 8N1. Bez wgrywania, resetowania ESP32, kasowania NVS i konfiguracji modułów HC-05. Początkowe RIS zerowało model terminala, nie ESP32; paczka BUF nie zawierała RIS, aby zachować pauzę.

| Etap | Wysłane dane | RX=parser, narastająco | CRC32 narastająco | Historia | Wynik transportu |
|---|---:|---:|---|---:|---|
| LOG0001–0140 |4488 B|4488 B|013c2758|112|zgodne CRC, błędy/straty0|
| LOG0141–0425 |9120 B|13608 B|761422b7|128|zgodne CRC, błędy/straty0|
| BUF0001–0512 |15360 B|28968 B|7f62b6dd|128|zgodne CRC, błędy/straty0|

Łącznie937 numerowanych wierszy. Użytkownik potwierdził, że gesty w górę/dół zatrzymują i przewijają obraz w obu kierunkach. COM5 rejestrował PAUSE i zmiany offsetu39→80→39→21→0 podczas dalszego odbioru. Użytkownik zgłosił brak reakcji przy napisie PAUZA/WZNOW. Z tego powodu pełny test wznowienia nie jest zaliczony.

512 dodatkowych wierszy wysłano przy PAUSE i pełnej historii. Limit pozostał128, odbiór i parsowanie trwały bez strat; offset10 został ograniczony do0 przy nadpisywaniu najstarszych danych. Test dotyczy kontrolowanego nadpisywania bufora historii, nie wymuszenia przepełnienia sprzętowego FIFO UART. Nie sprawdzono jeszcze na LCD numerów najstarszego/najnowszego dostępnego wiersza po wznowieniu. Ocena końcowego obrazu po paczce BUF pozostaje oczekiwana od użytkownika.

W obserwowanym oknie po puszczeniu dotyku liczniki próbek nie rosły; IRQ budzi pomiary podczas kontaktu. Końcowe liczniki samples784/valid428/io0/edges3208. Liczba zboczy nie oznacza liczby naciśnięć. Log nie zawiera zaobserwowanego restartu; raportowany wolny wewnętrzny heap144656 B był stały w próbie. Nie jest to długotrwały test stabilności.

## Przyczyna nietrafiania i zmiana v0.3.11

Dawne pole przycisku miało zaledwie8 px wysokości: y232–239. Zapis COM5 zawiera kontakty przy dolnym lewym napisie m.in.27,228;29,227;24,229;48,225;32,231;58,222;54,224. Współrzędne y222–231 nie spełniały warunku. Ten błąd wyboru obszaru potwierdza kod i ślad współrzędnych; nie wymaga ponownej kalibracji.

Na polecenie użytkownika:

- Tapnięcie w tekst w trybie LIVE zatrzymuje obraz po puszczeniu; ponowne tapnięcie w tekst pozostawia pauzę.
- WZNOW na dole po lewej przywraca LIVE. W stanie LIVE wyświetla się sam napis LIVE.
- Pola WZNOW i CR sięgają24 px od dołu; pionowy ruch anuluje tapnięcie i przewija historię.
- CAL zachowuje własne przytrzymanie2 s. Kalibracja/NVS, IRQ, piny, historia i parser pozostają zgodne z dotychczasowym firmware.

Kompilacje v0.3.11 `esp32dev_internal` i `esp32dev_touch`: SUCCESS,28,860 s i29,496 s. Kontrole programowe po publikacji zostaną zapisane osobnym commitem. Nowego firmware nie wgrano; obsługa nowych tapnięć nie jest jeszcze potwierdzona na panelu.

## Materiał pomiarowy i ograniczenia

Lokalny materiał w `.pio/audit/scroll_v0310_2026-11-17/`: `console.txt`, `timeline.jsonl`, trzy pliki `*_result.json`, wysłane pliki `*_sent.bin` i zamknięty `status.json`. Katalog `.pio` jest ignorowany przez Git; trwały skrót wyników stanowi ten raport.

Pierwszy helper zapisu zakończył się błędem Windows PermissionError o19:01:52 UTC. Nie zachowano tracebacku tej pierwszej awarii, więc dokładna przyczyna pozostaje niepotwierdzona. Próba wysyłania BUF wykryła nieaktywny zapis i zatrzymała się przed transmisją. Po dodaniu tolerancji krótkiego zablokowania pliku statusu wznowiono dopisywanie logów bez resetu ESP32; liczniki zachowały13608 B. W przerwie niczego nie nadawano. Dopiero potem wysłano512 wierszy. Końcowy zapis zakończono poprawnie; **COM5 i COM14 są zamknięte**.

Pozostała próba fizyczna v0.3.11: tapnięcie → pauza, kolejne tapnięcie bez wznowienia, gesty obu kierunków, WZNOW podczas ciągłego odbioru; następnie wzrokowe sprawdzenie końców historii. Dla zachowanego strumienia BUF oczekiwany najstarszy wiersz przy maksymalnym offset128 to BUF0357, LIVE zaczyna BUF0485 i kończy BUF0512 plus pusty wiersz29. To wartości oczekiwane, jeszcze nieodczytane z LCD.
