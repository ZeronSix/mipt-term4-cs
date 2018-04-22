TARGET_CLIENT = client
TARGET_SERVER = server
CC = gcc
CFLAGS = -Wall -pedantic -MD -std=gnu99
LDFLAGS = -pthread

.PHONY: all clean

all: $(TARGET_CLIENT) $(TARGET_SERVER)

$(TARGET_CLIENT): client.o 
	$(CC) $^ -o $(TARGET_CLIENT) $(LDFLAGS)

$(TARGET_SERVER): server.o 
	$(CC) $^ -o $(TARGET_SERVER)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf $(TARGET_CLIENT) $(TARGET_SERVER) *.o *.d

-include *.d
