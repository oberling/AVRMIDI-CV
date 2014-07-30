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

# switch mode to either 'debug' or 'release'
MODE = $(shell echo release)
# Place -D or -U options here
ifeq ($(MODE), debug)
CDEFS += -DDEBUG
# this makes our device usable for example with an FTDI adapter
# as those don't support strange midi baud rates :-)
CDEFS += -DBAUD=38400UL
endif
ifeq ($(MODE), release)
# but for the device in production environment we would want
# the real midi baud rate
CDEFS += -DBAUD=31250UL
endif
CDEFS += -DF_CPU=$(F_OSC)
# RINGBUFFER_SIZE must be something 2^n
CDEFS += -DRINGBUFFER_SIZE=32
CDEFS += -DMIDI_CHANNEL=4
CDEFS += -DNUM_PLAY_NOTES=4
CDEFS += -DMIDINOTE_STACK_SIZE=8
CDEFS += -DTRIGGER_COUNTER_INIT=6

CFLAGS += $(CDEFS)

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

