
# vim: ft=make noexpandtab

SOURCES := src/main.c src/synproto.c src/netio.c
OBJECTS := $(subst .c,.o,$(SOURCES))

CFLAGS_WATCOM_W32 := -bt=nt -i=$(WATCOM)/h/nt -DMINPUT_OS_WIN32 -hw $(DEFINES)
CFLAGS_WRC_W32 := -DMINPUT_OS_WIN32
RES_PATH_W32 := obj/win32/src/minhop.res

CFLAGS_WATCOM_W16 := -bt=windows -2 -ms -i=$(WATCOM)/h/win -hw -ecc -DMINPUT_OS_WIN16 $(DEFINES)
CFLAGS_WRC_W16 := -DMINPUT_OS_WIN16
RES_PATH_W16 := obj/win16/src/minhop.res

CFLAGS_GCC := $(DEFINES)

ifneq ("$(BUILD)", "RELEASE")
	CFLAGS_WATCOM_W32 += -we -DDEBUG -d3 -db -hc
	CFLAGS_WATCOM_W16 += -we -DDEBUG -d3 -db -hc
	CFLAGS_WRC_W32 += -DDEBUG
	CFLAGS_WRC_W16 += -DDEBUG
	CFLAGS_GCC += -g -fsanitize=address -fsanitize=leak -fsanitize=undefined  -DDEBUG -Werror -Wall -DDEBUG_PROTO_CLIP
	LDFLAGS_WATCOM_W16 += debug codeview
	LDFLAGS_WATCOM_W32 += debug codeview
endif

all: minhop minhop32.exe minhop16.exe

minhop32.exe: $(addprefix obj/win32/,$(OBJECTS)) obj/win32/src/osio_win.o | $(RES_PATH_W32)
	wlink system nt_win name minhop32 $(LDFLAGS_WATCOM_W16) lib wsock32 lib winmm fil {$^} option resource $(RES_PATH_W32)

minhop16.exe: $(addprefix obj/win16/,$(OBJECTS)) obj/win16/src/osio_win.o | $(RES_PATH_W16)
	wlink system windows name minhop16 $(LDFLAGS_WATCOM_W16) lib winsock lib mmsystem fil {$^} option resource $(RES_PATH_W16)

minhop: $(addprefix obj/unix/,$(OBJECTS)) obj/unix/src/osio_unix.o
	gcc $(CFLAGS_GCC) -o "$@" $^

$(RES_PATH_W32): src/minhop.rc
	wrc $(CFLAGS_WRC_W32) -r $< -bt=nt -fo=$@

$(RES_PATH_W16): src/minhop.rc
	wrc $(CFLAGS_WRC_W16) -r $< -bt=windows -fo=$@

obj/win32/%.o: %.c
	mkdir -p "obj/win32/$(dir $<)"
	wcc386 $(CFLAGS_WATCOM_W32) -fo=$@ $<

obj/win16/%.o: %.c
	mkdir -p "obj/win16/$(dir $<)"
	wcc $(CFLAGS_WATCOM_W16) -fo=$@ $<

obj/unix/%.o: %.c
	mkdir -p "obj/unix/$(dir $<)"
	$(CC) $(CFLAGS_GCC) -c -o "$@" "$<"

clean:
	rm -rf obj minhop minhop*.exe

