CC ?= cc
CFLAGS ?= -Wall -Wextra -Werror -std=c99 -pedantic
PREFIX ?= /usr/local

BIN = tagit
SRC = src/tagit.c

.PHONY: all clean install test

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

test: $(BIN)
	sh tests/run-tests.sh

install: $(BIN)
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 0755 "$(BIN)" "$(DESTDIR)$(PREFIX)/bin/$(BIN)"

clean:
	rm -f "$(BIN)"
