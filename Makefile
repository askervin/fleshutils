bin/ic: cmd/ic/ic.c
	$(CC) -rdynamic -g -O0 -Wall -o $@ $< -ldl

clean:
	$(RM) bin/ic

install:
	python3 setup.py install
