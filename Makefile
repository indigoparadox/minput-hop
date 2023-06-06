
# vim: ft=make noexpandtab

SOURCES := src/main.c
OBJECTS := $(subst .c,.o,$(SOURCES))

minhop32.exe: CFLAGS := -bt=nt -i=$(WATCOM)/h/nt -DWIN32

minhop: CFLAGS := -g -fsanitize=address -fsanitize=leak -fsanitize=undefined 

all: minhop minhop32.exe

minhop32.exe: $(addprefix obj/win32/,$(OBJECTS))
	wlink system nt name minhop32 fil {$^}

minhop: $(addprefix obj/unix/,$(OBJECTS))
	gcc $(CFLAGS) -o "$@" $^

obj/win32/%.o: %.c
	mkdir -p "obj/unix/$(dir $<)"
	wcc386 $(CFLAGS) -fo=$@ $<

obj/unix/%.o: %.c
	mkdir -p "obj/unix/$(dir $<)"
	$(CC) $(CFLAGS) -c -o "$@" "$<"

clean:
	rm -rf obj minhop

