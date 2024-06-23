#include "RUDP_API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_RETRANSMISSION_COUNT 30
#define PACKET_HISTORY_SIZE 10

// Array to store the history of recently sent packets
RUDPPacket packet_history[PACKET_HISTORY_SIZE];
int history_index = 0;

unsigned short int calculate_checksum(void *data, unsigned int bytes);



/**
 * @brief A function to create a new RUDP connection.
 * @param receiver_addr The address of the receiver.
 * @param sender_addr The address of the sender.
 * @param sockfd The socket file descriptor.
 * @return A pointer to the newly created RUDPConnection.
 * @note This function handles the three-way handshake for establishing a connection.
 * @note It sends a SYN packet, waits for a SYN-ACK packet, and then sends an ACK packet.
 * @note If any error occurs during the handshake, it prints an error message and exits.
 */
RUDPConnection *rudp_socket(struct sockaddr_in *receiver_addr, struct sockaddr_in *sender_addr, int sockfd)
{   
    
    RUDPConnection *connection = (RUDPConnection *)malloc(sizeof(RUDPConnection));// Allocate memory for the RUDPConnection
    if (connection == NULL)
    {
        perror("Failed to allocate memory for RUDPConnection");
        exit(1);
    }

    connection->sockfd = sockfd;// Store the socket file descriptor
    connection->receiver_addr = *receiver_addr;// Store the receiver's address
    if (sender_addr != NULL)
    {
        connection->sender_addr = *sender_addr;
    }
    connection->next_sequence_number = 1;// Set the next sequence number to 1

    if (sender_addr == NULL)
    {
        // Sender side
        connection->sender_addr = *receiver_addr; // Store the receiver's address as the sender's address

        RUDPPacket syn_packet;// Create a SYN packet
        syn_packet.length = htons(RUDP_HEADER_SIZE);// Set the length of the packet
        syn_packet.header.checksum = 0;// Set the checksum to 0
        syn_packet.header.flags.SYN = 1;// Set the SYN flag
        syn_packet.header.checksum = htons(calculate_checksum((char *)&syn_packet, sizeof(RUDPPacket)));// Calculate the checksum
        printf("Sending SYN packet with checksum: %u\n", ntohs(syn_packet.header.checksum));// Print the checksum

        if (sendto(sockfd, &syn_packet, RUDP_HEADER_SIZE, 0, (struct sockaddr *)receiver_addr, sizeof(struct sockaddr_in)) < 0)// Send the SYN packet
        {
            perror("Error sending SYN packet");
            free(connection);
            exit(1);
        }

        RUDPPacket synack_packet;// Create a SYN-ACK packet
        struct sockaddr_in synack_sender_addr;
        socklen_t synack_sender_addr_len = sizeof(synack_sender_addr);
        while (1)
        {
            if (recvfrom(sockfd, &synack_packet, sizeof(RUDPPacket), 0, (struct sockaddr *)&synack_sender_addr, &synack_sender_addr_len) < 0)
            {
                perror("Error receiving SYN-ACK packet");
                free(connection);
                exit(1);
            }

            if (synack_packet.header.flags.SYN == 1 && synack_packet.header.flags.ACK == 1)
            {
                printf("Received SYN-ACK packet with checksum: %u\n", ntohs(synack_packet.header.checksum));
                if (verify_checksum(&synack_packet, sizeof(RUDPPacket), synack_packet.header.checksum) == 0)
                {
                    break;
                }
            }
        }

        RUDPPacket ack_packet;// Create an ACK packet
        ack_packet.length = htons(RUDP_HEADER_SIZE);
        ack_packet.header.checksum = 0;
        ack_packet.header.flags.ACK = 1;
        ack_packet.header.checksum = htons(calculate_checksum((char *)&ack_packet, sizeof(RUDPPacket)));
        printf("Sending ACK packet with checksum: %u\n", ntohs(ack_packet.header.checksum));

        if (sendto(sockfd, &ack_packet, RUDP_HEADER_SIZE, 0, (struct sockaddr *)&synack_sender_addr, sizeof(struct sockaddr_in)) < 0)
        {
            perror("Error sending ACK packet");
            free(connection);
            exit(1);
        }
    }
    else
    {
        // Receiver side
        RUDPPacket syn_packet;// Create a SYN packet
        struct sockaddr_in syn_sender_addr;
        socklen_t syn_sender_addr_len = sizeof(syn_sender_addr);
        while (1)// Wait for a SYN packet
        {
            if (recvfrom(sockfd, &syn_packet, sizeof(RUDPPacket), 0, (struct sockaddr *)&syn_sender_addr, &syn_sender_addr_len) < 0)
            {
                perror("Error receiving SYN packet");
                free(connection);
                exit(1);
            }

            if (syn_packet.header.flags.SYN == 1)
            {
                printf("Received SYN packet with checksum: %u\n", ntohs(syn_packet.header.checksum));
                if (verify_checksum(&syn_packet, sizeof(RUDPPacket), syn_packet.header.checksum) == 0)
                {
                    connection->sender_addr = syn_sender_addr; // Store the sender's address
                    break;
                }
            }
        }

        RUDPPacket synack_packet;// Create a SYN-ACK packet
        synack_packet.length = htons(RUDP_HEADER_SIZE);
        synack_packet.header.checksum = 0;
        synack_packet.header.flags.SYN = 1;
        synack_packet.header.flags.ACK = 1;
        synack_packet.header.checksum = htons(calculate_checksum((char *)&synack_packet, sizeof(RUDPPacket)));
        printf("Sending SYN-ACK packet with checksum: %u\n", ntohs(synack_packet.header.checksum));

        if (sendto(sockfd, &synack_packet, RUDP_HEADER_SIZE, 0, (struct sockaddr *)&syn_sender_addr, sizeof(struct sockaddr_in)) < 0)
        {
            perror("Error sending SYN-ACK packet");
            free(connection);
            exit(1);
        }

        RUDPPacket ack_packet;// Create an ACK packet
        while (1)// Wait for an ACK packet
        {
            if (recvfrom(sockfd, &ack_packet, sizeof(RUDPPacket), 0, NULL, NULL) < 0)
            {
                perror("Error receiving ACK packet");
                free(connection);
                exit(1);
            }

            if (ack_packet.header.flags.ACK == 1)
            {
                printf("Received ACK packet with checksum: %u\n", ntohs(ack_packet.header.checksum));
                if (verify_checksum(&ack_packet, sizeof(RUDPPacket), ack_packet.header.checksum) == 0)
                {
                    break;
                }
            }
        }
    }

    return connection;
}
/**
 * @brief Sends a data packet over a RUDP connection.
 * 
 * This function prepares and sends a data packet, then waits for an acknowledgment.
 * It handles retransmissions if no ACK is received or if a NACK is received.
 * 
 * @param connection Pointer to the RUDPConnection structure.
 * @param buffer Pointer to the data buffer to be sent.
 * @param buffer_size Size of the data buffer in bytes.
 * @param sender_addr Pointer to the sockaddr_in structure containing the sender's address.
 * 
 * @return Number of bytes sent on success, -1 on failure.
 */
int rudp_send(RUDPConnection *connection, char *buffer, int buffer_size, struct sockaddr_in *sender_addr)
{
    RUDPPacket packet;
    packet.length = buffer_size;  // Set the packet length
    memcpy(packet.data, buffer, buffer_size);  // Copy data to the packet
    packet.header.sequence_number = connection->next_sequence_number;  // Set the sequence number
    packet.header.checksum = calculate_checksum(&packet.data, sizeof(packet.data));  // Calculate the checksum
    packet.header.flags.DATA = 1;  // Mark the packet as a data packet

    int max_retries = 5;  // Maximum number of retransmission attempts
    int retry_count = 0;  // Current retry count

    while (retry_count < max_retries) {
        // Send the packet
        int bytes_sent = sendto(connection->sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr));
        if (bytes_sent < 0) {
            perror("Error sending data packet");
            return -1;
        }

        printf("Sent packet with sequence number: %u\n", packet.header.sequence_number);

        // Store the packet in history
        packet_history[history_index] = packet;
        history_index = (history_index + 1) % PACKET_HISTORY_SIZE;

        RUDPPacket ack_packet;
        socklen_t sender_addr_len = sizeof(struct sockaddr_in);
        
        // Set timeout for receiving ACK
        struct timeval tv;
        tv.tv_sec = 1;  // 1 second timeout
        tv.tv_usec = 0;
        setsockopt(connection->sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        // Try to receive ACK
        int bytes_received = recvfrom(connection->sockfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)sender_addr, &sender_addr_len);

        if (bytes_received > 0 && ack_packet.header.flags.ACK == 1 && ack_packet.header.sequence_number == connection->next_sequence_number) {
            // Received valid ACK
            printf("Received ACK for packet %u\n", connection->next_sequence_number);
            connection->next_sequence_number++;
            return bytes_sent;
        } else if (bytes_received > 0 && ack_packet.header.flags.NACK == 1) {
            // Received NACK
            printf("Received NACK for packet %u, expected %u\n", connection->next_sequence_number, ack_packet.header.sequence_number);
            if (ack_packet.header.sequence_number < connection->next_sequence_number) {
                // Receiver expects a lower sequence number, go back
                for (int i = 0; i < PACKET_HISTORY_SIZE; i++) {
                    if (packet_history[i].header.sequence_number == ack_packet.header.sequence_number) {
                        packet = packet_history[i];  // Retrieve the old packet from history
                        connection->next_sequence_number = ack_packet.header.sequence_number;
                        retry_count = 0;  // Reset retry count
                        break;
                    }
                }
            }
        } else {
            // No response received
            printf("No ACK received, retrying...\n");
        }

        retry_count++;
    }

    printf("Max retries reached for packet %u\n", connection->next_sequence_number);
    return -1;
}
/**
 * @brief Receives a data packet over a RUDP connection.
 * 
 * This function waits for and processes incoming packets. It handles in-order,
 * out-of-order, and duplicate packets. It sends acknowledgments for received packets.
 * 
 * @param connection Pointer to the RUDPConnection structure.
 * @param buffer Pointer to the buffer where received data will be stored.
 * @param buffer_size Size of the receive buffer in bytes.
 * @param sender_addr Pointer to the sockaddr_in structure to store the sender's address.
 * 
 * @return Number of bytes received on success, -1 on failure.
 */

int rudp_recv(RUDPConnection *connection, char *buffer, int buffer_size, struct sockaddr_in *sender_addr)
{
    RUDPPacket packet;
    socklen_t sender_addr_len = sizeof(*sender_addr);
    int bytes_received;
    int valid_checksum;
    uint16_t last_in_order_sequence = connection->next_sequence_number - 1;  // Last in-order sequence number received

    while (1) {
        // Receive a packet
        bytes_received = recvfrom(connection->sockfd, &packet, buffer_size, 0, (struct sockaddr *)sender_addr, &sender_addr_len);
        if (bytes_received < 0) {
            perror("Error receiving data packet");
            return -1;
        }

        // Verify checksum
        valid_checksum = verify_checksum(&packet.data, sizeof(packet.data), packet.header.checksum);
        
        printf("Received packet with sequence number: %u, expected: %u\n", packet.header.sequence_number, connection->next_sequence_number);
        
        if (packet.header.sequence_number == connection->next_sequence_number && packet.header.flags.DATA == 1 && valid_checksum == 1) {
            // Received valid packet in correct order
            printf("Valid packet received\n");
            memcpy(buffer, packet.data, packet.length);  // Copy data to buffer
            
            connection->next_sequence_number++;
            last_in_order_sequence = packet.header.sequence_number;
        } else if (packet.header.sequence_number < connection->next_sequence_number) {
            // Received old packet
            printf("Received old packet %u, expected %u. Sending ACK.\n", packet.header.sequence_number, connection->next_sequence_number);
            RUDPPacket ack_packet;
            ack_packet.header.sequence_number = packet.header.sequence_number;
            ack_packet.header.flags.ACK = 1;
            sendto(connection->sockfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr));
            continue;
        } else if (packet.header.sequence_number > connection->next_sequence_number) {
            // Received future packet
            printf("Received future packet %u, expected %u. Sending NACK.\n", packet.header.sequence_number, connection->next_sequence_number);
            RUDPPacket nack_packet;
            nack_packet.header.sequence_number = connection->next_sequence_number;
            nack_packet.header.flags.NACK = 1;
            sendto(connection->sockfd, &nack_packet, sizeof(nack_packet), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr));
            continue;
        }

        // Send cumulative ACK
        RUDPPacket cumulative_ack;
        cumulative_ack.header.sequence_number = last_in_order_sequence;
        cumulative_ack.header.flags.ACK = 1;
        if (sendto(connection->sockfd, &cumulative_ack, sizeof(cumulative_ack), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr)) < 0) {
            perror("Error sending ACK packet");
            return -1;
        }
        printf("Sent ACK for packet %u\n", last_in_order_sequence);

        return packet.length;  // Return the length of received data
    }
}
/**
 * @brief Receives a FIN packet over a RUDP connection and sends a FIN-ACK packet in response.
 * 
 * This function waits for a FIN packet to be received over a Reliable UDP (RUDP) connection.
 * Once a FIN packet is received, it sends a FIN-ACK packet back to the sender to acknowledge
 * the termination request.
 * 
 * @param connection A pointer to the RUDPConnection structure representing the connection.
 * 
 * @return 0 on success, or -1 on error.
 */
int rudp_recv_fin(RUDPConnection *connection){
    RUDPPacket fin_packet;
    socklen_t sender_addr_len = sizeof(connection->sender_addr);
    int bytes_received;
    //do - while until we get a FIN packet
    do{
        // Receive a FIN packet
        bytes_received = recvfrom(connection->sockfd, &fin_packet, sizeof(fin_packet), 0, (struct sockaddr *)&connection->sender_addr, &sender_addr_len);
        if (bytes_received < 0)
        {
            perror("Error receiving FIN packet");
            return -1;
        }
        if(fin_packet.header.flags.FIN != 1){
            printf("Error receiving FIN packet\n");
            
        }
        printf("Received FIN packet with checksum: %u\n", fin_packet.header.checksum);
        printf("Received FIN packet with sequence number: %u\n", fin_packet.header.sequence_number);
    }while(fin_packet.header.flags.FIN != 1);

    RUDPPacket fin_ack_packet;
    fin_ack_packet.header.flags.FIN_ACK = 1;
    char *fin_ack_massage = "FIN_ACK";
    memcpy(fin_ack_packet.data, fin_ack_massage, strlen(fin_ack_massage));
    if (sendto(connection->sockfd, &fin_ack_packet, sizeof(fin_ack_packet), 0, (struct sockaddr *)&connection->sender_addr, sizeof(connection->sender_addr)) < 0)
    {
        perror("Error sending FIN_ACK packet");
        return -1;
    }
    printf("Sending FIN_ACK packet with checksum: %u\n", fin_ack_packet.header.flags.FIN_ACK);
    return 0;
}
/**
 * @brief Sends a FIN packet to terminate a RUDP connection.
 * 
 * This function sends a FIN (Finish) packet to the peer to indicate the termination
 * of the Reliable UDP (RUDP) connection. It waits for a FIN-ACK (Finish Acknowledgment)
 * packet from the peer to confirm the termination.
 * 
 * @param connection A pointer to the RUDPConnection structure representing the connection.
 * 
 * @return 0 on success, or -1 on error.
 */
int rudp_send_fin(RUDPConnection *connection){
    RUDPPacket fin_packet;
    fin_packet.header.flags.FIN = 1;
    char *fin_massage = "FIN";
    memcpy(fin_packet.data, fin_massage, strlen(fin_massage));
    if (sendto(connection->sockfd, &fin_packet, sizeof(fin_packet), 0, (struct sockaddr *)&connection->sender_addr, sizeof(connection->sender_addr)) < 0)
    {
        perror("Error sending FIN packet");
        return -1;
    }
    printf("Sending FIN packet with checksum: %u\n", fin_packet.header.flags.FIN);
    //wait for FIN_ACK
    RUDPPacket fin_ack_packet;
    socklen_t sender_addr_len = sizeof(connection->sender_addr);
    if(recvfrom(connection->sockfd, &fin_ack_packet, sizeof(fin_ack_packet), 0, (struct sockaddr *)&connection->sender_addr, &sender_addr_len)<0){
        perror("Error receiving FIN_ACK packet");
        return -1;
    }
    printf("Received FIN_ACK packet with checksum: %u\n", fin_ack_packet.header.flags.FIN_ACK);
    return 0;
}

// Closes a connection between peers.
void rudp_close(RUDPConnection *connection)
{
    close(connection->sockfd);
    free(connection);
}

/*
 * @brief A checksum function that returns 16 bit checksum for data.
 * @param data The data to do the checksum for.
 * @param bytes The length of the data in bytes.
 * @return The checksum itself as 16 bit unsigned number.
 * @note This function is taken from RFC1071, can be found here:
 * @note https://tools.ietf.org/html/rfc1071
 * @note It is the simplest way to calculate a checksum and is not very strong.
 * However, it is good enough for this assignment.
 * @note You are free to use any other checksum function as well.
 * You can also use this function as such without any change.
 */
unsigned short int calculate_checksum(void *data, unsigned int bytes)
{
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
    // Main summing loop
    while (bytes > 1)
    {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
    // Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);
    // Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    return (~((unsigned short int)total_sum));
}

/**
 * @brief Verifies the checksum of the given data.
 * 
 * This function calculates the checksum of the provided data and compares it with the received checksum.
 * It uses a 16-bit checksum algorithm to ensure data integrity.
 * 
 * @param data A pointer to the data for which the checksum needs to be verified.
 * @param bytes The length of the data in bytes.
 * @param received_checksum The checksum received with the data that needs to be verified.
 * 
 * @return 1 if the checksum is valid, 0 otherwise.
 */
int verify_checksum(void *data, unsigned int bytes, unsigned short int received_checksum)
{
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
    // Main summing loop
    while (bytes > 1)
    {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
    // Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);

    total_sum += received_checksum;

    // Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    return ((unsigned short int)total_sum == 0xFFFF ? 1 : 0);
}

