cmake_minimum_required(VERSION 3.22)

# Minimalna konfiguracja glowna ESP-IDF dla PlatformIO.
# Nie ograniczamy jeszcze COMPONENTS: ta konfiguracja pozostaje najblizsza
# oryginalnemu projektowi v0.1.4, ktory wystartowal na sprzecie.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Terminal)
