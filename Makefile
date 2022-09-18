bin/ic: cmd/ic/ic.c
	$(CC) -rdynamic -g -O0 -o $@ $<

clean:
	$(RM) bin/ic

install:
	python3 setup.py install
