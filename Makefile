CC=gcc
CFLAGS=-Wall -Werror
LIBS=-lcurl -ljson-c
DEPS=check_rest_api.h
OBJ=check_rest_api.c 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o *~ core
