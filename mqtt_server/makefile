FLAG = -Wall -g -lssl -lcrypto -DMG_ENABLE_OPENSSL=1 -DDEBUG_
PROG ?= server

all: mongoose.o 
	g++ -c $(PROG).cc $(FLAG) -o $(PROG).o 
	g++ $(PROG).o mongoose.o $(FLAG) -o $(PROG) 
	rm *.o
mongoose.o: 
	gcc -c ../common/mongoose.c $(FLAG) -o mongoose.o

