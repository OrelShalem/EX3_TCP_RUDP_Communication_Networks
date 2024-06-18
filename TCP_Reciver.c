#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/time.h>


#define DEST_IP "127.0.0.1"
#define BUFFER_SIZE 2*1024*1024
#define FILE_SIZE 2*1024*1024

/**
 * @brief Function to print the data of a single file transfer run.
 * This function takes the run count, transfer time, and chunk bandwidth as input and prints the data in a formatted manner.
 *
 * @param run_count The number of the current run.
 * @param transfer_time The time taken for the transfer in milliseconds.
 * @param chunk_bandwidth The bandwidth achieved during the transfer in MB/s.
 *
 * @note This function is used to print the data of each file transfer run.
 */
void printRunData(int run_count, double transfer_time, double chunk_bandwidth) {
   printf("- Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", run_count, transfer_time, chunk_bandwidth);
}

/**
 * @brief Main function of the receiver program.
 *
 * This function is the entry point of the receiver program. It performs the following tasks:
 *  1. Initializes variables.
 *  2. Opens a file for writing ("test.bin").
 *  3. Sets up a TCP socket.
 *  4. Binds the socket to a specified port and listens for incoming connections.
 *  5. Accepts a connection from a sender.
 *  6. Receives data from the sender in a loop, writing it to the file.
 *  7. Calculates and prints statistics for each file transfer run (time and bandwidth).
 *  8. Sends a message ("Hello, World!") to the sender after each file transfer.
 *  9. Checks if the sender wants to continue (based on the received reply).
 * 10. Closes the connection and prints overall statistics after all transfers are complete.
 *
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @return 0 If the program runs successfully, 1 otherwise.
 */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <port_number> <congestion_control_algorithm>\n", argv[0]);
        return 1;
    }

    // Open file for writing the received data
    FILE* fp = fopen("test.bin", "wb");
    if (fp == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    char *message = "Hello, World!"; // Message to send to sender after each transfer
    int sock; // Socket descriptor
    struct sockaddr_in receiver, sender; // Address structures
    socklen_t sender_len = sizeof(sender);

    // Initialize variables for statistics
    int run_count = 0;
    double total_time_sum = 0.0; // Accumulated total time across all runs (seconds)
    double total_bytes_overall = 0.0; // Accumulated total bytes received across all runs

    // Variables used within the inner loop for each file transfer
    double run_time = 0.0; // Time taken for the current file transfer (milliseconds)
    double run_bandwidth = 0.0; // Bandwidth achieved for the current file transfer (MB/s)
    double run_bytes = 0.0; // Total bytes received for the current file transfer

    // Initialize address structures to zero
    memset(&receiver, 0, sizeof(receiver));
    memset(&sender, 0, sizeof(sender));

    // Get port number and congestion control algorithm from command line arguments
    int port_number = atoi(argv[2]);
    char* algo = argv[4];



    // Set socket options
    int opt = 1;
    sock = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket

    if (sock == -1) {
        perror("socket");
        return 1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(sock);
        return 1;
    }

    // Set congestion control algorithm using setsockopt()
    if(setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, sizeof(algo)) < 0){
        perror("setsockopt");
        exit(1);
    }

    // Configure receiver address
    receiver.sin_family = AF_INET;
    inet_pton(AF_INET, DEST_IP, &receiver.sin_addr); 
    receiver.sin_port = htons(port_number);

    // Bind the socket to the specified address
    if (bind(sock, (struct sockaddr *)&receiver, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    // Listen for incoming connections on the bound port
    if (listen(sock, 1) < 0) {
        perror("listen");
        close(sock);
        return 1;
    }

    printf("Starting Receiver...\n");
    printf("Waiting for TCP connection...\n");
    printf("Server is listening on port %d\n", port_number);

    // Accept an incoming connection from a sender
    int sender_sock = accept(sock, (struct sockaddr *)&sender, &sender_len);
    if (sender_sock < 0){
        perror("accept");
        close(sock);
        return 1;
    }
    printf("Sender connected, beginning to receive file...\n");

    // Main loop for handling file transfers
    while (1) {
        char buffer[BUFFER_SIZE] = {0}; // Buffer for receiving data
        char reply[BUFFER_SIZE] = {0}; // Buffer for receiving sender's response
        int flagOpen = 1; // Flag to indicate if the file has been opened for writing

        // Initialize variables for the current file transfer
        double total_transfer_time = 0.0; // Total transfer time for the current file (seconds)
        double total_bytes = 0.0; // Total bytes received for the current file

        // Receive data from the sender in a loop until the file size is reached
        while (total_bytes < FILE_SIZE) {
            struct timeval start_time, end_time; // Time structures for measuring transfer time

            // Get the start time before receiving data
            gettimeofday(&start_time, NULL);

            // Receive data from the sender
            int bytes_received = recv(sender_sock, buffer, BUFFER_SIZE, 0);

            // Check for connection errors
            if (bytes_received == 0) {
                printf("disconnect\n");
                close(sender_sock);
                break;
            }

            // Handle the first data chunk by opening the file for writing
            if (flagOpen == 0) {
                flagOpen = 1;
                fopen("test.bin", "wb"); // Open the file in write binary mode
            }

            // Write the received data to the file
            fwrite(buffer, sizeof(char), bytes_received, fp);

            // Update the total bytes received
            total_bytes += bytes_received;

            // Get the end time after receiving data
            gettimeofday(&end_time, NULL);

            // Calculate the transfer time for this chunk
            double transfer_time_us = (double)(end_time.tv_sec - start_time.tv_sec) * 1000000.0 + (double)(end_time.tv_usec - start_time.tv_usec);
            double transfer_time_s = transfer_time_us / 1000000.0; // Convert to seconds

            // Update the total transfer time
            total_transfer_time += transfer_time_s;
        }

        // Calculate the average time and bandwidth for the current file transfer
        run_time = total_transfer_time * 1000.0; // Convert to milliseconds
                 run_bandwidth = total_bytes / total_transfer_time * 8.0 / (1024.0 * 1024.0); // Convert to MB/s

        // Store the total bytes for this run
        run_bytes = total_bytes;

        // Accumulate total bytes for all runs
        total_bytes_overall += run_bytes;

        // Accumulate total time in seconds
        total_time_sum += run_time / 1000.0;

        // Increment the run count
        run_count++;

        // Print statistics for the current file transfer
        printf("File transfer completed.\n");
        printRunData(run_count, run_time, run_bandwidth);

        // Send a message ("Hello, World!") to the sender
        send(sender_sock, message, sizeof(message), 0);

        // Receive the sender's response
        if (recv(sender_sock, reply, BUFFER_SIZE, 0) < 0) {
            perror("recv");
            close(sender_sock);
            return 1;
        }

        // Check if the sender wants to continue
        if (strcmp(reply, "no") == 0) {
            printf("Waiting for Sender response...\n");
            printf("Sender sent exit message.\n");
            break;
        }
    }

    // Close the sender socket
    close(sender_sock);

    // Calculate overall statistics
    double average_time_overall = total_time_sum * 1000.0; // Convert to milliseconds
    double average_bandwidth_overall = total_bytes_overall / total_time_sum * 8.0 / (1024.0 * 1024.0); // Convert to MB/s

    // Print overall statistics
    printf("\n----------------------------------\n");
    printf("- * Statistics * -\n");
    for (int i = 0; i < run_count; i++) {
        printf("- Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", i + 1, run_time, run_bandwidth);
    }
    printf("-\n");
    printf("- Average time: %.2fms\n", average_time_overall);
    printf("- Average bandwidth: %.6fMB/s\n", average_bandwidth_overall);
    printf("----------------------------------\n");
    printf("Receiver end.\n");

    // Close the main socket
    close(sock);

    return 0;
}


