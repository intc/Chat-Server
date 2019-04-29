TARGET = chat_server       # Standard target
TGTDEB = dbg_chat_server   # Debug target
DEFS = "-D_POSIX_C_SOURCE" # Required by strtok_r
LIBS = -lpthread           # This code uses POSIX threads

COMMON_CF = -std=c11 -fomit-frame-pointer -Wall -Wmissing-prototypes\
			-Wstrict-prototypes -Wextra -Wpedantic

CFDBUG = -O0 -ggdb -DDEBUG $(COMMON_CF)
CFLAGS = -O2 -DNDEBUG $(COMMON_CF)

SOURCES = $(wildcard *.c $(LIBDIR)/*.c)
P_OBJECTS=$(SOURCES:.c=_std.o)
D_OBJECTS=$(SOURCES:.c=_debug.o)
HEADERS=$(SOURCES:.c=.h)

CC = gcc

.PHONY: clean all debug

%_std.o: $(SOURCES)
	$(CC) $(DEFS) $(CFLAGS) -c -o $@ $<

%_debug.o: $(SOURCES)
	$(CC) $(DEFS) $(CFDBUG)  -c -o $@ $<

all: $(TARGET)
	@printf "CFLAGS: $(CFLAGS)\n"
	@printf "Standard Binary: $(TARGET)\n"

debug: $(TGTDEB)
	@printf "CFLAGS: $(CFLAGS)\n"
	@printf "Debug Binary: $(TGTDEB)\n"

chat_server: $(P_OBJECTS)
	$(CC) $^ $(LIBS) -o $@

dbg_chat_server: $(D_OBJECTS)
	$(CC) $^ $(LIBS) -o $@

clean:
	$(RM) -rf $(TARGET) $(TGTDEB) \
		*.o\
