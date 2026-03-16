# Variables
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
SRCS = main.cpp TCPserver.cpp
NAME = my_server

# The "all" rule - what happens when you just type 'make'
all:
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(NAME)

# The "clean" rule - to delete the executable
clean:
	rm -f $(NAME)