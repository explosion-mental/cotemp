# scaf - simple auto cpu freq
# See LICENSE file for copyright and license details.

include config.mk

SRC = cotemp.c util.c
OBJ = ${SRC:.c=.o}

all: options cotemp

options:
	@echo cotemp build options:
	@echo "VERSION  = ${VERSION}"
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

config.h:
	cp config.def.h config.h

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

cotemp: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f cotemp ${OBJ} cotemp-${VERSION}.tar.gz

dist: clean
	mkdir -p cotemp-${VERSION}
	cp -R LICENSE Makefile README.md config.mk config.def.h util.h ${SRC} cotemp.1 cotemp-${VERSION}
	tar -cf cotemp-${VERSION}.tar cotemp-${VERSION}
	gzip cotemp-${VERSION}.tar
	rm -rf cotemp-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f cotemp ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/cotemp
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < cotemp.1 > ${DESTDIR}${MANPREFIX}/man1/cotemp.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/cotemp.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/cotemp \
		${DESTDIR}${MANPREFIX}/man1/cotemp.1

.PHONY: all options clean dist install uninstall
