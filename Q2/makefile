compile: server.c client.c relay.c log.o packet.h
	gcc -o server server.c log.o && gcc -o client client.c log.o && gcc -o relay relay.c log.o

log.o: packet.h log.c
	gcc -c log.c

clean:
	rm *.o && rm client && rm server && rm relay