# Baza v0.1.9 sprzed dodania modelu ANSI/VT100

Zachowano dokładne pliki głównego projektu z 13.11.2026: main.c, oba CMakeLists, platformio.ini, sdkconfig.defaults/esp32dev oraz debug_led_logic.h sprzed dodania kanału nadzoru zadania terminala. Nazwy użytkownika ESP32-Terminal/Terminal-SPP i port COM5 są zachowane. SHA256.json opisuje siedem plików. Pozostałe moduły BT/LED/HCI, helper CMake i vendor nie zmieniły się w patchu modelu terminala.

To punkt powrotu w tym repozytorium, nie osobny projekt do kompilowania w katalogu baseline. Aby wrócić, po zachowaniu własnych nowszych zmian skopiować te pliki na odpowiadające im miejsca w katalogu głównym oraz src. Przywrócony src/CMakeLists nie dołącza modułów ANSI/terminal/view. Pozostałe wcześniejsze moduły są nadal w src. Używany przez v0.1.9 backport kontrolera pozostaje w cmake/vendor projektu.

Poprzedni main.c ma SHA256 88823473b49736be111dd5291f72e7c1e6730be846a83fb6288cd05cb8146e5b. Bazy v0.1.4/v0.1.6/v0.1.7/v0.1.8 pozostały zachowane. Powrót źródeł nie wymaga kasowania NVS.
