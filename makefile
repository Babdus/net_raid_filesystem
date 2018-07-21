CLIENT = net_raid_client
SERVER = net_raid_server
LIBS = -lm `pkg-config fuse --cflags --libs`
CC = gcc
CFLAGS = -g -Wall -Wextra

.PHONY: default all clean

default: $(CLIENT) $(SERVER)
all: default

COBJECTS = $(wildcard client/*.c) $(wildcard *.c)
CHEADERS = $(wildcard client/*.h) $(wildcard *.h)

SOBJECTS = $(wildcard server/*.c) $(wildcard *.c)
SHEADERS = $(wildcard server/*.h) $(wildcard *.h)

%.o: %.c $(CHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c $(SHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(CLIENT) $(COBJECTS) $(SERVER) $(SOBJECTS)

$(CLIENT): $(COBJECTS)
	$(CC) $(COBJECTS) -Wall -o $@ $(LIBS)

$(SERVER): $(SOBJECTS)
	$(CC) $(SOBJECTS) -Wall -o $@ $(LIBS)

clean:
	-rm -f *.o
	-rm -f $(CLIENT)
	-rm -f $(SERVER)
	-rm -f ../nrf.log
	-rm -f ../error.log

unmount:
	sudo umount -f /home/vagrant/code/personal/final-nrf/mount_dirs/Kosciuszko