# Nombre del ejecutable
TARGET = pomorock

# ficheros fuente
SRC = $(wildcard *.c)

# Flags del compilador
CFLAGS = -Wall -Wextra -O2

# Librerías necesarias
LDLIBS = 

# Regla principal
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

# Limpieza de ficheros generados
clean:
	rm -f $(TARGET)

# Instalación opcional en ~/.local/bin
install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	install -m 755 $(TARGET) $(HOME)/.local/bin

.PHONY: all clean install
