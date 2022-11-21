#

TARGET	= seamagic
CC			= gcc
CFLAGS	= -g -O2 -Wall -pthread -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LIBS		= -lrt -lsodium

HEADERS = $(wildcard *.h)
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

.PHONY: all clean

all: $(TARGET) clean

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -f *.o
