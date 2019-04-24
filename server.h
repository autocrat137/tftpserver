#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define MAX_BUF_SIZE 1024
#define PAYLOAD_SIZE 512
#define CLIENT_HT_SIZE 20


#define MAX_STRING_SIZE 1024
#define MAX_MODE_SIZE 8
#define MAX_DATA_SIZE 512

//op codes
#define TFTP_RRQ  1
#define TFTP_WRQ  2
#define TFTP_DATA 3
#define TFTP_ACK  4
#define TFTP_ERR  5

//error Codes
#define TFTP_ERRCODE_UNDEFINED           0
#define TFTP_ERRCODE_FILE_NOT_FOUND      1
#define TFTP_ERRCODE_ACCESS_VIOLATION    2
#define TFTP_ERRCODE_DISK_FULL           3
#define TFTP_ERRCODE_ILLEGAL_OPERATION   4
#define TFTP_ERRCODE_UNKNOWN_TRANSFER_ID 5
#define TFTP_ERRCODE_FILE_ALREADY_EXISTS 6
#define TFTP_ERRCODE_NO_SUCH_USER        7

typedef struct {
    char filename[MAX_STRING_SIZE+1];
    char mode[MAX_MODE_SIZE+1];
} rrq_;

typedef struct {
  unsigned short blockNumber;
  unsigned int dataSize;
  char data[MAX_DATA_SIZE];
} data_;

typedef struct {
  unsigned short blockNumber;
} ack_;

typedef struct {
  unsigned short errorCode;
  char message[MAX_STRING_SIZE];
} error_;

typedef struct {
  unsigned short opcode;
  union {
    rrq_ read_req;
    data_ data;
    ack_ ack;
    error_ error;
  };
} packet_;

typedef struct client_info_ {
	int fd;
	FILE * fp;
	packet_* last_pkt;
	struct sockaddr_in * cliaddr;
	struct client_info_ * next;
} client_info;

typedef client_info * Client_info;

int hash(int fd);
Client_info lookup(Client_info table[],int fd);
void insert(Client_info clientHT[], int fd, FILE * fp, struct sockaddr_in * cliaddr);
void delete(Client_info clientHT[], int fd);
Client_info create_client(int fd, FILE * fp, struct sockaddr_in * cliaddr);
int processClient(Client_info clientHT[], int fd);
void sendpkt(Client_info cinfo, packet_ * pkt);
packet_ *makePkt(char *buffer, Client_info cinfo,Client_info clientHT[]);
char *pkt_to_string(packet_* pkt);
void file_to_pkt(Client_info cinfo, packet_ * pkt, int acknumber);