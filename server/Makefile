CFLAGS += -Wall -Werror

all: default

default : aesdsocket.o
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.o

aesdsocket.o : aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c 

clean :
	rm -f  aesdsocket aesdsocket.o 
#$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)
