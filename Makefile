target   := nanoarch
sources  := nanoarch.c
CFLAGS   := -Wall -O2 -g
LFLAGS   := -static-libgcc
LIBS     := -ldl -lglfw3 -lasound -lX11 -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor -pthread -lm
packages := gl glew alsa

# do not edit from here onwards
objects := $(addprefix build/,$(sources:.c=.o))
ifneq ($(packages),)
    LIBS    += $(shell pkg-config --libs-only-l $(packages))
    LFLAGS  += $(shell pkg-config --libs-only-L --libs-only-other $(packages))
    CFLAGS  += $(shell pkg-config --cflags $(packages))
endif

.PHONY: all clean

all: $(target)
clean:
	-rm -rf build
	-rm -f $(target)

$(target): Makefile $(objects)
	$(CC) $(LFLAGS) -o $@ $(objects) $(LIBS)

build/%.o: %.c Makefile
	-mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -MMD -o $@ $<

-include $(addprefix build/,$(sources:.c=.d))

