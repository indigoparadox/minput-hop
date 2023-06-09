
# vim: ft=make noexpandtab

SOURCES := src/main.c src/synproto.c
OBJECTS := $(subst .c,.o,$(SOURCES))

minhop32.exe: CFLAGS := -bt=nt -i=$(WATCOM)/h/nt -DMINPUT_OS_WIN32 -we -DDEBUG

minhop16.exe: CFLAGS := -bt=win -i=$(WATCOM)/h/win -DMINPUT_OS_WIN16 -we -DDEBUG

minhop: CFLAGS := -g -fsanitize=address -fsanitize=leak -fsanitize=undefined  -DDEBUG

all: minhop minhop32.exe minhop16.exe

minhop32.exe: $(addprefix obj/win32/,$(OBJECTS)) obj/win32/src/osio_win.o
	wlink system nt name minhop32 lib wsock32 lib winmm fil {$^}

minhop16.exe: $(addprefix obj/win16/,$(OBJECTS)) obj/win16/src/osio_win.o
	wlink system windows name minhop16 lib winsock lib mmsystem fil {$^}

minhop: $(addprefix obj/unix/,$(OBJECTS)) obj/unix/src/osio_unix.o
minhop: $(addprefix obj/unix/,$(OBJECTS)) obj/unix/src/osio_unix.o
	gcc $(CFLAGS) -o "$@" $^

obj/win32/%.o: %.c
	mkdir -p "obj/win32/$(dir $<)"
	wcc386 $(CFLAGS) -fo=$@ $<

obj/win16/%.o: %.c
	mkdir -p "obj/win16/$(dir $<)"
	wcc $(CFLAGS) -fo=$@ $<

obj/unix/%.o: %.c
	mkdir -p "obj/unix/$(dir $<)"
	$(CC) $(CFLAGS) -c -o "$@" "$<"

clean:
	rm -rf obj minhop minhop*.exe

