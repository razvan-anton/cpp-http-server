NAME = my_server
APP_SRCS = $(wildcard *.cpp)
TRACY_SRC = profiler/tracy/public/TracyClient.cpp
APP_OBJS = $(APP_SRCS:.cpp=.o)
TRACY_OBJ = TracyClient.o
OBJS = $(APP_OBJS) $(TRACY_OBJ)
CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -Wshadow -std=c++20 -O3 -g -fno-omit-frame-pointer -isystem profiler/tracy/public
LDFLAGS = -lpthread -ldl

all: $(NAME)

# Dedicated profiling target
profile: CXXFLAGS += -DTRACY_ENABLE
profile: clean $(NAME)

tsan: CXXFLAGS = -Wall -Wextra -Wshadow -std=c++20 -O1 -g -fno-omit-frame-pointer -fsanitize=thread -isystem profiler/tracy/public
tsan: LDFLAGS = -fsanitize=thread -lpthread -ldl
tsan: clean $(NAME)

# 1. Linking Stage
$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(NAME)

# 2. Compilation Stage - Application Sources
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 3. Compilation Stage - Tracy Client
$(TRACY_OBJ): $(TRACY_SRC)
	$(CXX) $(CXXFLAGS) -w -c $< -o $@

.PHONY: all clean profile
clean:
	rm -f $(NAME) $(OBJS)