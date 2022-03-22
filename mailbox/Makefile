#embedded linux course

CC:=${CROSS_COMPILE}gcc
CFLAGS=-O0 -Wall
LIBS=-lpthread
INCLUDE=-I include/
SRC_PATH=src/
OBJ_PATH=build/

SRC=$(wildcard $(SRC_PATH)*.c)
SRC_TMP=$(notdir $(SRC))
OBJS_TMP=$(SRC_TMP:.c=.o)
OBJS=$(addprefix $(OBJ_PATH), $(OBJS_TMP))

all: make_dir app

make_dir:
	test -d $(OBJ_PATH) || mkdir $(OBJ_PATH)

$(OBJ_PATH)%.o: $(SRC_PATH)%.c
	$(CC) $(CFLAGS) -c $(INCLUDE) $< -o $@

app: $(OBJS)
	$(CC) $^ -o $(OBJ_PATH)$@ $(LIBS)

clean:
	rm -r $(OBJ_PATH)

.PHONY: all clean make_dir

