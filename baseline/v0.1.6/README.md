# Baza v0.1.6 z testu użytkownika

Dokładna kopia 12 plików źródłowych/konfiguracyjnych sprzed diagnostycznej zmiany CoD/EIR v0.1.7. Zachowuje GPIO2 LED, heartbeat, UART2 GPIO25/26 i ustawienia portu COM10 użytkownika.

SHA-256 `src/main.c`: `51f25172e7c52f3cbd1cc1aaa079d4597200073a43ee53c688e6544aaede446e`.

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d baseline/v0.1.6 -e esp32dev
```

Dodanie `-t upload` wgrywa tę bazę. Nie trzeba nadpisywać bieżących źródeł ani kasować NVS. Ta wersja startuje i zestawia ACL według logu użytkownika, ale nie kończy parowania Windows i nie jest widoczna na Galaxy A55 / Android 16. Nie traktować jej jako działającego radiowo terminala SPP.
