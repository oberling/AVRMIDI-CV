CC=gcc
CFLAGS=-Wall
LIBS=-lportmidi
TARGET=main
SOURCES=$(wildcard *.c)
OBJS=$(SOURCES:.c=.o)

all: $(TARGET) $(SOURCES)
	@echo everything up and running - hf.

$(TARGET): $(OBJS)
	@echo Linking $(TARGET)
	$(CC) -o $@ $^ $(LIBS) $(CFLAGS)
	@echo done.

%.o: %.c
	@echo Compiling $<
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	@echo Removing files:
	@-rm -v $(OBJS)
	@-rm -v $(TARGET)
	@echo done.


