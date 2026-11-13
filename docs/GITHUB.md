# Git i GitHub

Repozytorium: [Daniel99999999999/Terminal](https://github.com/Daniel99999999999/Terminal), gałąź główna `main`. Pierwszy import ma wersję **v0.1.10**; [opis wersji](../CHANGELOG.md) podaje funkcje, wyniki kontroli i ograniczenia sprzętowe.

## Odtworzenie wersji

```powershell
git clone https://github.com/Daniel99999999999/Terminal.git
cd Terminal
git switch --detach v0.1.10
```

Tag wskazuje źródła wydania. Do kolejnych zmian utwórz gałąź, np. `git switch -c feature/tft-bringup`. Instrukcja kompilacji i wymagane zależności są w [README](../README.md). Do odtworzenia sprawdzonego środowiska v0.1.10 użyj espressif32 7.0.1 / ESP-IDF 6.0.1; konfiguracja główna nie przypina wersji platformy. Nie wgrywaj firmware do podłączonego shielda przed weryfikacją jego połączeń i poziomów.

## Zawartość repozytorium

- Źródła C, konfiguracja projektu i SDK, testy hosta, dokumentacja, wybrane zdjęcia i logi diagnostyczne.
- `baseline/` zachowuje wcześniejsze kopie. Manifesty SHA256 obejmują wskazane pliki; nie każda baza jest samodzielnym projektem. `.gitattributes` zachowuje dokładne bajty tych kopii i plików upstream, aby checkout nie zmienił ich hashy.
- Biblioteka `vendor/esp32-bt-lib/` jest celowo wersjonowana: build korzysta z konkretnego archiwum o sprawdzanym SHA256. Pochodzenie i licencję zachowano obok pliku. Przykład Espressif ma osobne metadane i licencję w `diagnostics/esp_idf_spp_acceptor/`.
- `.pio/`, wyniki buildów, cache i generowane `c_cpp_properties.json` / `launch.json` pozostają lokalne. Wspólne zadania i ustawienia VS Code są wersjonowane.

## Kolejne zmiany

Przed commitem sprawdź `git status` i `git diff`, wykonaj kontrole odpowiednie do zmiany i zaktualizuj dokumentację. Przykład dla zmiany kodu terminala:

```powershell
python tests/run_host_tests.py
pio run -e esp32dev
git add src tests docs README.md CHANGELOG.md
git diff --cached --stat
git commit -m "Opisz konkretną zmianę"
git push
```

Nie dodawaj wszystkich wygenerowanych plików przez `git add -f`. Do publikacji używaj własnego uwierzytelnienia Git/GitHub; tokenów nie zapisuj w plikach projektu ani w adresie `origin`. Nie nadpisuj opublikowanej historii przez force push.
