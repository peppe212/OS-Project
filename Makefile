CC = gcc
CFLAGS = -std=c99 -Wall -pedantic
OPTFLAGS = -g -O3
LIBS = -lpthread
MACRO = BUILD2

DIROBJS = server.o shared.o
MRKTOBJS = client.o shared.o queue.o
SOCKNAME = mysock
TARGETS = server server2 client


.PHONY: all clean test1 test2
.SUFFIXES: .c .h


#target di default
all: 			$(TARGETS)


%.o:			%.c
			$(CC) $(CFLAGS) $(OPTFLAGS) -c $<


server: 		$(DIROBJS)
			$(CC) $(CFLAGS) $(OPTFLAGS) $^ -o $@ $(LIBS)


server2:		server.c shared.c
			$(CC) $(CFLAGS) $(OPTFLAGS) -D$(MACRO) $^ -o $@ $(LIBS)


client: 		$(MRKTOBJS)
			$(CC) $(CFLAGS) $(OPTFLAGS) $^ -o $@ $(LIBS)		
			
		
clean: 
			@echo "Removing all files"
			-rm -f $(TARGETS) $(DIROBJS) $(MRKTOBJS) $(SOCKNAME) *.log *~				
			
			
test1:			$(TARGETS)
			@echo "Sto eseguendo test1..."
			@timeout --foreground -s SIGQUIT 15 ./server &
			@valgrind --leak-check=full ./client
			@echo "Test 1 terminato!"
			@chmod +x ./analisi.sh
			./analisi.sh resoconto.log
				
				
test2: 			$(TARGETS)	
			@echo "Sto eseguendo test2..."
			@timeout --preserve-status -s SIGHUP 25 ./server2 config2.txt
			@echo "Test 2 terminato!"
			@chmod +x ./analisi.sh
			./analisi.sh resoconto2.log
				
