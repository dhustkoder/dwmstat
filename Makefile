CFLAGS_RELEASE=-Wall -Wextra -Os -march=native
CFLAGS_DEBUG=-O0 -ggdb
CFLAGS=$(CFLAGS_RELEASE)
LIBS=-lX11 -lcurl -ludev

all: config.h dwmstat 

dwmstat: dwmstat.c
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

config.h: config.def.h
	cp config.def.h config.h

install: all
	cp dwmstat /usr/local/bin

uninstall:
	rm /usr/local/bin/dwmstat

clean: 
	rm dwmstat
