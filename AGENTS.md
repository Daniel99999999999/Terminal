# Instrukcje dla Codex — ESP32 RetroTerminal

## Cel projektu

Zbuduj terminal szeregowy DIY wykorzystujący podzespoły użytkownika: ESP32-WROVER (D0WD-V3), shield TFT 2,4 cala z 8-bitowym interfejsem równoległym, dotyk rezystancyjny 4-wire. Docelowo terminal dwukierunkowy UART ↔ Bluetooth SPP; LCD wyświetla ekran terminala i historię, dotyk może obsługiwać UI.

## Środowisko i zasady niepodlegające negocjacji

- Windows + VS Code + PlatformIO, `platform = espressif32`, `board = esp32dev`, **ESP-IDF 6.0.1** z logów użytkownika (sprawdź faktycznie zainstalowane zależności), **C**, bez Arduino.
- Kod nieblokujący w operacjach komunikacyjnych; preferuj zdarzenia, kolejki, DMA i kontrolowane timeouty. Nie zastępuj tego pętlami z `delay_ms()`.
- Zachowaj realny kod v0.1.4 jako punkt odniesienia. Nie przepisuj całej architektury bez wskazania problemu i testów regresji. Zmieniaj małe, sprawdzalne fragmenty.
- **GPIO16 i GPIO17 są zajęte przez PSRAM** w ESP32-WROVER — nigdy nie przypisuj ich do UART/LCD/dotyku.
- UART2: **TX GPIO25, RX GPIO26**, log konsoli UART0 przez USB, 115200 baud. Wszelkie sygnały do ESP32 muszą mieć zgodne poziomy 3,3 V. Nie łącz sygnałów RS-232 ani 5 V bez odpowiedniej konwersji.
- **Nie zakładaj**, że shield to ILI9341: kontroler LCD jeszcze nie został zidentyfikowany. Nie przypisuj pinów DB0–DB7/WR/DC/CS/RD/RST bez rzeczywistej weryfikacji PCB.
- Shield ma dwa układy buforowe z rodziny HC245 (oznaczenie widoczne na zdjęciu); ich zasilanie, kierunki i poziomy logiczne trzeba zbadać. Nie podłączaj wprost nieznanych sygnałów z płytki Arduino do ESP32.
- Panel rezystancyjny ma wyprowadzenia **X+/X−/Y+/Y−**, widoczne na zdjęciu spod mikroskopu. Przebieg do pinów shielda **nie jest potwierdzony**. Nie kopiuj domyślnego pinoutu popularnych shieldów.
- PSRAM: wykryty układ 8 MB, **4 MB mapowane** i działające w logu testowym. Na klasycznym ESP32 I80 DMA ma korzystać z buforów **DMA-capable INTERNAL RAM**; framebuffer/logi można przechowywać w PSRAM i kopiować porcjami.
- Wi-Fi w module zostało zgłoszone jako niesprawne. Android nie wykrywa nazwy ani adresu ESP32 SPP, mimo że serwer się uruchamia. Traktuj Bluetooth RF jako **niepotwierdzony**, nie jako działający.
- W obszarze RF użytkownik widział podejrzane połączenie w pobliżu L1/L4/C15. **Nie polecaj usuwania cyny ani przeróbek** bez schematu, pomiarów i identyfikacji elementu; nie myl go ze zworką antenową R14/R15.

## Reguły pracy

1. Na początku sesji przeczytaj `README.md`, `docs/STATUS.md`, `docs/HARDWARE.md`, `docs/NEXT_STEPS.md` i istniejący `src/main.c`.
2. Rozróżniaj **potwierdzone wynikami testów** od **hipotez**. Nie opisuj podzespołów ani pinów jako ustalonych, jeśli nie są.
3. Wprowadzaj zmiany przez małe moduły `.c/.h`, aktualizuj `src/CMakeLists.txt` i dokumentację; unikaj monolitu, ale nie rozbijaj bez potrzeby działającej części.
4. Po każdej zmianie wykonaj dostępne kontrole. Jeśli nie możesz uruchomić PlatformIO lub testu fizycznego, napisz to wprost. Nie deklaruj "skompilowane" albo "działa" bez potwierdzenia.
5. Przed przyłączeniem LCD: zweryfikuj sterownik, pinout, napięcia i kierunki linii. Dla dwukierunkowego odczytu rejestrów pamiętaj o buforach i tri-state.
6. Przy diagnozie Bluetooth: instrumentuj API / eventy GAP i sprawdź zwroty, nie wyciągaj wniosków o sprawności RF z samego logu SPP.
7. Zachowaj prostą ścieżkę powrotu do działającego kodu v0.1.4; unikaj czyszczenia niepowiązanych plików i kasowania ustawień użytkownika.
8. Komunikuj się z użytkownikiem po polsku. Dokumentuj zmiany, plan testu i ryzyko dla hardware.

## Pierwsze zadanie

Aktualizacja zgodnie z poleceniem użytkownika z 03.11.2026: **zakończono próby wbudowanego Bluetooth**. Oryginalny akceptor Espressif oraz ostatni wariant pełnej kalibracji RF również pozostały niewidoczne na Samsungu. Nie rozpoczynaj kolejnych eksperymentów BT bez nowego polecenia. Priorytetem jest **TFT LCD**: najpierw bezpieczna identyfikacja kontrolera, mapowanie PCB/HC245/dotyku oraz napięcia, potem kod i test grafiki. Użytkownik ma multimetr. Docelowy Bluetooth to **zewnętrzny HC-05 na osobnym UART**, z zachowaniem UART2 TX25/RX26 i USB/UART0. Uwzględnij dodatkowy UART w budżecie GPIO; nie kopiuj starej tabeli samego LCD jako gotowego schematu. Nie uznawaj toru RF za uszkodzony bez dowodu. Aktualny stan: `docs/STATUS.md`, procedura: `docs/TFT_BRINGUP.md`.

Wymaganie z 13.11.2026: zwykły tekst ma być biały na czarnym tle, a sekwencje ANSI/VT100 i popularne rozszerzenia mają być interpretowane automatycznie. W v0.1.10 dodano model/parser w pamięci oraz odbiór kopii UART przez kolejkę. Dokładny podzbiór, ograniczenia i testy są w `docs/TERMINAL_EMULATION.md`; nie deklaruj pełnej zgodności xterm ani fizycznego obrazu TFT bez testu. Nie gub stanu komend/UTF-8 na granicach porcji i nie zmieniaj bajtów przekazywanych przez most. Fizyczny renderer nadal wymaga wcześniejszej identyfikacji i pomiarów shielda.
