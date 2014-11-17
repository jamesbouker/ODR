CC=gcc
FLAGS=-g -O2 -Wall -Werror
CFLAGS = ${FLAGS} -I/home/stufs1/cse533/Stevens/unpv13e/lib
EXE=odr_$(SELF) server_$(SELF) client_$(SELF)
EVERYTHINGELSE=uds.h prhwaddrs.h shared.h
SELF=jbouker

all: $(EXE)

server_$(SELF): server.o
	gcc $(CFLAGS) -o $@ $^

client_$(SELF): client.o
	gcc $(CFLAGS) -o $@ $^

odr_$(SELF): odr.o prhwaddrs.o
	gcc $(CFLAGS) -o $@ $^

%.o: %.c %.h $(EVERYTHINGELSE)
	gcc $(CFLAGS) -c $^

clean:
	rm -f *.o *.out *.gch $(EXE)

#use these to easily deploy, remove, start, and kill the executables

deploy:
	~/cse533/deploy_app server_$(SELF)
	~/cse533/deploy_app client_$(SELF)
	~/cse533/deploy_app odr_$(SELF)

deployServer:
	~/cse533/deploy_app server_$(SELF)

deployClient:
	~/cse533/deploy_app client_$(SELF)

deployOdr:
	~/cse533/deploy_app odr_$(SELF)

startServer:
	~/cse533/start_app server_$(SELF)

startOdr:
	~/cse533/start_app odr_$(SELF)

kill: 
	~/cse533/kill_apps

cleanup: 
	~/cse533/cleanup_vms
