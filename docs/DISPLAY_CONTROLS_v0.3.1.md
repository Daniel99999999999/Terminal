# Pauza obrazu i zakończenia wiersza — v0.3.1

Użytkownik sprawdził pisanie z PuTTY przez COM14: tekst dochodził, ale Enter wracał do początku bieżącego wiersza. Test znanych bajtów CR/LF/CRLF potwierdził transport bez strat. Dotychczasowy model realizował standardowe CR: powrót karetki bez przesunięcia w pionie. Nie przechwycono oddzielnie bajtów wysyłanych przez klawisz Enter w sesji PuTTY użytkownika; sam objaw jest zgodny z wysłaniem CR.

## Tryb CR

Nowa opcja `CONFIG_RETRO_DISPLAY_CR_AS_NEWLINE` jest domyślnie włączona w aplikacji. Zwykły CR przechodzi do kolumny 0 następnego wiersza. Bezpośrednie CR+LF przechodzi o jeden wiersz, także gdy bajty przyjdą w osobnych porcjach. Kolejne dwa CR tworzą dwa przejścia. Sam LF zachowuje dotychczasową semantykę VT, zależną od LNM.

Opcja działa w modelu wyświetlania: bajty odbioru i CRC pozostają niezmienione. Dane wewnątrz ignorowanych OSC nie otrzymują dodatkowych przejść. Ustawienie nie zmienia obsługi kolorów, kursora ani innych komend ANSI. **Przy CR=NL nadpisywane linie postępu będą rozbijane na osobne wiersze.** Dla takich logów należy wybrać CR=CR, czyli standardową semantykę VT. Nie istnieje niezawodne rozpoznanie intencji pojedynczego CR wyłącznie z tego bajtu.

Bez dotyku tryb wybiera Kconfig, a aktualny stan widać w stopce. Z dotykiem można go przełączyć po kalibracji. Ustawienie z UI obowiązuje do resetu; zapis kalibracji NVS nie zapisuje tej preferencji. Reset terminala ESC c zachowuje wybraną preferencję użytkownika.

Opcje „Implicit CR/LF” w PuTTY dotyczą interpretacji obrazu w samym PuTTY; nie należy traktować ich jako pewnego sposobu zmiany bajtów nadawanych po serial. [Dokumentacja PuTTY: opcje terminala](https://the.earth.li/~sgtatham/putty/latest/htmldoc/Chapter4.html#config-lfcr).

## Sterowanie po kalibracji XPT2046

Dolny pasek ma trzy obszary:

| Obszar | Działanie |
|---|---|
| Lewa trzecia, PAUZA / WZNÓW | Dotknięcie zamraża lub wznawia obraz terminala. |
| Środkowa trzecia, CR=NL / CR=CR | Dotknięcie przełącza interpretację CR. |
| Prawa trzecia, CAL:2s | Przytrzymanie przez 2 s uruchamia ponowną kalibrację. |

Jedno naciśnięcie wywołuje jedną zmianę; przytrzymywanie PAUZY lub CR nie przełącza wielokrotnie. Wejście do kalibracji wyłącza pauzę. Przyciski są dostępne w widoku terminala; na ekranie kalibracji i testu obowiązuje procedura [pięciu punktów](TOUCH_CALIBRATION_v0.3.0.md).

Pauza zatrzymuje odświeżanie treści i miganie kursora. Stopka może się zmienić, aby pokazać WZNÓW. **UART1, parser i bieżący model nadal pracują.** Po wznowieniu odrysowywany jest aktualny model; to nie jest historia scrollback. Dane, które w międzyczasie wyszły poza wysokość bieżącego modelu, nie są zachowywane do przewijania. Scrollback pozostaje kolejną funkcją.

## Weryfikacja

Regresje obejmują CR, CRLF na wszystkich granicach podziału, kolejne CR, rozdzielenie pary komendą ANSI lub utratą transportu, ignorowane OSC, przełączanie na ścisły VT i reset terminala. Test UI obejmuje PAUZĘ/WZNÓW, pojedynczą akcję przy przytrzymaniu, przełączenie CR i nowy obszar kalibracji.

Log SCREEN zawiera teraz pozycję kursora `(x,y)` liczoną od zera, stan LIVE/PAUSE i tryb CR. Umożliwia to sprawdzenie skutku znanych bajtów CR na modelu przez COM5 bez zgadywania z samego licznika odbioru. Potwierdzenie obrazu i dotyku nadal wymaga sprzętu/użytkownika. Wyniki w STATUS.
