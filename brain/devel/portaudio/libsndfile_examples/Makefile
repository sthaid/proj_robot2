SRC=$(wildcard *.c)
APPS=$(SRC:%.c=%)
SNDLIBS=$(LIBS) $(shell pkg-config --cflags --libs sndfile) -lm

.PHONY: default clean

default: $(APPS)

%: %.c
	$(CC) -Icommon $(CPPFLAGS) $(CFLAGS) -o $@ $< $(SNDLIBS)

clean:
	rm -f $(APPS)
