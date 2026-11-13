# Prompt startowy do Codex

Skopiuj poniższy tekst do Codex w VS Code po otwarciu folderu projektu:

> Kontynuujemy ESP32 RetroTerminal. Przeczytaj AGENTS.md, README.md, docs/STATUS.md, docs/HARDWARE.md, docs/NEXT_STEPS.md, docs/TERMINAL_EMULATION.md i kod. Aktualne źródła to v0.1.10: domyślnie biały tekst na czarnym tle, strumieniowy model podzbioru ANSI/VT100 z kolorami xterm, UTF-8, kursorem, przewijaniem, ramkami DEC i ekranem alternatywnym. Kopia UART2 trafia przez kolejkę do modelu także bez BT. Testy: 62 scenariusze i 2 warianty adaptera PASS, Cppcheck 10 C i build ESP-IDF 6.0.1 SUCCESS; nie wgrano nowego programu ani nie uruchomiono LCD. Nie deklaruj pełnej emulacji xterm ani sprawności fizycznego renderera. Ostatni potwierdzony program na ESP32 to ref-fullcal-6.0.1 / ELF 3769a91ad..., COM5. Użytkownik zakończył próby wbudowanego BT; nie wracaj do nich. Priorytet to pomiary i identyfikacja TFT/HC245/dotyku, następnie renderer modelu; docelowy BT przez HC-05 na osobnym UART. Użytkownik ma multimetr. Nie zakładaj ILI9341 ani pinoutu PCB. UART2 TX25/RX26 i USB/UART0 zachowane, GPIO16/17 zajęte PSRAM. C, bez Arduino. Bazy v0.1.4/v0.1.6/v0.1.7/v0.1.8/v0.1.9 zachowane. Autobaud jest możliwy sprzętowo, ale nadal niewłączony w aplikacji.

## Praca dalsza

**Wyniki sprzętowe v0.1.6:** użytkownik dostarczył log z udanym startem, nazwą i adresem `7C:87:CE:2F:DE:DA`. Windows widzi ESP32 i zestawia ACL, ale pierwsza próba parowania kończy się po ok. 30 s timeoutem LMP 0x22 (GAP 290); brak AUTH i SRV_OPEN. Galaxy A55 SM-A556B/DS z Androidem 16 nie widzi urządzenia w systemowym skanowaniu. Użytkownik powtórzył skan przy wyłączonym BT Windows, po resecie ESP32 i z odległości 30–50 cm: **nadal niewidoczny**. Nie powtarzaj tej samej próby na v0.1.6 jako nowego kroku diagnostycznego. Wynik nie przesądza o awarii RF.

**Wynik v0.1.7 / tryb 1:** pełny log w `docs/logs/2026-11-02_v0.1.7_identity1_samsung.txt`: INIT/START status=0, CoD=0x000100, automatyczne EIR z polami nazwy/UUID, brak ACL/AUTH/SRV_OPEN do 261486 ms, Samsung nadal niewidoczny. Nie rozpoznawaj zawieszonej inicjalizacji na podstawie uciętego wcześniejszego fragmentu.

**Wynik v0.1.7 / tryb 2:** log w `docs/logs/2026-11-02_v0.1.7_identity2_samsung.txt`: INIT/START=0, CoD=0x000100, jawny CONFIG_EIR=0 z dodatkowym polem flags, brak ACL/AUTH/SRV_OPEN do 326456 ms, Android nadal niewidoczny. Obie próby CoD/EIR są wykonane; nie proś o ich powtórzenie jako nowego kroku.

**Wynik v0.1.8:** pełny log `docs/logs/2026-11-03_v0.1.8_android_hci.txt` potwierdza 0x03/status=0 i pełny ślad do 126757 ms; brak wyłączenia/resetu/ACL/AUTH/SPP. Android nadal niewidoczny, test kontrolny innego urządzenia dodatni. W wycinku Windows handle=128 rozłączono z 0x22 przy 405017 ms (brak początku tego łącza), a handle=129 zestawiono przy 412847 ms i obserwowano tylko do 422347 ms. Nie scalać tych handle i nie wyciągać z tego czasu jednej próby.

**Bieżący kierunek:** [pomiary TFT](docs/TFT_BRINGUP.md), następnie kod LCD i [integracja HC-05](docs/HC05_INTEGRATION.md). Zwykła referencja i ostatnia próba pełnej kalibracji są zakończone negatywnie. Nie zlecać kolejnych testów wewnętrznego BT.

**Oddzielnie Windows:** zwykłe **PIO: Upload** wgrywa główny v0.1.9 z btfix=83ede08. Biblioteka przed poprawką pasuje bajtowo do 6.0.1, poprawiona pochodzi z oficjalnego commita i jest linkowana z vendor, a komputer ma adapter Realtek. Skuteczność dla naszego przypadku niepotwierdzona. Nie przedstawiać poprawki jako naprawy Androida. [Dowody i porównanie C](docs/BT_CODE_COMPARISON_2026-11-03.md).

Po zakończeniu diagnostyki Bluetooth kolejne etapy są w `docs/NEXT_STEPS.md`. Jeśli sprzętowego Bluetooth nie uda się uruchomić, można rozważyć osobny moduł UART-Bluetooth, ale dopiero po zakończeniu testów.
