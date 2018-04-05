TARGET = integral
CC = gcc
CFLAGS = -Wall -pedantic -MD -std=gnu99
LDFLAGS = -pthread

.PHONY: all clean

all: $(TARGET)

$(TARGET): integral.o cpuinfo.o
	$(CC) $^ -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf $(TARGET) *.o *.d

-include *.d
