TARGET=caltimist
OBJS=$(patsubst %.c,%.o,$(wildcard *.c))
FORMATOBJS=$(patsubst %.c,%.o,$(wildcard formats/*.c))
TESTS=$(patsubst %.c,test_%,$(filter-out ${TARGET}.c, $(wildcard *.c)))
CFLAGS=-pedantic -Wall -O2 -fomit-frame-pointer -fPIE -D_GNU_SOURCE
LDLIBS=-lowfat -lssl
CC=gcc

${TARGET}: ${OBJS} ${FORMATOBJS}
	${CC} -o $@ ${OBJS} ${FORMATOBJS} ${LDFLAGS} ${LDLIBS}
	@[ ! -L ${TARGET}.cgi ] && ln -s ${TARGET} ${TARGET}.cgi || true

nossl: CC=diet -v gcc
nossl: LDFLAGS=-static
nossl: CFLAGS+=-DNOSSL
nossl: LDLIBS=-lowfat
nossl: ${TARGET}

memcheck: CFLAGS += -g -ggdb
memcheck: ${TARGET}
	@valgrind --leak-check=full --show-leak-kinds=all -s ./$< -vvvv

stats: ${TARGET}
	@/usr/bin/time -v ./$<
	du -k $<

unittests: ${TESTS}

test_%: *.c *.h ${FORMATOBJS}
	${CC} -o $@ $(patsubst test_%,%.c,$@) $(if $(subst test_ics,,$@),,${FORMATOBJS}) ${CFLAGS} -DUNITTEST ${LDFLAGS} ${LDLIBS}
	./$@

formats/%.o: formats/*.[ch] format.h
	${CC} ${CFLAGS} -o $@ -c $(subst .o,.c,$@)

%.o: %.c *.h
	${CC} ${CFLAGS} -c $<

clean:
	rm -f ${TARGET} ${OBJS} ${TESTS} ${FORMATOBJS}
