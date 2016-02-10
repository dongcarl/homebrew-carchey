PREFIX ?= /usr/local

CFLAGS += -O0 -framework CoreFoundation -framework IOKit -framework CoreServices -Weverything

BROOT = ${PREFIX}/bin
BPATH = ${BROOT}/carchey
MROOT = ${PREFIX}/share/man/man1
MPATH = ${MROOT}/carchey.1

carchey: carchey.c
	${CC} ${CFLAGS} $< -o $@

clean:
	rm carchey

install: carchey
	@echo "carchey -> ${BPATH}"
	@mkdir -p "${BROOT}"
	@cp -f carchey "${BROOT}"
	@chmod 755 "${BPATH}"
	@echo "carchey.1 -> ${MPATH}"
	@mkdir -p "${MROOT}"
	@cp -f carchey.1 "${MPATH}"
	@chmod 644 "${MPATH}"

.PHONY: clean install
