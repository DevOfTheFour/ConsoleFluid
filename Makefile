CC ?= gcc
CFLAGS = -std=c99 -O2 -Wall -Wextra
LDLIBS = -lm -luser32

TARGET = fluid.exe
SOURCE = fluid_win.c

all:
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET) $(LDLIBS)

clean:
	del /Q $(TARGET) 2>NUL || exit 0