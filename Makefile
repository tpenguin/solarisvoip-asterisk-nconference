#
# nconference Makefile
#
# Port By: Joseph Benden <joe@thrallingpenguin.com>
#
# Financial support for porting provided by Digilink, Inc.
#
# Digilink has provided quality service for over 12 years 
# with a demonstrated rock solid up time of 99.995%. We 
# give the highest level of service at the lowest price.
#
#       http://www.digilink.net/
#
#

.EXPORT_ALL_VARIABLES:

MODS=app_nconference.so
CC=gcc

ifeq ($(NOISY_BUILD),)
   ECHO_PREFIX=@
   CMD_PREFIX=@
else
   ECHO_PREFIX=@\#
   CMD_PREFIX=
endif

CFLAGS+=-fPIC -pipe -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -g -Iinclude -I../include -D_REENTRANT -D_GNU_SOURCE -O6  -DAST_MODULE=\"app_nconference\"
CFLAGS+=-I../asterisk
CFLAGS+=-I../asterisk/include
CFLAGS+=-D_GNU_SOURCE
SOLINK+=-shared -fpic

OSARCH=$(shell uname -s)
ifeq ($(OSARCH),SunOS)
 CFLAGS+=-Wcast-align -DSOLARIS
 CFLAGS+=-I../asterisk/include/solaris-compat
 SOLINK+=-lrt
 ifeq ($(OSCPU),sun4u)
  CFLAGS=+-mcpu=ultrasparc
 endif
endif

INSTALL=install
INSTALL_PREFIX=
ifeq ($(OSARCH),SunOS)
	ASTLIBDIR=$(INSTALL_PREFIX)/opt/asterisk/lib
else
	ASTLIBDIR=$(INSTALL_PREFIX)/usr/lib/asterisk
endif
MODULES_DIR=$(ASTLIBDIR)/modules
all: depend $(MODS)

install: all
	for x in $(MODS); do $(INSTALL) -m 755 $$x $(MODULES_DIR) ; done

clean:
	$(ECHO_PREFIX) echo "   [--] -> $@"
	$(CMD_PREFIX) rm -f *.so *.o .depend

app_nconference.so: frame.o conference.o member.o sounds.o dtmf.o vad.o cli.o app_nconference.o 
	$(ECHO_PREFIX) echo "   [LD] $^ -> $@"
	$(CMD_PREFIX) $(CC) $(SOLINK) -o $@ frame.o conference.o member.o sounds.o dtmf.o vad.o cli.o app_nconference.o

%.o : %.c
	$(ECHO_PREFIX) echo "   [CC] $< -> $@"
	$(CMD_PREFIX) $(CC) -c -o $@ $(CFLAGS) $<

ifneq ($(wildcard .depend),)
include .depend
endif

depend: .depend

.depend:
	$(ECHO_PREFIX) echo "   [--] -> $@"
	$(CMD_PREFIX) ./mkdep $(CFLAGS) `ls *.c`

