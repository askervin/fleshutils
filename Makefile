PREFIX ?= /usr/local

build: bin/ic

bin/ic: cmd/ic/ic.c
	$(CC) -rdynamic -g -O0 -Wall -o $@ $< -ldl

install: build
	mkdir -p $(PREFIX)/bin
	install -v bin/* $(PREFIX)/bin

clean:
	$(RM) bin/ic
