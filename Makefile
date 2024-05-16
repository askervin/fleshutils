PREFIX ?= /usr/local

build: bin/ic

bin/ic: cmd/ic/ic.c
	$(CC) -rdynamic -g -O0 -Wall -o $@ $< -ldl

install: build
	mkdir -p $(PREFIX)/bin
	bash -c 'for f in bin/*; do [[ "$$f" != *~ ]] && install -v $$f $(PREFIX)/$$f; done'

clean:
	$(RM) bin/ic
