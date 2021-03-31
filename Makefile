ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif
SRC = $(wildcard *.c)
OBJECTS = $(SRC:.c=)

all: $(OBJECTS)

%: %.c
	gcc $< -o $@ #`pkg-config --cflags --libs gstreamer-1.0`

install: 2in1screen
	install -d $(PREFIX)/bin
	install -m 755 2in1screen $(PREFIX)/bin
