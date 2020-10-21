CFLAGS := -Wall -O2

server: client.o client_cmds.o io_utils.o main.o path_utils.o
	$(LD) -o $@ $^ -lc -lpthread

clean:
	$(RM) server *.o

.PHONY: clean
