CC      = cc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O2 -march=native -pipe
LDFLAGS = -lX11 -lm
PREFIX  = /usr/local
SRC     = xcwm_2.c
BIN     = xcwm

all: $(BIN)

$(BIN): $(SRC) xcwm.h config.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)

install: all
	install -Dm755 $(BIN) $(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

.PHONY: all clean install uninstall
