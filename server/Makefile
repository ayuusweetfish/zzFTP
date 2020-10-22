CFLAGS := -Wall -O2

server: $(patsubst %.c, %.o, $(wildcard *.c))
	$(LD) -o $@ $^ -lc -lpthread

clean:
	$(RM) server *.o

.PHONY: clean
