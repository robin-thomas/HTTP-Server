OBJ   = main.o header.o resource.o
FLAGS = -std=c++11 -pthread
CC    = g++

server.out: $(OBJ)
	@$(CC) -o server.out $(OBJ) $(FLAGS)
	@rm -f $(OBJ)
	@echo "HTTP Server by Robin Thomas successfuly installed!\n"
	@read -p "Press ENTER key to continue to the program ..." ky
	@clear
	@./server.out

main.o: main.cpp header.h
	@clear
	@$(CC) -c main.cpp $(FLAGS)

header.o: header.cpp resource.h
	@$(CC) -c header.cpp $(FLAGS)

resource.o: resource.cpp
	@$(CC) -c resource.cpp $(FLAGS)
