#include "server.h"



int main(int argc, char *argv[]){
	int port;
	if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'p') {
		port = atoi(argv[2]);
	}
	else if (argc != 3) {
		printf("Usage: %s [-p port]\n",argv[0]);
		return 0;
	}
	int sockfd,confd;
	struct sockaddr_in serv_addr;
	bzero((char*) &serv_addr, sizeof(serv_addr));


	if((sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket creation");
		exit(0);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	printf("Running on port=%d\n",port);
	if ( bind( sockfd, (struct sockaddr * ) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind");
		exit(0);
	}

	//no listen in udp sockets....make socket bind and send/recv

	// int lfd = listen(sockfd, 20);
	// if(lfd==-1)
	// 	perror("lfd:");

	fd_set rset, allset;
	int maxfd = sockfd;
	int connfd;
	FD_ZERO (&allset);
	FD_SET (sockfd, &allset);
    Client_info clientHT[CLIENT_HT_SIZE];

	for(int i=0;i<CLIENT_HT_SIZE;i++)
		clientHT[i]=NULL;

	Client_info cinfo;

	while(1) {
		rset = allset;
		// printf("sockfd = %d\tmaxfd = %d\n",sockfd,maxfd);
		int ready = select (maxfd + 1, &rset, NULL, NULL, NULL);
		if(ready == -1) {
			perror("select");
			exit(0);
		}
		if (FD_ISSET (sockfd, &rset)) {
			struct sockaddr_in cliaddr;
			int clilen = sizeof (cliaddr);
			char buffer[MAX_BUF_SIZE];
			memset(buffer,0,sizeof(buffer));
			int recv_len = recvfrom(sockfd, buffer, MAX_BUF_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t * ) &clilen);

			//******need to make one packet here itself and process the request...already got one (get request)




			if((connfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
				perror("socket creation");
				exit(0);
			}
			printf("Created new Socket for %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
			serv_addr.sin_port = htons(0);	//port number random,rest all as before

			if ( bind( connfd, (struct sockaddr * ) &serv_addr, sizeof(serv_addr)) < 0) {
				perror("bind");
				exit(0);
			}
			if (connfd > maxfd)
				maxfd = connfd;
			FD_SET (connfd, &allset);	//add confd to allset

			insert(clientHT, connfd, NULL, &cliaddr);

			cinfo = lookup(clientHT,connfd);
			packet_ *reply_pkt = makePkt(buffer,cinfo,clientHT);
			char * reply_string = pkt_to_string(reply_pkt);
			// for(int k=0; k<30; k++) {
			// 	printf("%d=%d\n", k, reply_string[k]);
			// }
			// printf("rs = %s\n", reply_string+4);
			int sendret = sendto(connfd, reply_string, MAX_BUF_SIZE, 0,(struct sockaddr*) cinfo->cliaddr, clilen);
			// printf("sendret=%d\n", sendret);
			if(sendret < 0) {
				perror("sendto");
			}
		}
		for (int ifd = 0; ifd <= maxfd; ifd++) {
			if(ifd == sockfd)	//already handled
				continue;
			if (FD_ISSET (ifd, &rset)) {			//if in readset
				int isAlive = processClient(clientHT,ifd);	//read socket ,makePkt and send reply
				if(isAlive == -1) {
					FD_CLR (ifd, &allset);
				}
			}
		}
	}
}


packet_ *makePkt(char *buffer, Client_info cinfo,Client_info clientHT[]){
	packet_ *pkt = (packet_*)malloc(sizeof(packet_));
	unsigned short int opcode;
    	memcpy(&opcode,buffer,sizeof(short));		//get opcode
	opcode = ntohs(opcode);
    	// pkt->opcode=opcode;		//kj
	int readptr =2;	//already read 2 bytes
	switch(opcode){
		case(TFTP_RRQ):{
			// memset(pkt->read_req.filename, '\0',MAX_STRING_SIZE+1);
			// memset(pkt->read_req.mode, '\0',MAX_MODE_SIZE+1);
			char filename[MAX_STRING_SIZE];
			memset(filename, 0, sizeof(filename));
			int i;
			for(i = readptr; i<MAX_BUF_SIZE; i++) {		//obtaining filename
				if(buffer[i] == '\0')
					break;
				filename[i-readptr] = buffer[i];
			}
			readptr =i+1;
			char mode[MAX_MODE_SIZE];
			memset(mode, 0, sizeof(mode));
			for(i = readptr; i<MAX_BUF_SIZE; i++) {		//obtaining mode
				if(buffer[i] == '\0')
					break;
				mode[i-readptr] = buffer[i];
			}
			// printf("opcode = %d\nfilename =%s\nmode=%s\n", opcode, filename, mode);
			FILE * fp = fopen(filename, "r");
			cinfo->fp = fp;
			file_to_pkt(cinfo, pkt, 0);		// this will check if file exists, yes-packet of type DATA, if no then packet of type ERR
		}
		break;
		case(TFTP_WRQ):{

		}
		break;
		case(TFTP_DATA):{

		}
		break;
		case(TFTP_ACK):{
			unsigned short int block_no;
			memcpy(&block_no, buffer + readptr, sizeof(unsigned short));		//get opcode
			block_no = ntohs(block_no);
			printf("Recvd ACK %d\n", block_no);
			if(cinfo->last_pkt->data.dataSize < MAX_DATA_SIZE) {
				printf("File transfer complete\n");
				// close(cinfo->fd);
				char kk[MAX_STRING_SIZE];
				memset(kk, 0, sizeof(kk));
				int clilen = sizeof(struct sockaddr_in);
				int sret = sendto(cinfo->fd, kk, MAX_STRING_SIZE, 0, (struct sockaddr *) cinfo->cliaddr, clilen);
				if(sret<0)
					perror("sendto");
				return NULL;
			}
			file_to_pkt(cinfo, pkt, block_no);
		}
		break;
		case(TFTP_ERR):{
			unsigned short int errorcode;
			memcpy(&errorcode, buffer + readptr, sizeof(unsigned short));
			errorcode = ntohs(errorcode);
			char errormsg[MAX_STRING_SIZE];
			readptr += 2;
			for(int k=readptr; k<MAX_BUF_SIZE; k++) {
				if(buffer[k] != '\0')
					break;
				errormsg[k - readptr] = buffer[k];
			}
			printf("Error %d: %s\n", errorcode, errormsg);
			pkt = NULL;
		}
		break;
	}
	return pkt;
}
int processClient(Client_info clientHT[], int fd) {
	char buffer[MAX_BUF_SIZE];
	struct sockaddr_in cliaddr;
	int clilen = sizeof(cliaddr);
	// printf("fd  = %d\n",fd);
	int nread = recvfrom(fd, buffer, MAX_BUF_SIZE, 0 , (struct sockaddr *) &cliaddr, &clilen);		//read from socket
	if(nread < 0) {
		perror("recvfrom");
		return 0;
	}
	Client_info cinfo = lookup(clientHT,fd);
	// printf("fd  = %d\n",fd);
	if(cinfo->cliaddr->sin_addr.s_addr != cliaddr.sin_addr.s_addr || cinfo->cliaddr->sin_port != cliaddr.sin_port){
		printf("Received from different client on fd =%d",fd);
		return 0;
	}
	packet_ *reply_pkt = makePkt(buffer, cinfo,clientHT);	//make the reply packet
	if(reply_pkt == NULL) {	//all data sent and got final ack
		// char reply2[MAX_STRING_SIZE];
		// memset(reply2, 0, sizeof(reply2));
		// packet_ reply_pkt2;
		// reply_pkt2.data.blockNumber = cinfo->last_pkt->data.blockNumber+1;
		// reply_pkt2.data.dataSize = MAX_DATA_SIZE-1;
		// memset(reply_pkt2.data.data, 0, MAX_DATA_SIZE);
		// reply_pkt2.opcode = TFTP_DATA;
		// char * reply_string = pkt_to_string(&reply_pkt2);
		// int sendret = sendto(fd, reply_string, MAX_BUF_SIZE, 0,(struct sockaddr*) cinfo->cliaddr, clilen);
		// if(sendret < 0) {
		// 	perror("sendto");
		// }
		// close(cinfo->fd);
		// fclose(cinfo->fp);
		// delete(clientHT, cinfo->fd);
		return -1;
	}
	char * reply_string = pkt_to_string(reply_pkt);
	int sendret = sendto(fd, reply_string, MAX_BUF_SIZE, 0,(struct sockaddr*) cinfo->cliaddr, clilen);
	if(sendret < 0) {
		perror("sendto");
	}
	return 0;
}
//takes as input a char* buffer and returns the equivalent packet structure

void file_to_pkt(Client_info cinfo, packet_ * pkt, int acknumber) {
	FILE * fp = cinfo->fp;
	if(fp == NULL) {		// file doesn't exist
		pkt->opcode = TFTP_ERR;
		pkt->error.errorCode = 17;	// no such file
		sprintf(pkt->error.message, "File doesn't exist at server!");
		return;
	}
	if(cinfo->last_pkt && cinfo->last_pkt->data.blockNumber == (acknumber + 1)) {		//Retransmit
		pkt = cinfo->last_pkt;
		return;
	}
	if(cinfo->last_pkt && cinfo->last_pkt->data.blockNumber > (acknumber + 1)) {		//Drop
		pkt = NULL;
		return;
	}
	pkt->opcode = TFTP_DATA;
	pkt->data.blockNumber = acknumber + 1;
	int n = fread(pkt->data.data, MAX_DATA_SIZE, 1, fp);
	if(n == 0) {						// reached the end of file
		pkt->data.dataSize = strlen(pkt->data.data);
	}
	else {							// not in end
		pkt->data.dataSize = MAX_DATA_SIZE;
	}
	// cinfo->fp = fp;
	cinfo->last_pkt = pkt;
}

//returns the string to be sent
char *pkt_to_string(packet_* pkt) {
	char *buffer = (char*)malloc(sizeof(char)*MAX_BUF_SIZE);
	memset(buffer, 0, sizeof(buffer));
	short int op = htons(pkt->opcode);
	memcpy(buffer,&op,sizeof(short int));
	int writeptr = 2;

	switch(pkt->opcode){
		case TFTP_DATA :
		{
			short int block_no = htons(pkt->data.blockNumber);
			memcpy(buffer + writeptr, &block_no, sizeof(short int));
			writeptr+=2;
			memcpy(buffer + writeptr, pkt->data.data, pkt->data.dataSize);
		}
		break;
		case TFTP_ERR :
		{
			short int errorcode = htons(pkt->error.errorCode);
			memcpy(buffer + writeptr, &errorcode, sizeof(short int));
			writeptr += 2;
			memcpy(buffer + writeptr, pkt->error.message, strlen(pkt->error.message));
		}
		break;
	}
	return buffer;
}
int hash(int fd) {
	int keyval = 0;
	while(fd > 0) {
		keyval += fd%10;
		fd /= 10;
	}
	return keyval%CLIENT_HT_SIZE;
}

Client_info lookup(Client_info table[], int fd){
	int index = hash(fd);
	Client_info cinfo = table[index];
	while(cinfo && cinfo->fd!=fd)
		cinfo = cinfo->next;
	return cinfo;
}

Client_info create_client(int fd, FILE * fp, struct sockaddr_in * cliaddr) {
	Client_info newclient = (Client_info) malloc (sizeof(client_info));
	newclient->fd = fd;
	newclient->fp = fp;
	newclient->cliaddr = cliaddr;
	newclient->next = NULL;
	newclient->last_pkt = NULL;
	return newclient;
}

void insert(Client_info clientHT[], int fd, FILE * fp, struct sockaddr_in * cliaddr) {
	int index = hash(fd);
	Client_info newclient = create_client(fd, fp, cliaddr);
	Client_info itr = clientHT[index];
	if(itr == NULL){
		clientHT[index] = newclient;
		return;
	}
	while(itr->next != NULL)
		itr = itr->next;
	itr->next = newclient;
}

void delete(Client_info clientHT[], int fd) {
	Client_info cinfo = lookup(clientHT,fd);
	if(cinfo==NULL)
		return;
	int index = hash(fd);
	Client_info temp = clientHT[index];
	if(temp == cinfo) {
		clientHT[index]=cinfo->next;
		free(clientHT[index]);
		return;
	}
	while(temp && temp->next!=cinfo){
		temp=temp->next;
	}
	temp->next = cinfo->next;
	free(cinfo);
}