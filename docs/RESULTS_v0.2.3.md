# Wynik v0.2.3 — 15.11.2026

Źródła `df48866` opublikowano po udanej kompilacji i przed próbą sprzętową. Upload `esp32dev_internal` przez COM5 zakończył się SUCCESS; esptool zweryfikował hash zapisu. [Log startu](logs/2026-11-15_v0.2.3_internal_boot.txt) obejmuje 15 s po uploadzie: brak restartów, regularne HEALTH/UART1/SCREEN, internal_free=224408 B. Powitanie trwało 3001 ms (501→3502 ms). W tym czasie RX=0; ten odczyt nie był testem przesyłania danych.

Użytkownik zaakceptował obrót LCD3 jako docelowe ustawienie i polecił przejść dalej bez kolejnego wzoru z narożnikami. Nie wysyłano następnego wzoru. Jest to akceptacja ustawienia przez użytkownika, bez nowego zdjęcia po uploadzie. Na ESP32 pozostaje v0.2.3 z RAM wewnętrznym, dotyk wyłączony. Wybrany następny etap: XPT2046 i kalibracja.
