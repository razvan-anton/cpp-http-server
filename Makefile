# Variables
CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -g -std=c++17
SRCS = $(wildcard *.cpp) # all .cpp files go here
NAME = my_server

# The "all" rule - what happens when you just type 'make'
all:
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(NAME)

# The "clean" rule - to delete the executable
clean:
	rm -f $(NAME)