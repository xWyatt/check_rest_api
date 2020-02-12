CC=gcc
CFLAGS=-lcurl -ljson-c
DEPS=
OBJ=check_rest_api.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

check_rest_api: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
