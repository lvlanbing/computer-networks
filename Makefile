all: httpd client
LIBS = -lpthread 
httpd: httpd.c
	gcc -W -Wall -o httpd httpd.c -lpthread

client: simpleclient.c
	gcc -W -Wall -o $@ $<
clean:
	rm httpd
