
#include "server.h"



int main(int argc, char *argv[]){
	int port;
	if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'p') {
		port = atoi(argv[2]);
	}
	else if (argc != 2) {
		printf("Usage: %s [-p port]\n",argv[0]);
		return 0;
	}
	int sockfd,confd;
	struct sockaddr_in serv_addr;
	bzero((char*) &serv_addr, sizeof(serv_addr));


	if((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket creation");
		exit(0);
	}

	serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);

	if ( bind( sockfd, (struct sockaddr * ) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind");
		exit(0);
	}

	int lfd = listen(sockfd, 20);

	fd_set rset, allset;
	int maxfd = lfd, connfd;
	FD_ZERO (&allset);
	FD_SET (lfd, &allset);
    	Client_info clientHT[CLIENT_HT_SIZE];

	for(int i=0;i<CLIENT_HT_SIZE;i++)
		clientHT[i]=NULL;

	Client_info cinfo;

	while(1) {
		rset = allset;
		int ready = select (maxfd + 1, &rset, NULL, NULL, NULL);
		if(ready == -1) {
			perror("select");
			exit(0);
		}
		if (FD_ISSET (lfd, &rset)) {
			struct sockaddr_in cliaddr;
			int clilen = sizeof (cliaddr);
			char buffer[MAX_BUF_SIZE];
			int recv_len = recvfrom(sockfd, buffer, MAX_BUF_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t * ) &clilen);

			if((connfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
				perror("socket creation");
				exit(0);
			}

			serv_addr.sin_port = htons(0);	//port number random

			if ( bind( connfd, (struct sockaddr * ) &serv_addr, sizeof(serv_addr)) < 0) {
				perror("bind");
				exit(0);
			}

			if (connfd > maxfd)
				maxfd = connfd;
			FD_SET (connfd, &allset);	//add confd to allset

			insert(clientHT, connfd, NULL, &cliaddr);

		}
		for (int ifd = 0; ifd <= maxfd; ifd++) {
			if(ifd == lfd)	//already handled
				continue;
			processClient(clientHT,ifd);

					// if ((n = read (sockfd, buf, MAXLINE)) == 0) {
					// 	/*connection closed by client */
					// 	close (sockfd);
					// 	FD_CLR (sockfd, &allset);
					// 	client[i] = -1;
					// }
					// else
					// 	write (sockfd, buf, n);

					// if (--nready <= 0)
					// 	break;		/* no more readable descriptors */

		}
	}
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
	while(temp->next!=cinfo)
		temp=temp->next;
	temp->next = cinfo->next;
	free(cinfo);
}

int processClient(Client_info clientHT[], int fd) {
	char buffer[MAX_BUF_SIZE];
	struct sockaddr_in cliaddr;
	int clilen = sizeof(cliaddr);
	int nread = recvfrom(fd, buffer, MAX_BUF_SIZE, 0 , (struct sockaddr *) &cliaddr, &clilen);
	if(nread < 0) {
		perror("recvfrom");
		return 0;
	}
	Client_info cinfo = lookup(clientHT,fd);

	if(cinfo->cliaddr->sin_addr.s_addr != cliaddr.sin_addr.s_addr || cinfo->cliaddr->sin_port != cliaddr.sin_port){
		printf("Received from different client on fd =%d",fd);
		return 0;
	}
	char opcode[3];
	strcpy(opcode, buffer);
	opcode[2] = '\0';
	int readptr = 2;
	packet_ packet;
	short int op;					//converts first 2 bytes of buffer to int-readable opcode
	memcpy(&op, opcode, sizeof(short int));
	op = ntohs(op);
	switch(op) {
		case 1:		// RRQ
		{
			char filename[1024];
			memset(filename, '\0', sizeof(filename));
			for(int i = readptr; i<MAX_BUF_SIZE; i++) {		//obtaining filename
				if(buffer[i] == '\0')
					break;
				filename[i-readptr] = buffer[i];
				readptr++;
			}
			readptr += 2;
			char mode[1024];
			memset(mode, '\0', sizeof(mode));
			for(int i = readptr; i<MAX_BUF_SIZE; i++) {		//obtaining mode
				if(buffer[i] == '\0')
					break;
				mode[i-readptr] = (char) tolower(buffer[i]);
				readptr++;
			}
			if(strcmp(mode, "octet") != 0) {
				packet.opcode = TFTP_ERR;
				packet.error.errorCode = 4;
				strcpy(packet.error.message, "Only octet mode allowed!");
				sendpkt(cinfo, &packet);
				break;
			}
			FILE * fp = fopen(filename, "r");
			cinfo->fp = fp;
			// send_data(cinfo);
		}
		break;
		case 2:		// WRQ
		{

		}
		break;
		case 3:		// DATA
		{

		}
		break;
		case 4:		// ACK
		{

		}
		break;
		case 5:		// ERR
		{

		}
		break;
	}
}

void sendpkt(Client_info cinfo, packet_ * pkt) {
	char buffer[MAX_BUF_SIZE];
	memset(buffer, 0, sizeof(buffer));
	// sprintf(buffer, "%2d%2d%s%c", pkt->opcode, pkt->error.errorCode, pkt->error.message, (char) 0);
	int nread = sendto(cinfo->fd, buffer, MAX_BUF_SIZE, 0 , (struct sockaddr *) cinfo->cliaddr,sizeof(cinfo->cliaddr));
}