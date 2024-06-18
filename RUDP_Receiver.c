#include "RUDP_API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#define FILE_SIZE (2 * 1024 * 1024) // 2MB
#define CONTROL_MSG_SIZE 100

int main(int argc, char *argv[])
{
    if (argc != 3 || strcmp(argv[1], "-p") != 0)
    {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[2]);

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Set up client address
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(0);

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Error setting socket option");
        exit(1);
    }

    // Bind the socket to the server address
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding socket");
        exit(1);
    }

    // Set up RUDP socket
    RUDPConnection *rudp_conn = rudp_socket(&server_addr, &client_addr, sockfd);
    if (rudp_conn == NULL)
    {
        fprintf(stderr, "Failed to create RUDP socket\n");
        exit(1);
    }

    printf("Starting Receiver...\n");
    printf("Waiting for RUDP connection...\n");

    // Receive the file
    char file_data[FILE_SIZE];
    clock_t start_time, end_time;
    int num_runs = 0;
    double total_time = 0;
    double total_bandwidth = 0;
    double *time_taken = NULL;
    double *bandwidth = NULL;
    int current_run = 0;

    FILE *fp = fopen("RUDP_file.bin", "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening file\n");
        rudp_close(rudp_conn);
        exit(1);
    }

    while (1)
    {
        fclose(fp);
        fp = fopen("RUDP_file.bin", "wb");
        if (fp == NULL)
        {
            fprintf(stderr, "Error opening file\n");
            rudp_close(rudp_conn);
            exit(1);
        }
        int total_bytes_received = 0;

        start_time = clock();
        while (total_bytes_received < FILE_SIZE)
        {
            if (current_run == 0)
            {
                time_taken = malloc(sizeof(double) * num_runs);
                bandwidth = malloc(sizeof(double) * num_runs);
            }
            // Receive file data

            ssize_t bytes_received = rudp_recv(rudp_conn, file_data, FILE_SIZE, &rudp_conn->sender_addr);

            if (bytes_received < 0)
            {
                fprintf(stderr, "Error receiving file\n");
                rudp_close(rudp_conn);
                exit(1);
            }
            // printf("size received: %ld\n", bytes_received);
            fwrite(file_data, sizeof(char), bytes_received, fp);
            total_bytes_received += bytes_received;
        }
        end_time = clock();

        // calculate time taken in milliseconds
        time_taken[current_run] = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000;
        printf("Time taken: %.2fms\n", time_taken[current_run]);

        total_time += time_taken[current_run];
        double total_mb = (double)total_bytes_received / 1024 / 1024;
        bandwidth[current_run] = total_mb / (time_taken[current_run] / 1000); // in MB/s
        printf("Bandwidth: %.2fMB/s\n", bandwidth[current_run]);
        total_bandwidth += bandwidth[current_run];
        printf("File transfer completed.\n");
        num_runs++;
        current_run++;
        printf("Waiting for control message...\n");
        char control_msg[1024] = {0};
        ssize_t msg_size = rudp_recv(rudp_conn, control_msg, sizeof(control_msg), &rudp_conn->sender_addr);
        if (msg_size < 0)
        {
            fprintf(stderr, "Error receiving control message\n");
            rudp_close(rudp_conn);
            exit(1);
        }
        printf("Control message received: %s\n", control_msg);

        if (strcmp(control_msg, "exit") == 0)
        {
            printf("Sender sent exit message.\n");
            rudp_recv_fin(rudp_conn);
            break;
        } 

    }

    // Calculate average time and bandwidth

    printf("File transfer completed.\n");

    printf("----------------------------------\n");
    printf("- * Statistics * -\n");
    for (int i = 0; i < num_runs; i++)
    {
        printf("Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", i + 1, time_taken[i], bandwidth[i]);
    }
    double avg_time = total_time / num_runs;
    double avg_bandwidth = total_bandwidth / num_runs;
    printf("- Average time: %.2fms\n", avg_time);
    printf("- Average bandwidth: %.2fMB/s\n", avg_bandwidth);
    printf("----------------------------------\n");

    // Close the socket
    rudp_close(rudp_conn);

    return 0;
}