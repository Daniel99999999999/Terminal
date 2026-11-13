# Zachowana baza v0.1.8 — przed poprawką kontrolera

Dokładne 18 plików źródeł/konfiguracji sprzed v0.1.9, z manifestem [SHA256.json](SHA256.json). Ta wersja korzysta z oryginalnej biblioteki kontrolera z zainstalowanego ESP-IDF 6.0.1; nie zawiera nowego include CMake ani archiwum vendor.

Wynik użytkownika: HCI potwierdza WRITE_SCAN_ENABLE=0x03/status=0. Samsung nie widzi ESP32, Windows zestawia ACL, lecz zgłasza brak odpowiedzi przy parowaniu; HCI 0x22 w wycinku. Pełny transfer SPP nie jest potwierdzony. [Analiza i ograniczenia logów](../../docs/BT_CODE_COMPARISON_2026-11-03.md).

Build / powrót z katalogu głównego:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d baseline/v0.1.8 -e esp32dev
```

Po zamknięciu monitora dopisać `-t upload`, aby wgrać bazę. Zachowano COM10 i ustawienia PSRAM/UART. Nie wykonano osobnego buildu w katalogu bazy ani uploadu; sprawdzono wszystkie hashe manifestu. Bazy v0.1.4, v0.1.6 i v0.1.7 pozostają zachowane.
