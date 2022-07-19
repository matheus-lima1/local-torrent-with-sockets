#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#define BUFLEN 1024 
#define SERVER_PORT 3000 
#define CLIENT_PORT 2222
#define USER_PORT 1111

typedef struct
{
  uint32_t num_seq; // Número de sequencia
  unsigned long checksum; //Checksum
  char data[BUFLEN]; //Info
} packet_t; //Pacote para transferência

typedef struct segmentation
{
	int port;
	char file[BUFLEN];
} segmentation;

void error(char *s)
{
  perror(s);
  exit(1);
}

unsigned long hash(unsigned char *str)
{
  unsigned long hash = 5381;
  int c;

  while (c = *str++)
    hash = ((hash << 5) + hash) + c;
  return hash;
}

void notifyServer(char * filename){
  struct sockaddr_in server_socket_in; //Socket de info do server
  struct segmentation seg; //Struct com dados para as informações necessário para atualização

  seg.port = USER_PORT; //Atribuindo o valor da porta

  int server_socket, server_socket_len = sizeof(server_socket_in); // Socket do server e seu tamanho
  char notify_file_name[BUFLEN];

  sprintf(seg.file, "%s", filename);

  if ((server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) //Verificação se a criação do socket ocorreu corretamente
  {
    error("Error creating socket\n");
  }

  memset((char *)&server_socket_in, 0, sizeof(server_socket_in));

  //Definição das infos do socket do servidor
  server_socket_in.sin_family = AF_INET;
  server_socket_in.sin_port = htons(SERVER_PORT);
  server_socket_in.sin_addr.s_addr = INADDR_ANY;                                      
    

  //Envio do pacote com os dados de atualização para o servidor
  if (sendto(server_socket, &seg, sizeof(seg), 0, (struct sockaddr *)&server_socket_in, server_socket_len) == -1)
  {
    error("Error sending filename\n");
  }
}

void receiveFile(char * request_addr, char * filename){
    struct sockaddr_in receive_file_socket_in; //Infos do socket
    int receive_file_socket; //Socket de recebimento do arquivo
    int receive_file_socket_len = sizeof(receive_file_socket_in); //Tamanho do socket de recebimento

    if ((receive_file_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) //Verificação se socket foi criado com sucesso
    {
        error("Error creating socket\n");
    }

    memset((char *)&receive_file_socket_in, 0, sizeof(receive_file_socket_in));

    //Definindo as infos do socket de recebimento do file
    receive_file_socket_in.sin_family = AF_INET; 
    receive_file_socket_in.sin_port = htons(USER_PORT);
    receive_file_socket_in.sin_addr.s_addr = inet_addr("127.0.0.1");

    //Intervalo de timeout
    struct timeval read_timeout;
    read_timeout.tv_sec = 2;
    read_timeout.tv_usec = 0;

    setsockopt(receive_file_socket, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

    //Envio da request para receber o pacote
    if (sendto(receive_file_socket, filename, BUFLEN, 0, (struct sockaddr *)&receive_file_socket_in, receive_file_socket_len) == -1)
    {
        error("Error sending request to PEER\n");
    }


    int checksum_error = 0;

    //Variável utilizada para armazenar os dados do arquivo
    FILE *fp;
    packet_t package;

    fp = fopen(filename, "w+");

    //Laço para receber os pacotes 
    while (recvfrom(receive_file_socket, &package, sizeof(package), 0, (struct sockaddr *)&receive_file_socket_in, &receive_file_socket_len) > 0)
    {
        unsigned long result;
        result = hash(package.data);

        //Verificação se o checksum bateu
        if (package.checksum != result)
        {
            printf("Error package sequence number: %d\n", package.num_seq);
            checksum_error++;
        }

        //Escrita do pacote
        fwrite(package.data, sizeof(package.data), 1, fp);
    }

    //Fecha o arquivo
    fclose(fp);

    if (checksum_error > 0)
    {
        printf("Checksum error on %d packages.\n", checksum_error);
    }
    else
    {
        printf("\nThe file was received without errors.\n");
        notifyServer(filename);
    }
}


void removeCharFromString(char *string, char c)
{
  int i;
  for (i = 0; i < strlen(string); i++)
  {
    if (string[i] == c)
    {
      string[i] = '\0';
    }
  }
}

void await_requisition(){
    struct sockaddr_in si_me, si_other; //Definição da struct que guardará os endereços
  int s; //Variável que guardará as informações do socket
  int slen = sizeof(si_other), recv_len; //Variáveis auxiliares que armazenam os tamanhos
  char fileBuf[BUFLEN]; //Buffer utilizado para armazenar e transferir informações
  packet_t packet; //Criação do pacote
  char fileName[BUFLEN]; //Buffer que armazenará as informações do nome do arquivo
  
  //Criação e verificação da integridade do socket
  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
    error("Erro ao criar o socket!\n");
  }

  //Limpando as informações da struct
  memset((char *)&si_me, 0, sizeof(si_me));
  //Definindo as características do socket
  si_me.sin_family = AF_INET; //Tipo de família do protocolo
  si_me.sin_port = htons(USER_PORT); //Define a porta em que será utilizado
  si_me.sin_addr.s_addr = inet_addr("127.0.0.1"); //Nesse caso será utilizado a constante INADDR_ANY
  //                                           que indica que não será definido um IP específico
  

  //Atrelando um socket à uma porta e verificando a sua integridade
  if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
  {
    error("Erro na funcao bind!\n");
  }

  char *returnIp; //Variável utilizada para receber o endereço IP do peer que possui o arquivo desejado
  returnIp = malloc(16);


  //Laço infinito
  while (1)
  {
    printf("\n[**] Aguardando requisicoes...\n");
    fflush(stdout);

    //Indica que recebeu um pedido, é verificado se houve erro na recepção ou não
    //Caso não haja, o nome do arquivo é armazenado em fileName
    if ((recv_len = recvfrom(s, fileName, BUFLEN, 0, (struct sockaddr *)&si_other, &slen)) == -1)
    {
      error("Erro ao receber o nome do arquivo!\n");
    }

        removeCharFromString(fileName, '\n');
        
        FILE *file;
        file = fopen(fileName, "r+");//Abrindo arquivo
        printf("Depois tentar: %s\n", fileName);

        if(!file)//Verificando se foi possível abrir o arquivo
        {
            printf("Error opening file\n");
            exit(1);
        }

        //Determinando o tamanho do arquivo
        fseek(file, 0, SEEK_END);
        size_t bits_amount = ftell(file);
        fseek(file, 0, SEEK_SET);

        if(bits_amount > BUFLEN){
            bits_amount = BUFLEN;
        }

        //Variavel que armazenará o número de sequencia para enviar junto com os pacotes do arquivo
        uint32_t sequence_number;
        sequence_number = 0;

        printf("Sending File...\n");

        char file_buffer[BUFLEN];
        packet_t package;

        //Executa o processo enquanto ainda houver bits a serem lidos
        while(fread(file_buffer, bits_amount, 1, file) > 0){
            unsigned long hash_value;
            hash_value = hash(file_buffer); //Calculo do hash para ser usado no checksum 

            //Atribuição de dados ao pacote que será enviado
            package.checksum = hash_value;
            package.num_seq = sequence_number;
            memset(package.data, 0, BUFLEN);
            memcpy(package.data, file_buffer, bits_amount);

            //Envio do pacote ao user
            int aux = sendto(
                s, 
                &package, 
                sizeof(package),
                0, 
                (struct sockaddr *)&si_other,
                slen
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
        printf("File sent successfully\n");
    }
    close(s);

}
void requestFile()
{
  struct sockaddr_in socket_server_in, socket_user_in; //Infos do socket do user e do server
  int socket_server, socket_server_len = sizeof(socket_server_in); //Socket do server e seu tamanho
  int socket_user, socket_user_len = sizeof(socket_user_in); //Socket do user e seu tamanho
  packet_t package; //Struct com os dados do pacote
  char filename[BUFLEN];
  char buf[BUFLEN];

  //Leitura do arquivo desejado
  printf("Filename: ");
  scanf("%s", filename);

  //Verificação da criação do socket
  if ((socket_server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
    error("Error creating socket\n");
  }
  
  memset((char *)&socket_server_in, 0, sizeof(socket_server_in));

  //Definindo as infos do socket do server
  socket_server_in.sin_family = AF_INET; 
  socket_server_in.sin_port = htons(SERVER_PORT); 
  socket_server_in.sin_addr.s_addr = INADDR_ANY; 
  
  //Envio do request pelo file para o server
  if (sendto(socket_server, filename, strlen(filename), 0, (struct sockaddr *)&socket_server_in, socket_server_len) == -1)
  {
    error("Error sending filename\n");
  }

  //Recebimento do client que possui o arquivo 
  recvfrom(socket_server, (char *)buf, BUFLEN, 0, (struct sockaddr *)&socket_server_in, &socket_server_len);

  //Verificação se é um client válido 
  if(strcmp(buf, "0") == 0)
  {
    printf("There is no PEER that has this file.\n");
    return;    
  }

  //Inicia o as etapas de recebimento do arquivo
  receiveFile(buf, filename);
}


int main()
{
  int option = 0;

  while (1)
  {
    printf("1 - Seed File\n");
    printf("2 - Request File\n");
    printf("Press 0 to exit\n");
    scanf("%d", &option);

    switch (option)
    {
    case 1:
      await_requisition();
      break;
    case 2:
      requestFile();
      break;
    case 0:
      exit(0);
      break;
    default:
      printf("Invalid option.\n");
    }
  }

  return 0;
}