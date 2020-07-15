NAME = test
SRC = main.cpp

EXTRA_FLAGS = -O3
INCLUDES_FLAGS = -lm -lpthread -lrt -I /home/gleb/Downloads/OO/onload-7.1.0.265/src/lib/ciul/ -I /home/gleb/Downloads/OO/onload-7.1.0.265/src/include
LIBRARIES = /usr/lib/x86_64-linux-gnu/libciul1.a /usr/lib/x86_64-linux-gnu/libonload_zf_static.a

OBJ=$(SRC:.cpp=.o)
CC = g++
CFLAGS = --std=c++1z -Wall -Wextra $(EXTRA_FLAGS) $(INCLUDES_FLAGS)


all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(OBJ) $(LIBRARIES) $(CFLAGS)  -o $(NAME)

%.o: %.cpp $(HDR) $(COMMON_HDR_WITH_DIR)
	$(CC) $(CFLAGS)  -c $< -o $@

fclean: clean
	rm -f $(NAME)

clean:
	rm -f $(OBJ)

re: fclean all
