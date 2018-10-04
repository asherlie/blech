CXX=gcc
all: b
b:
	$(CXX) blech_exp.c peer_list.c -lpthread -D_GNU_SOURCE -o b -Wall -Wextra -O3 -std=c99
	strip b
db:
	$(CXX) blech_exp.c peer_list.c -lpthread -D_GNU_SOURCE -o b -Wall -Wextra -std=c99 -DDEBUG -g
clean:
	rm -f b
