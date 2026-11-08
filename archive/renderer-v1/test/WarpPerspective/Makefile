# Makefile

CC = gcc #-fsanitize=address
CPPFLAGS = #-MMD
CFLAGS = -Wall -Wextra -O3 `pkg-config --cflags sdl2 SDL2_image`
LDFLAGS =
LDLIBS = `pkg-config --libs sdl2 SDL2_image` -lm


SRC = warpPerspective.c main.c
DEP= ${SRC:.c=.d}
PRG= ${SRC:.c=}

all: main

main: ${SRC:.c=.o}

-include ${DEP}

.PHONY: clean

clean:
	rm -f *.o
	rm -f ${DEP}
	rm -f ${PRG}

re: clean all

# END
