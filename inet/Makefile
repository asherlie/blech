CXX=gcc
all: b
b:
	$(CXX) blech_exp.c peer_list.c net.c snd.c -lpthread -D_GNU_SOURCE -o b -Wall -Wextra -pedantic -Wno-unused-variable -O3 -std=c99
	strip b
db:
	$(CXX) blech_exp.c peer_list.c net.c snd.c -lpthread -D_GNU_SOURCE -o b -Wall -Wextra -pedantic -Wno-unused-variable -std=c99 -DDEBUG -g
clean:
	rm -f b
