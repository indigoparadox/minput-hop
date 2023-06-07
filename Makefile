
# vim: ft=make noexpandtab

SOURCES := src/main.c src/synproto.c
OBJECTS := $(subst .c,.o,$(SOURCES))

minhop32.exe: CFLAGS := -bt=nt -i=$(WATCOM)/h/nt -DMINPUT_OS_WIN

minhop: CFLAGS := -g -fsanitize=address -fsanitize=leak -fsanitize=undefined 

all: minhop minhop32.exe

minhop32.exe: $(addprefix obj/win32/,$(OBJECTS)) obj/win32/src/osio_win.o
	wlink system nt name minhop32 lib wsock32 fil {$^}

minhop: $(addprefix obj/unix/,$(OBJECTS)) obj/unix/src/osio_unix.o
	gcc $(CFLAGS) -o "$@" $^

obj/win32/%.o: %.c
	mkdir -p "obj/win32/$(dir $<)"
	wcc386 $(CFLAGS) -fo=$@ $<

obj/unix/%.o: %.c
	mkdir -p "obj/unix/$(dir $<)"
	$(CC) $(CFLAGS) -c -o "$@" "$<"

clean:
	rm -rf obj minhop

