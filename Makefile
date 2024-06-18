# Use the gcc compiler.
CC = gcc

# Flags for the compiler.
CFLAGS = -Wall -g -Wextra -Werror -std=c99 -pedantic

# Command to remove files.
RM = rm -f

# Phony targets - targets that are not files but commands to be executed by make.
.PHONY: all default clean runtsr runtcr runtsc runtcc runus runuc

# Default target - compile everything and create the executables and libraries.
all: TCP_Reciver TCP_Sender RUDP_Receiver RUDP_Sender

# Alias for the default target.
default: all

############
# Programs #
############

# Compile the tcp server.
TCP_Reciver: TCP_Reciver.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile the tcp client.
TCP_Sender: TCP_Sender.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile the rudp server.
RUDP_Receiver: RUDP_Receiver.o RUDP_API.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile the rudp client.
RUDP_Sender: RUDP_Sender.o RUDP_API.o
	$(CC) $(CFLAGS) -o $@ $^

################
# Run programs #
################

# Run tcp server.
runtsr: TCP_Reciver
	./TCP_Reciver -p 5678 -algo reno

# Run tcp client.
runtcr: TCP_Sender
	./TCP_Sender -ip "127.0.0.1" -p 5678 -algo reno

# Run tcp server.
runtsc: TCP_Reciver
	./TCP_Reciver -p 5678 -algo cubic

# Run tcp client.
runtcc: TCP_Sender
	./TCP_Sender -ip "127.0.0.1" -p 5678 -algo cubic

# Run rudp server.
runus: RUDP_Receiver
	./RUDP_Receiver -p 5678

# Run rudp client.
runuc: RUDP_Sender
	./RUDP_Sender -ip "127.0.0.1" -p 5678

################
# Object files #
################

# Compile all the C files into object files.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#################
# Cleanup files #
#################

# Remove all the object files, shared libraries and executables.
clean:
	$(RM) *.o *.so TCP_Reciver TCP_Sender RUDP_Sender RUDP_Receiver