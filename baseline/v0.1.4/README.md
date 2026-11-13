# Zachowana baza v0.1.4

Dokładna kopia źródeł i konfiguracji sprzed napraw P1/P2. Kod uruchomił się wcześniej na sprzęcie użytkownika; transmisja i wykrywalność RF pozostają niepotwierdzone.

SHA-256 `src/main.c`: `BAA0BFA758C3C0FABF9E26913BA0089FEE75773ECD21343050045CDF2D0E81A0`.

Powrót do bazy bez nadpisywania bieżących plików:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d baseline/v0.1.4 -e esp32dev
```

Wgranie tej kopii wymaga osobnego polecenia z `-t upload`. Zachowano oryginalny `sdkconfig.esp32dev`; nie trzeba kasować konfiguracji ani NVS.
