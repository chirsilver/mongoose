FLAGS = -lssl -lcrypto -DM_SSL

all:main.cc 
	g++ -c main.cc $(FLAGS) -o main.o 
	g++ main.o mongoose.o $(FLAGS) -o main