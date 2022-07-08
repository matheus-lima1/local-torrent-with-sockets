#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define client_port 9000
#define buffer_size 500

typedef struct
{
  uint32_t sequence_number;
  unsigned long checksum;
  char data[buffer_size]; 
} packet_t;

unsigned long hash_function(unsigned char *str){
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
    hash = ((hash << 5) + hash) + c;

    return hash;
}

void await_requisition(){
    int socket_client; //Socket do client
    struct sockaddr_in socket_client_in; //Infos do socket do client
    int receive_len; //Armazenar tamanho da mensagem recebida
    char file_name[buffer_size];

    socket_client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); //Criação do socket do cliente
    if(socket_client == -1){ //Verificação se foi criado com sucesso
        perror("Error creating client socket");
        exit(1);
    }

    memset((char *)&socket_client_in, 0, sizeof(socket_client_in));//Limpando a struct que guardará as infos do socket do client

    socket_client_in.sin_addr.s_addr = htonl(INADDR_ANY); //Definindo um ip não específico
    socket_client_in.sin_family = AF_INET; //Família do protocolo
    socket_client_in.sin_port = htons(client_port); //Define a porta do client 

    int aux = bind( //Fazendo o bind do socket com as as definições de endereço
        socket_client, 
        (struct sockaddr *)&socket_client_in, 
        sizeof(socket_client_in));
    
    if (aux == -1){
        perror("Binding error");
        exit(1);
    }

    struct sockaddr_in socket_user_in;
    int socket_user_size = sizeof(socket_user_in);

    while (1)
    {
        printf("Awaiting requests...");
        fflush(stdout);

        //Recebendo o nome do arquivo e salvando 
        receive_len = recvfrom(
            socket_client, 
            file_name, 
            buffer_size, 
            0,
            (struct sockaddr *)&socket_user_in,
            &socket_user_size
            );
        
        //Caso ocorreu algum erro no recebimento
        if(receive_len == -1){
            perror("Error receiving file name");
            exit(1);
        }

        FILE *file;
        file = fopen(file_name, "r+");//Abrindo arquivo

        if(!file)//Verificando se foi possível abrir o arquivo
        {
            printf("Error opening file");
            exit(1);
        }

        //Determinando o tamanho do arquivo
        fseek(file, 0, SEEK_END);
        size_t bits_amount = ftell(file);
        fseek(file, 0, SEEK_SET);

        if(bits_amount > buffer_size){
            bits_amount = buffer_size;
        }

        //Variaver que armazenará o número de sequencia para enviar junto com os pacotes do arquivo
        uint32_t sequence_number;
        sequence_number = 0;

        printf("Sending File...");

        char file_buffer[buffer_size];
        packet_t packet;

        //Executa o processo enquanto ainda houver bits a serem lidos
        while(fread(file_buffer, bits_amount, 1, file) > 0){
            unsigned long hash_value;
            hash_value = hash_function(file_buffer); //Calculo do hash para ser usado no checksum 

            //Atribuição de dados ao pacote que será enviado
            packet.checksum = hash_value;
            packet.sequence_number = sequence_number;
            memset(packet.data, 0, buffer_size);
            memcpy(packet.data, file_buffer, bits_amount);

            //Envio do pacote ao user
            int aux = sendto(
                socket_client, 
                &packet, 
                sizeof(packet),
                0, 
                (struct sockaddr *)&socket_user_in,
                socket_user_size
            );

            //Validando se o envio ocorreu corretamente
            if(aux == -1){
                printf("Error sending package %d", sequence_number);
                exit(1);
            }
            
            //Incremento no numero de senquência
            sequence_number++;
        }

        fclose(file);
        printf("File sent successfully");
    }
    close(socket_client);

}

int main(){
    await_requisition();
    return 0;
}