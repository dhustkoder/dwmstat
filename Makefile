CFLAGS=-Wall -Wextra -Os
LIBS=-lX11 -lcurl

all: dwmstat

dwmstat: dwmstat.c
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean: 
	rm dwmstat
