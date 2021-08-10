FLAGS = -lssl -lcrypto

all:main.cc 
	gcc -c ../common/mongoose.c $(FLAGS) -o mongoose.o
	g++ -c main.cc $(FLAGS) -o main.o 
	g++ main.o mongoose.o $(FLAGS) -o main