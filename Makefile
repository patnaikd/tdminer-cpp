CC=g++
CFLAGS=-Wall -O3 -Wno-sign-compare
LDFLAGS=-lpthread -lm
SOURCES=main.cpp kernel.cpp occurrences.cpp
HEADERS=kernel.h global.h types.h occurrences.h
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=tdminer

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o: $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o $(EXECUTABLE) $(EXECUTABLE).exe

rebuild: clean all
