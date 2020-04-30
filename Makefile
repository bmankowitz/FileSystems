SOURCES=$(wildcard *.c)

OBJECTS=$(SOURCES:.c=.o)

DEPS=$(SOURCES:.c=.d)

BINS=$(SOURCES:.c=)

#I temporarily "fixed" the fread errors by disabling optimization, but the
#best thing to do is find out what is causing the errors in the first place
CFLAGS+= -g -Wall -O0 -Wpedantic -Werror -pthread





all: $(BINS)



.PHONY: clean



clean:

	$(RM) $(OBJECTS) $(BINS)



-include $(DEPS)
