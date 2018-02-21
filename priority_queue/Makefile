TARGET = test
CC = gcc
CFLAGS = -Wall -pedantic -MD -fprofile-arcs -ftest-coverage
LDFLAGS = -lcheck -pthread -lcheck_pic -pthread -lrt -lm -lsubunit -lgcov --coverage

.PHONY: all clean

all: $(TARGET)

$(TARGET): test.o pqueue.o
	$(CC) $^ -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf $(TARGET) *.o *.d

-include *.d
