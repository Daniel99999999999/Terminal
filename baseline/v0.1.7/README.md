# Zachowana baza v0.1.7 — identity_test=2

Dokładne 14 plików źródeł i konfiguracji sprzed dodania obserwatora HCI v0.1.8. Sumy kontrolne są w [SHA256.json](SHA256.json). Nie modyfikować tej kopii podczas dalszej pracy. Bazy v0.1.4 i v0.1.6 pozostają zachowane.

Wynik sprzętowy użytkownika: INIT/START, Computer CoD i jawne EIR z polami nazwy/UUID/flags potwierdzone lokalnie; Samsung nadal nie widzi urządzenia. Ten kod nie potwierdza zakończonego parowania ani transferu SPP. [Log próby](../../docs/logs/2026-11-02_v0.1.7_identity2_samsung.txt).

Build z katalogu głównego projektu:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d baseline/v0.1.7 -e esp32dev
```

Powrót na płytce: zakończyć monitor COM10 i wykonać ten sam rozkaz z dopisanym `-t upload`. Zachowano ustawienia użytkownika COM10; port może wymagać dopasowania do rzeczywistego połączenia. Przy tworzeniu kopii nie wykonano osobnego buildu w katalogu bazy ani uploadu.
