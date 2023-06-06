
# vim: ft=make noexpandtab

CFLAGS := -g -fsanitize=address -fsanitize=leak -fsanitize=undefined 

minhop: src/main.c
	gcc $(CFLAGS) -o "$@" $<

%.o: %.c
	gcc $(CFLAGS) -c -o "$@" "$<"

