## Phaser
CC = g++
CFLAGS = -std=c++14 -O0 -g3 -Wall -c 
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
TARGET=phase

all: $(TARGET)


$(TARGET): $(OBJECTS)
	$(CC) -g -o $@ $^ -L../../haplotyperProject/libStatGen -lStatGen_debug -lz 

%.o: %.cpp %.h
	$(CC) $(CFLAGS) -g $< -I ../../haplotyperProject/libStatGen/include/ -I ../../haplotyperProject/genoUtils/
	
	
%.o: %.cpp
	 $(CC) $(CFLAGS) $< -I ../../haplotyperProject/libStatGen/include/ -I ../../haplotyperProject/genoUtils/ 


clean:
	rm -f *.o 