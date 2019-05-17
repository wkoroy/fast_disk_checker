APP = ./fast_disk_checker

GIT_VERSION := $(shell git describe --abbrev=16 --dirty --always --tags)
CC=g++ -msse -g  -DTESTING -DVERSION=\"$(GIT_VERSION)\"  -std=c++11
CFLAGS=-ldl -g -lm -lrt -lpthread -Wall -Wextra -lstdc++ 

all: $(APP)


$(APP): fast_disk_checker.o
	$(CC)   fast_disk_checker.o -o $(APP) $(CFLAGS)


fast_disk_checker.o: fast_disk_checker.cpp 	
	$(CC) fast_disk_checker.cpp -c -Wall -lstdc++
clean:
	rm -f *.o ; rm $(APP)
