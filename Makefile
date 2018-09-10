CXX=gcc
all: b
b:
	$(CXX) blech.c -lbluetooth -lpthread -D_GNU_SOURCE -o b -Wall -Wextra -O3 -std=c99
	strip b
bt:
	$(CXX) blech.c -lbluetooth -lpthread -D_GNU_SOURCE -o b -Wall -Wextra -O3 -std=c99 -DTEST
	strip b

clean:
	rm -f b
