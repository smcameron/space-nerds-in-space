
CC ?= gcc

DEBUGFLAG=
OPTIMIZEFLAG=-O3
STOP_ON_WARN=

ifeq (${V},1)
Q=
ECHO=echo
else
Q=@
ECHO=echo
endif

MYCFLAGS=${DEBUGFLAG} ${OPTIMIZEFLAG} --pedantic -Wall ${STOP_ON_WARN} ${CFLAGS}
COMPILE=$(CC) ${MYCFLAGS} -c -o $@ $< && $(ECHO) '  COMPILE' $<

mikktspace.o:	mikktspace.c mikktspace.h Makefile
	$(Q)$(COMPILE) -c mikktspace.c

clean:
	rm -f mikktspace.o

