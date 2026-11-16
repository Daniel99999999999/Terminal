# Biały LCD po EN/RESET — v0.3.7

## Zgłoszenie i fakty

Użytkownik potwierdził całkowicie biały ekran od naciśnięcia EN/RESET na ESP32, bez widocznego powitania. LCD pozostaje zasilany podczas restartu ESP32. Log v0.3.6 dochodzi do READY, rejestruje wysłanie powitania, odtwarza mapę NVS i stabilnie pracuje. Samo API SPI bez błędu nie dowodzi, że panel odebrał i wykonał komendy. Wcześniejsze logi TOUCH_RAW dotyczą osobnej magistrali SPI2; io=0 w nich nie sprawdza LCD na SPI3.

## Potwierdzona wada sekwencji

`vendor/esp_lcd_ili9341/esp_lcd_ili9341.c`: reset sprzętowy ustawia RESX LOW na10 ms, następnie HIGH i czeka10 ms. `src/lcd_panel.c` wywoływał bezpośrednio init po powrocie reset. Pierwszym poleceniem init jest SLPOUT0x11. Zatem odstęp RESX HIGH→SLPOUT wynosi nominalnie10 ms, niezależnie od późniejszego100 ms oczekiwania wewnątrz init.

[Dokumentacja ILITEK ILI9341 V1.05, §15.4, strona225](https://files.waveshare.com/upload/e/e3/ILI9341_DS.pdf#page=225) przewiduje do120 ms kończenia resetu rozpoczętego w Sleep Out. Uwaga7 zabrania SLPOUT przez120 ms po zwolnieniu resetu. Restart EN aktywnego terminala może zastać LCD w tym stanie. Ta niezgodność czasowa jest wykazana w kodzie; nie dowodzi jeszcze, że wyjaśnia wszystkie przypadki bielenia ani że połączenia są poprawne.

## Poprawka

Aplikacja dodaje `vTaskDelay(150 ms)` po udanym reset API, przed init. Przy obecnym sterowniku daje to nominalnie160 ms od RESX HIGH do SLPOUT; zapas uwzględnia rozdzielczość planisty. Log TFT mierzy dodatkowy odstęp po powrocie API i nie nazywa go potwierdzeniem odpowiedzi panelu. Sterownik upstream pozostaje bez zmian; zgodność jego manifestu zachowana.

Czeka wyłącznie zadanie ekranu podczas startu. UART1 uruchamia się osobno i odbiera w tle. Powitanie nadal ma trwać3 s po narysowaniu. Piny GPIO27/21/22/18/23/19, SPI20 MHz, rotacja3 i NVS nie zmieniają się. Nie wprowadzono automatycznych ponowień inicjalizacji ani zmian parametrów komunikacji w tej samej próbie.

## Kontrole i odbiór

`python tests/run_host_tests.py --suite lcd` kompiluje rzeczywiste `lcd_panel.c` i niezmieniony sterownik upstream. Wirtualne GPIO rejestruje zbocze RESX, SPI rejestruje pierwszą komendę SLPOUT. Regresja wymaga co najmniej120 ms, nie samej obecności funkcji delay. Osobne uruchomienia sprawdzają błąd init i zachowanie bufora po timeout DMA. Test nie modeluje elektrycznych połączeń ani odpowiedzi fizycznego panelu.

Kolejność: build internal/touch → commit/push → regresja i analiza → upload aplikacji z zachowaniem NVS → zapis startu oraz próba przyciskiem EN → osobny commit wyników. Przy teście trzeba potwierdzić widoczne powitanie i terminal po kilku resetach, bez zmiany okablowania. Przechwycenie wyłącznie poprawnych logów nie wystarcza do uznania białego ekranu za naprawiony. Jeśli objaw pozostanie, kolejne pomiary dotyczą fizycznego RESX/CS/DC/zasilania i osobnej próby wolniejszego SPI; nie zakładamy wyniku z góry.

## Wynik po publikacji

Źródła78db372 opublikowano po udanych buildach obu profili i przed testami. Regresja z rzeczywistym sterownikiem: poprzedni lcd_panel.c daje10 ms i oczekiwany błąd testu; poprawiony160 ms i PASS. Błąd init oraz timeout DMA również PASS; Cppcheck obu profili bez zgłoszeń. Upstream zgodny z manifestem.

Wgrano v0.3.7 touch. Zapis uruchomiony przed resetem potwierdził nową wersję, dodatkowe150 ms i odtworzenie mapy NVS. Następnie uruchomiono osobny zapis bez automatycznych resetów. Użytkownik trzykrotnie nacisnął EN i potwierdził widoczne powitanie oraz terminal po każdym. Log zawiera dokładnie3 starty,3 odstępy150 ms,3 odtworzenia NVS i3 zakończenia powitania. [Wyniki](logs/2026-11-16_v0.3.7_validation.json). Zapis zakończony; COM5 wolny.

Wynik3/3 potwierdza ustąpienie zgłoszonego objawu w tej próbie po usunięciu niezgodności czasowej. Nie wykonano pomiaru przebiegu RESX oscyloskopem; nie przypisujemy automatycznie tej samej przyczyny wszystkim historycznym przypadkom bielenia LCD ani bootloopowi PSRAM.
