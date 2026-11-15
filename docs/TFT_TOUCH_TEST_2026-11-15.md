# Próby LCD i XPT2046 — 15.11.2026

## Wyniki

| Próba | Odbiór/model | Obraz potwierdzony przez użytkownika |
|---|---|---|
| v0.3.3 touch, trzy przytrzymania | irq_low896, valid0, brak błędów API SPI; UART17894 B bez strat | Biały ekran, delikatne migotanie; wysłanie wzoru bez resetu nie przywróciło obrazu. |
| v0.3.3 internal, pierwsze przywrócenie | Start prawidłowy; UART17894 dodatkowych B bez strat | Nadal biały; użytkownik doprecyzował, że po resecie widzi powitanie, potem bielenie. |
| Zachowany v0.3.1 internal | UART17894 B, CRC40cad4e7, bez strat | Powitanie, terminal, strumień, końcowy wzór RGB poprawne. |
| Ponownie ten sam zachowany v0.3.3 internal | UART17894 B, CRC40cad4e7, bez strat | Poprawny obraz, potwierdzony przez użytkownika. |

Strumień tekstu, czyszczenie i końcowy wzór są celowym przebiegiem `tests/serial_smoke.py`. Nie interpretujemy ich jako resetu ani znikania odebranych danych wskutek błędu.

[Zbiorczy wynik](logs/2026-11-15_v0.3.3_validation.json), [dotyk RAW](logs/2026-11-15_v0.3.3_touch_press_watch.txt), [UART touch](logs/2026-11-15_v0.3.3_touch_uart_result.json), [pierwszy UART internal](logs/2026-11-15_v0.3.3_internal_recovery_uart_result.json), [referencja v0.3.1](logs/2026-11-15_v0.3.1_reference_retest_uart_result.json), [ponowna v0.3.3](logs/2026-11-15_v0.3.3_reference_retest_uart_result.json). Logi startu i strumienia o analogicznych nazwach są w tym samym katalogu.

## Ograniczenia wniosków

GPIO36 zmieniało stan podczas nacisku, ale ADC zwracało zera. Nie znamy jeszcze przyczyny; samo powodzenie API SPI nie potwierdza odpowiedzi kontrolera. Nie potwierdzono działania pauzy ani dokładności kalibracji. Poprawne odtworzenie mapy NVS po resecie zostało potwierdzone w logach.

Przy białym LCD nie było widocznych restartów, panic ani błędów renderera w przechwyconych logach. Nie wyklucza to problemu toru fizycznego lub stanu kontrolera. Ponowne wgranie tego samego v0.3.3 dało poprawny obraz, dlatego nie ustalamy przyczyny jako błędu konkretnej wersji, zasilania ani przewodów.

Porównano zachowane ELF: funkcje screen_task, draw, parse_input, terminal_init, terminal_cell_colors, terminal_raster_row, terminal_raster_swap_bytes i lcd_panel_write_row są bajtowo identyczne w obu wariantach internal. Nie oznacza to identyczności całego firmware. Sekcje kodu/danych SDK i adresy różnią się; w lcd_panel_open różni się docelowy adres spi_bus_initialize. [Porównanie ELF](logs/2026-11-15_v0.3.3_internal_elf_comparison.json), [dysasemblacja](logs/2026-11-15_v0.3.3_lcd_open_disassembly.diff).

Na koniec porównania działa v0.3.3 internal, bez dotyku. Wgrywanie referencji zmieniało wyłącznie aplikację pod0x10000, hash zweryfikowany, NVS nie kasowano. Kolejny etap odczytu dotyku ma porównać go z krótkimi transakcjami z v0.3.1, która wcześniej przyjęła rzeczywiste punkty.
