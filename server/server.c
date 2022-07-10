#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define IP_LOCAL "127.0.0.1"

#define SERVER_PORT 2000
#define SIZE_BUFFER 1024

void die(char *s)
{
	perror(s);
	exit(1);
}

typedef struct segmentation
{
	int port;
	char file[50];
} segmentation;

// busca via dns.txt o cliente que possui o arquivo solicitado
int find_dns(FILE *DNS, char *buffer, char *client_port)
{

	char port[5];
	char file[50];

	fseek(DNS, 0, SEEK_SET);
	while (fscanf(DNS, "%s %s", file, port) != EOF)
	{
		if (strcmp(file, buffer) == 0)
		{
			strcpy(client_port, port);
			return 1;
		}
	}

	return 0;
}

// atualiza o banco dns.txt com as informações propagadas
int update_dns(FILE *DNS, struct segmentation att)
{
	char port[5];
	char file[50];

	while (fscanf(DNS, "%s %s", file, port) != EOF)
	{
		if (strcmp(file, att.file) == 0 && att.port == atoi(port))
			return 0;
	}

	fprintf(DNS, "%s %d\n", att.file, att.port);
	fflush(DNS);

	return 0;
}

int check_buffer(char *buffer)
{
	if (buffer[0] != '\0')
		return 1;

	return 0;
}

void start_server()
{
}

int main(int argc, char *argv[])
{

	struct sockaddr_in user_address;
	int socket_server;

	int len_recv;

	socklen_t len_socket_user;

	char seeder[50];
	char *buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

	FILE *DNS;
	DNS = fopen("dns.txt", "r+b");
	if (DNS == NULL)
	{
		die("ERROR: failed to open dns file");
	}

	// Criação e verificação da integridade do socket
	if ((socket_server = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		die("ERROR: broken integrity socket");
	}

	// Limpando as informações da struct
	memset((char *)&user_address, 0, sizeof(user_address));

	user_address.sin_family = AF_INET;
	user_address.sin_addr.s_addr = inet_addr(IP_LOCAL);
	user_address.sin_port = htons(SERVER_PORT);

	// Atrelando um socket à uma porta local e verificando a sua integridade
	if (bind(socket_server, (struct sockaddr *)&user_address, sizeof(user_address)) == -1)
	{
		die("ERROR: binding socket to local port");
	}

	printf("server RUNNING on port %d\n", SERVER_PORT);

	while (1)
	{

		memset(buffer, '\0', SIZE_BUFFER);

		len_socket_user = sizeof(user_address);

		if (len_recv = recvfrom(socket_server, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&user_address, &len_socket_user) == -1){
			die("ERROR: the application was unable to get the file name\n");
		}
		printf("SUCCESS: request received!\n");
		printf("INFO: request received from %s:%d\n", inet_ntoa(user_address.sin_addr), ntohs(user_address.sin_port));

		if (check_buffer(buffer))
		{

			if (find_dns(DNS, buffer, seeder))
			{

				memset(buffer, '\0', SIZE_BUFFER);
				buffer[0] = '1';
				strcat(buffer, seeder);
				printf("INFO: sending response to client\n");
				sendto(socket_server, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&user_address, len_socket_user);

				printf("INFO: waiting response....\n");

				segmentation att;
				while (1)
				{
					memset(&att, 0, sizeof(segmentation));
					recvfrom(socket_server, &att, sizeof(att), 0, (struct sockaddr *)&user_address, &len_socket_user);
					update_dns(DNS, att);
					sendto(socket_server, buffer, sizeof(buffer), 0, (struct sockaddr *)&user_address, len_socket_user);
					printf("SUCCESS: updated DNS database\n");
					break;
				}
			}
			else
			{
				memset(buffer, '\0', SIZE_BUFFER);
				buffer[0] = '0';
				sendto(socket_server, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&user_address, len_socket_user);
			}
		}
	}
}
