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

static int retransmission_count = 0;

unsigned short int calculate_checksum(void *data, unsigned int bytes);



// Creating a RUDP socket and creating a handshake between two peers
RUDPConnection *rudp_socket(struct sockaddr_in *receiver_addr, struct sockaddr_in *sender_addr, int sockfd)
{
    RUDPConnection *connection = (RUDPConnection *)malloc(sizeof(RUDPConnection));
    if (connection == NULL)
    {
        perror("Failed to allocate memory for RUDPConnection");
        exit(1);
    }

    connection->sockfd = sockfd;
    connection->receiver_addr = *receiver_addr;
    if (sender_addr != NULL)
    {
        connection->sender_addr = *sender_addr;
    }
    connection->next_sequence_number = 1;

    if (sender_addr == NULL)
    {
        // Sender side
        connection->sender_addr = *receiver_addr; // Store the receiver's address as the sender's address

        RUDPPacket syn_packet;
        syn_packet.length = htons(RUDP_HEADER_SIZE);
        syn_packet.header.checksum = 0;
        syn_packet.header.flags.SYN = 1;
        syn_packet.header.checksum = htons(calculate_checksum((char *)&syn_packet, sizeof(RUDPPacket)));
        printf("Sending SYN packet with checksum: %u\n", ntohs(syn_packet.header.checksum));

        if (sendto(sockfd, &syn_packet, RUDP_HEADER_SIZE, 0, (struct sockaddr *)receiver_addr, sizeof(struct sockaddr_in)) < 0)
        {
            perror("Error sending SYN packet");
            free(connection);
            exit(1);
        }

        RUDPPacket synack_packet;
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

        RUDPPacket ack_packet;
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
        RUDPPacket syn_packet;
        struct sockaddr_in syn_sender_addr;
        socklen_t syn_sender_addr_len = sizeof(syn_sender_addr);
        while (1)
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

        RUDPPacket synack_packet;
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

        RUDPPacket ack_packet;
        while (1)
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

int rudp_send(RUDPConnection *connection, char *buffer, int buffer_size, struct sockaddr_in *sender_addr)
{
    RUDPPacket packet;
    // set a data size
    packet.length = buffer_size;

    // copy the data to the packet
    memcpy(packet.data, buffer, buffer_size);

    // set the sequence number
    packet.header.sequence_number = connection->next_sequence_number;
    // set the checksum
    packet.header.checksum = calculate_checksum(&packet.data, sizeof(packet.data));

    printf("Sending packet with checksum: %u\n", packet.header.checksum);
    printf("Sending packet with sequence number: %u\n", packet.header.sequence_number);
    // set the DATA flag
    packet.header.flags.DATA = 1;

    // Buffer to store sent packets
    RUDPPacket sent_packets[MAX_RETRANSMISSION_COUNT];
    int sent_packet_count = 0;

    // send the packet
    int bytes_sent = sendto(connection->sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr));
    if (bytes_sent < 0)
    {
        perror("Error sending data packet");
        return -1;
    }

    sent_packets[sent_packet_count++] = packet;

    // try to recive an ACK packet if not retransmit the packet again
    RUDPPacket ack_packet;
    socklen_t sender_addr_len = sizeof(struct sockaddr_in);
    int bytes_received;

    // do - while until we get an ACK packet
    do
    {
        // try to receive an ACK packet
        printf("Trying to receive ACK packet\n");
        bytes_received = recvfrom(connection->sockfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)sender_addr, &sender_addr_len);
        if (bytes_received <= 0 || ack_packet.header.flags.NACK == 1 || ack_packet.header.sequence_number != connection->next_sequence_number)
        {
            printf("Error receiving ACK packet\n");
            if (sendto(connection->sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr)) < 0)
            {
                perror("Error sending data packet");
                return -1;
            }
            packet.retransmission_count++;
            retransmission_count++;

            if (packet.retransmission_count > MAX_RETRANSMISSION_COUNT)
            {
                printf("Max retransmissions reached\n");
                return -1;
            }
        }
        else
        {
            if (ack_packet.header.sequence_number == connection->next_sequence_number && ack_packet.header.flags.ACK == 1)
            {
                printf("Received ACK packet with checksum: %u\n", ack_packet.header.sequence_number);
                break;
            }
        }

    } while (ack_packet.header.sequence_number != connection->next_sequence_number);

    // Remove the acknowledged packet from the buffer
    for (int i = 0; i < sent_packet_count; i++)
    {
        if (sent_packets[i].header.sequence_number == ack_packet.header.sequence_number)
        {
            // Shift the remaining packets
            for (int j = i; j < sent_packet_count - 1; j++)
            {
                sent_packets[j] = sent_packets[j + 1];
            }
            sent_packet_count--;
            break;
        }
    }
    // increment the next sequence number
    connection->next_sequence_number++;
    return bytes_sent;
}

int rudp_recv(RUDPConnection *connection, char *buffer, int buffer_size, struct sockaddr_in *sender_addr)
{
    RUDPPacket packet;
    socklen_t sender_addr_len = sizeof(*sender_addr);
    int bytes_received;
    int valid_checksum;
    // do - while until we get a DATA packet
    do
    {
        bytes_received = recvfrom(connection->sockfd, &packet, buffer_size, 0, (struct sockaddr *)sender_addr, &sender_addr_len);
        if (bytes_received < 0)
        {
            perror("Error receiving data packet");
            return -1;
        }
        // store the checksum of the packet
        unsigned short int checksum = packet.header.checksum;
        // set the checksum to 0
        valid_checksum = verify_checksum(&packet.data, sizeof(packet.data), checksum);
        
        printf("next_sequence_number: %u\n", connection->next_sequence_number);
        printf("packet sequence number: %u\n", packet.header.sequence_number);
        printf("valid_checksum: %d\n", valid_checksum);
        printf("packet DATA flag: %d\n", packet.header.flags.DATA);
        // print packet data
        // printf("packet data: %s\n", packet.data);
        // if the checksum is valid
        if (packet.header.sequence_number == connection->next_sequence_number && packet.header.flags.DATA == 1 && valid_checksum == 1)
        {
            printf("Received packet with checksum: %u\n", packet.header.checksum);
            printf("Received packet with sequence number: %u\n", packet.header.sequence_number);
            break;
        }
        // if the checksum is not valid, we send a NACK packet
        else
        {
            RUDPPacket nack_packet;
            nack_packet.header.flags.NACK = 1;
            char *nack_massage = "NACK";
            memcpy(nack_packet.data, nack_massage, strlen(nack_massage));
            if (sendto(connection->sockfd, &nack_packet, sizeof(nack_packet), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr)) < 0)
            {
                perror("Error sending NACK packet");
                return -1;
            }
            printf("Sending NACK packet with checksum: %u\n", nack_packet.header.flags.NACK);
            //decrement the next sequence number
            connection->next_sequence_number--;
        }
        printf("waiting for packet\n");
    } while (packet.header.sequence_number != connection->next_sequence_number || valid_checksum != 1 || packet.header.flags.DATA != 1);

    // copy the data from the packet to the buffer
    memcpy(buffer, packet.data, packet.length);
    // create an ACK packet
    RUDPPacket ack_packet;

    ack_packet.header.sequence_number = connection->next_sequence_number;
    ack_packet.header.flags.ACK = 1;
    char *ack_massage = "ACK";
    printf("Sending ACK packet with checksum");
    memcpy(ack_packet.data, ack_massage, strlen(ack_massage));
    if (sendto(connection->sockfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)sender_addr, sizeof(*sender_addr)) < 0)
    {
        perror("Error sending ACK packet");
        return -1;
    }
    // increment the next sequence number
    connection->next_sequence_number++;
    return bytes_received;
}

int rudp_recv_fin(RUDPConnection *connection){
    RUDPPacket fin_packet;
    socklen_t sender_addr_len = sizeof(connection->sender_addr);
    int bytes_received;
    do{
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

