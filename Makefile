CFLAGS := -Wall -W -g

cucu: cucu.o
	$(CC) $^ -o $@

clean:
	rm -f cucu
	rm -f cucu.o

.PHONY: cucu
