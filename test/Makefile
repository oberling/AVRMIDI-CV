# Hey Emacs, this is a -*- makefile -*-

TARGET = test

SRCDIR = ../src/
INCDIR = ../inc/
SOURCES = ../src/midibuffer.c \
	  ../src/midinote_stack.c \
	  ../src/polyphonic.c \
	  ../src/ringbuffer.c \
	  ../src/unison.c \
	  test.c

OBJS = $(SOURCES:.c=.o)

CC = gcc -g
CFLAGS = -I$(INCDIR)

all: $(TARGET) $(SOURCES)
	@echo everything built!

$(TARGET): $(OBJS)
	@echo Linking $(TARGET)...
	$(CC) -o $@ $^ $(LIBS) $(CFLAGS)
	@echo done.

%.o: %.cc
	@echo Compiling $<
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	@echo Removing files:
	@-rm -v $(OBJS)
	@-rm -v $(TARGET)
	@echo done.

