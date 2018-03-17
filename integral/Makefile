TARGET = integral
CC = gcc
CFLAGS = -Wall -pedantic -MD
LDFLAGS = -pthread

.PHONY: all clean

all: $(TARGET)

$(TARGET): integral.o
	$(CC) $^ -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf $(TARGET) *.o *.d

-include *.d
