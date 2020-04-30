SOURCES=$(wildcard *.c)

OBJECTS=$(SOURCES:.c=.o)

DEPS=$(SOURCES:.c=.d)

BINS=$(SOURCES:.c=)


#PUT THESE BACK IN ASAP-Wpedantic -Werror
CFLAGS+= -g -Wall -O1  -pthread





all: $(BINS)



.PHONY: clean



clean:

	$(RM) $(OBJECTS) $(BINS)



-include $(DEPS)
