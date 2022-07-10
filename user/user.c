#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>   
#include <sys/time.h>

#define LOCAL_IP "127.0.0.1"
#define SIZE 1024
#define USER_PORT 1111
#define SERVER_PORT 8888

typedef struct package{
    int seq_num;
    char data[1024];
    int checksum[8];
    int size;
} package;

typedef struct message{
    int client_port;
    char file[20];
} message;


void add_binary(int response[], int bin[]){
    int check = 0;
    int aux;

    for ( int i = 7; i >= 0; i-- ){
        aux = response[i];
        response[i] = ((aux ^ bin[i]) ^ check);                    
        check = ((aux & bin[i]) | (aux & check)) | (bin[i] & check);
    }
    if (check == 1){
        int aux2;
        for (int i = 7; i >= 0; i--){
            aux2 = response[i];
            response[i] = ((aux2 ^ 0) ^ check);          
            check = ((aux2 & 0) | (aux2 & check)) | (0 & check); 
        }
    }
}

int checksum(package *pkg){
    
    int bin[0];
    int sum[8];

    if(pkg == NULL){
        return 0;
    }

    for( int i = 0; i < pkg -> size; i++ ){
        char aux = pkg -> data[i];
        for( int j = 7; j>= 0; --j ){
            if(aux & (1 << j)){
                bin[7 - j] = 1;
            }
            else{
                bin[7 - j] = 0;
            }
        }

        add_binary(sum, bin);
    }

    add_binary(sum, pkg -> checksum);

    int verified = 1;
    for ( int i = 0; i < 8; i++ ){
        if(sum[i] != 0){
            verified = 0;
        }
    }

    return verified;

}



void consume_message(int server_domain, struct sockaddr_in remote_addr, char *buffer){
    int received_from;
    socklen_t addr_lenght = sizeof(remote_addr);

    while(1){
        received_from = recvfrom(server_domain, buffer, SIZE, 0, (struct sockaddr *)&remote_addr, &addr_lenght);

        if(received_from == -1){
            printf("Error receiving message\n");
            exit(1);
        }
        else{
            printf("Received message: %s\n", buffer);
            break;
        }
    }
}

void produce_message(int server_domain, struct sockaddr_in remote_addr, char *buffer, int type){
    int received_from;
    socklen_t addr_lenght = sizeof(remote_addr);

    if(type == 1){
        received_from = sendto(server_domain, buffer, SIZE, 0, (struct sockaddr *)&remote_addr, addr_lenght);

        if(received_from == -1){
            printf("Error sending message\n");
            exit(1);
        }
        else{
            // printf("Sent message: %s\n", buffer);
        }
    }
    else if(type == 2){
        message message;
        message.client_port = USER_PORT;
        strcpy(message.file, buffer);

        received_from = sendto(server_domain, &message, sizeof(message), 0, (struct sockaddr *)&remote_addr, addr_lenght);
    
        if(received_from == -1){
            printf("Error sending message\n");
            exit(1);
        }
        else{
            // printf("Sent message: %s\n", buffer);
        }
    }
}

void consume_package(int server_domain, struct sockaddr_in remote_addr, char *filename){
    package pkg;
    FILE *file;
    int received_from;
    int counter = 0;
    char ack = '1';
    char nak = '0';

    file = fopen(filename, "wb");
    if(file == NULL){
        printf("Error opening file\n");
        exit(1);
    }

    socklen_t addr_lenght = sizeof(remote_addr);

    while(1){
        memset(&pkg, 0, sizeof(package));

        received_from = recvfrom(server_domain, &pkg, sizeof(pkg), 0, (struct sockaddr *)&remote_addr, &addr_lenght);
        
        if(received_from == -1){
            printf("Error receiving package\n");
            exit(1);
        }
        else{
            if(checksum(&pkg) == 1){
                printf("Received package: %d\n", pkg.seq_num);
            }
        }
        
        int verified = checksum(&pkg);
        if(verified = 1 && pkg.seq_num == counter + 1){
            printf("package %d received \n", pkg.seq_num);
            
            usleep(4000);
            system("tput cuu1");
            system("tput dl1");

            fwrite(pkg.data, 1, pkg.size, file);
            sendto(server_domain, &ack, sizeof(ack), 0, (struct sockaddr *)&remote_addr, addr_lenght);
            counter++;

            if(pkg.size < SIZE){
                break;
            }
        }
        else{
            printf("Package %d corrupted in the way, waiting for resend\n", pkg.seq_num);
            sendto(server_domain, &nak, sizeof(nak), 0, (struct sockaddr *)&remote_addr, addr_lenght);
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]){
    struct sockaddr_in remote_server_addr;
    struct sockaddr_in remote_client_b;


    int server_domain;
    int CLIENT_PORT;

    char *buffer = (char *)malloc(SIZE * sizeof(char));

    if(argc == 1){
        printf("Error!! Enter the file name \n");
        exit(1);
    }

    strcpy(buffer, argv[1]);
    server_domain = socket(AF_INET, SOCK_DGRAM, 0);

    if(server_domain == -1){
        printf("Error creating socket\n");
        exit(1);
    }

    memset(&remote_server_addr, 0, sizeof(remote_server_addr));

    remote_server_addr.sin_family = AF_INET;
    remote_server_addr.sin_port = htons(SERVER_PORT);
    remote_server_addr.sin_addr.s_addr = inet_addr(LOCAL_IP);

    produce_message(server_domain, remote_server_addr, buffer, 1);

    printf("Request sent to server\n");
    sleep(1);
    memset(buffer, '\0', SIZE);

    consume_message(server_domain, remote_server_addr, buffer);

    if(buffer[0] == '1'){
        CLIENT_PORT = atoi(&buffer[1]);
        printf("The client on port %d has the file.\n", CLIENT_PORT);
        memset(buffer, '\0', SIZE);
    }
    else{
        printf("File not found.\n");
        exit(1);
    }

    memset(&remote_client_b, 0, sizeof(remote_client_b));

    remote_client_b.sin_family = AF_INET;
    remote_client_b.sin_port = htons(CLIENT_PORT);
    remote_client_b.sin_addr.s_addr = inet_addr(LOCAL_IP);

    strcpy(buffer, argv[1]);

    produce_message(server_domain, remote_client_b, buffer, 1);
    sleep(1);
    printf("Request sent to the client who owns the file\n");
    memset(buffer, '\0', SIZE);

    consume_message(server_domain, remote_client_b, buffer);

    if(buffer[0] == '1'){
        sleep(1);
        printf("Initiating transfer\n");
        sleep(1);
        strcpy(buffer, argv[1]);
        consume_package(server_domain, remote_client_b, buffer);

        produce_message(server_domain, remote_server_addr, buffer, 2);

        memset(buffer, '\0', SIZE);
        consume_message(server_domain, remote_server_addr, buffer);
        if (buffer[0] == '1')
            printf("\nCustomer A now present in the database\n");
    }
    else{
        printf("File not found.\n");
        exit(1);
    }

    free(buffer);

    return 0;


}