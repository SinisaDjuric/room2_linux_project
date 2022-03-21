#embedded linux course

CC:=gcc
CFLAGS = -O0 -Wall
LIBS = -lpthread

SRC=$(wildcard *.c)
OBJS=$(SRC:.c=.o)

all: app

%.o: %.c
	${CC} $(CFLAGS) -c $< -o $@

app: ${OBJS}
	${CC} $^ -o $@ $(LIBS)

clean:
	rm -f *.o app

.PHONY: all clean

