NAME = my_server
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
CXXFLAGS = -Wall -Wextra -Werror -Wshadow -std=c++20 -O3

# 1. Linking Stage
$(NAME): $(OBJS)
	$(CXX) $(OBJS) -o $(NAME)

# 2. Compilation Stage (Pattern Rule)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: all clean
clean: 
	rm -f $(NAME) $(OBJS)