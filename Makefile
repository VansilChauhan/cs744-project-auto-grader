all: server.c client.c
	gcc server.c -o server.o -lpthread
	gcc client.c -o client.o -lpthread

clean:
	rm -f *.o