#include "RUDP_API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define FILE_SIZE (2 * 1024 * 1024) // 2MB
#define PACKET_SIZE 59800
#define TIMEOUT 5 

/*
 * @brief A random data generator function based on srand() and rand().
 * @param size The size of the data to generate (up to 2^32 bytes).
 * @return A pointer to the buffer.
 */
char *util_generate_random_data(unsigned int size)
{
    char *buffer = NULL;
    // Argument check.
    if (size == 0)
        return NULL;
    buffer = (char *)calloc(size, sizeof(char));
    // Error checking.
    if (buffer == NULL)
        return NULL;
    // Randomize the seed of the random number generator.
    srand(time(NULL));
    for (unsigned int i = 0; i < size; i++)
        *(buffer + i) = ((unsigned int)rand() % 256);
    return buffer;
}

int main(int argc, char *argv[])
{
    if (argc != 5 || strcmp(argv[1], "-ip") != 0 || strcmp(argv[3], "-p") != 0)
    {
        fprintf(stderr, "Usage: %s -ip <IP> -p <port>\n", argv[0]);
        exit(1);
    }

    const char *ip = argv[2];
    int port = atoi(argv[4]);

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Failed to create UDP socket");
        exit(1);
    }

    struct timeval timeout = {TIMEOUT, 0};

    // Set up destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &(dest_addr.sin_addr)) <= 0)
    {
        perror("Invalid address");
        close(sockfd);
        exit(1);
    }

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
        perror("Failed to set timeout");
        close(sockfd);
        exit(1);
    }

    // Set up RUDP socket
    RUDPConnection *rudp_conn = rudp_socket(&dest_addr, NULL, sockfd);
    if (rudp_conn == NULL)
    {
        fprintf(stderr, "Failed to create RUDP socket\n");
        close(sockfd);
        exit(1);
    }
    printf("RUDP socket created successfully\n");
    printf("RUDP connection created successfully\n");

    // Generate random file data
    char *file_data = util_generate_random_data(FILE_SIZE);
    char* file_buffer = (char*)malloc(PACKET_SIZE*sizeof(char));
    if (file_buffer == NULL)
    {
        perror("Failed to allocate memory");
        rudp_close(rudp_conn);
        close(sockfd);
        exit(1);
    }
    
    if (file_data == NULL)
    {
        perror("Failed to generate random data");
        rudp_close(rudp_conn);
        close(sockfd);
        exit(1);
    }

    int send_again = 1;
    char c;
    while (send_again)
    {
        
        // Send the file
        printf("Sending file...\n");
        int total_bytes_sent = 0;
        while (total_bytes_sent < FILE_SIZE)
        {   
            printf("next_sequence_number before sending: %u\n", rudp_conn->next_sequence_number);
            int bytes_to_send = (FILE_SIZE - total_bytes_sent) < PACKET_SIZE ? (FILE_SIZE - total_bytes_sent) : PACKET_SIZE;
            memcpy(file_buffer, file_data + total_bytes_sent, bytes_to_send);
            
            if (rudp_send(rudp_conn, file_buffer, bytes_to_send, &dest_addr) < 0)
            {
                fprintf(stderr, "Failed to send file\n");
                break;
            }
            total_bytes_sent += bytes_to_send;
            printf("Sent %d bytes\n", total_bytes_sent);
        }
        printf("File sent successfully\n");

        // Ask the user if they want to send the file again
        
        printf("Do you want to send the file again? (y/n): ");
        scanf(" %c", &c);
        if (c == 'y' || c == 'Y')
        {
            char *keep_alive = "keep_alive";
            
            
            if (rudp_send(rudp_conn, keep_alive, sizeof(keep_alive), &dest_addr) < 0){
                fprintf(stderr, "Failed to send keep alive message\n");
                
            }
            printf("Keep alive message sent successfully\n");
            send_again = 1;
            total_bytes_sent = 0;
            
            
        }
        else
        {
            send_again = 0;
            char exit_message[5] = "exit";
            printf("Sending exit message...\n");
            if (rudp_send(rudp_conn, exit_message, sizeof(exit_message), &dest_addr) < 0)
            {
                fprintf(stderr, "Failed to send exit message\n");
            }
            rudp_send_fin(rudp_conn);
            printf("Exit message sent successfully\n");
            break;
        }
    }

    // Clean up
    free(file_data);
    free(file_buffer);
    rudp_close(rudp_conn);
    close(sockfd);

    return 0;
}