CFLAGS := -Wall -W -g -std=c89

cucu: cucu.o
	$(CC) $^ -o $@

cucu.o: cucu.c gen.c

clean:
	rm -f cucu
	rm -f cucu.o

.PHONY: cucu
