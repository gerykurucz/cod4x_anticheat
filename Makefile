# Fordító beállítása (Windows 32-bit vagy 64-bit célponthoz)
# Linuxos CoD4X szerver 32-bites (x86), ezért natív gcc-t használunk -m32 kapcsolóval.
# FIGYELEM: Szükséges a 32-bites fejlesztői csomag (pl. Ubuntu/Debian: sudo apt install gcc-multilib)
CC = gcc

# Plugin neve (.so kiterjesztés Linuxra)
TARGET = build/nektumshield.so

# Forrásfájlok
SRCS = nektumshield.c

# Fordítási opciók
# -m32: 32-bites bináris generálása (Kötelező a CoD4X Linux szervernek!)
# -O3: Optimalizáció
# -shared: Shared library (.so) készítése
# -fPIC: Position Independent Code (Kötelező Linuxon .so fájlokhoz)
CFLAGS = -m32 -O3 -Wall -shared -fPIC

# Include mappák (ahol a pinc.h és társai vannak)
INCLUDES = -I. -I..

# Linkelési opciók (matematikai könyvtár, 32-bites mód)
LDFLAGS = -m32 -lm

# Alapértelmezett parancs
all: prepare $(TARGET)

# Létrehozzuk a build mappát, ha nem létezik (ne hibázzon rá a Make)
prepare:
	@mkdir -p build

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) $(LDFLAGS)
	@echo "\n^2Sikeres forditas! A plugin elerheto: $(TARGET)^7\n"

# Takarítás
clean:
	rm -rf build