#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/tcp.h>


char *util_generate_random_data(unsigned int);

#define DATA_SIZE 2*1024*1024
#define DEST_IP "127.0.0.1"
#define DEST_PORT 5678
#define BUFFER_SIZE 2*1024*1024

int main(int argc, char *argv[])
{
      if (argc < 6) {
        printf("Usage: %s <port_number>\n", argv[0]);
        return 1;
    }
	printf("sender\n");
	char *random_data = util_generate_random_data(DATA_SIZE);

    char buffer[BUFFER_SIZE] = {0};

    int port_number = atoi(argv[4]);
    char* dest_ip = argv[2];
    char* algo = argv[6];
    

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in receiver;

	memset(&receiver, 0, sizeof(receiver));

	receiver.sin_family = AF_INET;
	inet_pton(AF_INET, dest_ip, &receiver.sin_addr);
	receiver.sin_port = htons(port_number);

    if(setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, sizeof(algo)) < 0){
        perror("setsockopt");
        exit(1);
    }

    int ret = connect(sock, (struct sockaddr *)&receiver, sizeof(receiver));
    if (ret < 0) {
        perror("connect error");
        exit(1);
    }

	
    char choice;
    do
    {   int total_bytes_sent = 0;
        while (total_bytes_sent < DATA_SIZE)
        {
            int bytes = send(sock, random_data, strlen(random_data), 0);
        if(bytes < 0){
            perror("send");
            exit(1);
        }
        printf("Sent %d bytes\n", bytes);
        total_bytes_sent += bytes;
        }
        
        
        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if(bytes_received < 0){
            perror("recv");
            exit(1);
        }
        if(buffer[BUFFER_SIZE-1] != '\0'){
            buffer[BUFFER_SIZE-1] = '\0';
        }

        printf("Received: %s\n", buffer);
    
        printf("Enter choice if send again: \n");
        scanf(" %c",&choice);
        if(choice == 'n'){
            send(sock, "no", 2, 0);
            break;
        }
        else if(choice == 'y'){
            send(sock, "yes", 3, 0);
        }
        else if(choice != 'y' && choice!= 'n'){
            printf("Invalid choice, enter y or n\n");
            scanf(" %c",&choice);
        }
    } while ( choice == 'y');
    

	close(sock);

	return EXIT_SUCCESS;
}

char *util_generate_random_data(unsigned int size)
{
	char *buffer = NULL;

	if (size == 0)
	{
		return NULL;
	}

	buffer = (char *)calloc(size, sizeof(char));

	if (buffer == NULL)
	{
		return NULL;
	}

	srand(time(NULL));

	for (unsigned int i = 0; i < size; i++)
	{
		*(buffer + i) = ((unsigned int)rand() % 256);
	}

	return buffer;
}