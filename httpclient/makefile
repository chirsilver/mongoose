CC = g++
CPPFLAGS = -Wall -g
TARGET = main
.PHONY = clean



$(TARGET): mongoose.o main.o http_client.o
	@$(CC) $^ -o $@ $(CPPFLAGS)
	@echo "BUILD $@ \033[01;32m OK \033[0m"

%.o:%.cpp
	@$(CC) -c $< -o $@ $(CPPFLAGS)
	@echo "COMPILE $< \033[01;32m OK \033[0m"

clean:
	@rm main.o http_client.o