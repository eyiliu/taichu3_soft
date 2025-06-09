#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#  include <crtdefs.h>
#  include <winsock2.h>
#  include <io.h>
#  pragma comment(lib, "Ws2_32.lib")
#else
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

#include <sys/socket.h>
#include <sys/select.h>

#include "rbcp.hh"

#define RBCP_DEBUG 0
#define RBCP_VER 0xFF
#define RBCP_CMD_WR 0x80
#define RBCP_CMD_RD 0xC0
#define RBCP_UDP_BUF_SIZE 2048


rbcp::rbcp(const std::string &ip, unsigned int udp, unsigned char id){
  m_ip = ip;
  m_udp = udp;
  m_id = id;
}

rbcp::rbcp(const std::string &ip){
  m_ip = ip;
  m_udp = 4660;
  m_id = 0;
}

int rbcp::DispatchCommand(const std::string& pszVerb,
			  unsigned int addrReg,
			  unsigned int dataReg,
			  std::string *recvStr){
  char sendData[RBCP_UDP_BUF_SIZE];
  char recvData[RBCP_UDP_BUF_SIZE];
  unsigned int tempInt;
  struct rbcp_header sndHeader;
  m_id ++;
  sndHeader.type=RBCP_VER;
  sndHeader.id = m_id;

  if(pszVerb == "wrb"){
    tempInt = dataReg;    
    sendData[0]= (char)(0xFF & tempInt);

    sndHeader.command= RBCP_CMD_WR;
    sndHeader.length=1;
    sndHeader.address=htonl(addrReg);
    
    return rbcp_com(m_ip.c_str(), m_udp, &sndHeader, sendData,recvData);
  }
  else if(pszVerb == "wrs"){
    tempInt = dataReg;    
    sendData[1]= (char)(0xFF & tempInt);
    sendData[0]= (char)((0xFF00 & tempInt)>>8);
 
    sndHeader.command= RBCP_CMD_WR;
    sndHeader.length=2;
    sndHeader.address=htonl(addrReg);

    return rbcp_com(m_ip.c_str(), m_udp, &sndHeader, sendData,recvData);
  }
  else if(pszVerb == "wrw"){
    tempInt = dataReg;

    sendData[3]= (char)(0xFF & tempInt);
    sendData[2]= (char)((0xFF00 & tempInt)>>8);
    sendData[1]= (char)((0xFF0000 & tempInt)>>16);
    sendData[0]= (char)((0xFF000000 & tempInt)>>24);

    sndHeader.command= RBCP_CMD_WR;
    sndHeader.length=4;
    sndHeader.address=htonl(addrReg);

    return rbcp_com(m_ip.c_str(), m_udp, &sndHeader, sendData,recvData);
  }
  else if(pszVerb == "rd"){
    sndHeader.command= RBCP_CMD_RD;
    sndHeader.length=(char)dataReg;
    sndHeader.address=htonl(addrReg);
    
    int re = rbcp_com(m_ip.c_str(), m_udp, &sndHeader, sendData,recvData);
    if(recvStr!=NULL){
      recvStr->clear();
      recvStr->append(recvData,dataReg);
    }
    return re;
  }
  puts("No such command!\n");
  return 0;
}


int rbcp::rbcp_com(const char* ipAddr, unsigned int port, struct rbcp_header* sendHeader, char* sendData, char* recvData){

  struct sockaddr_in sitcpAddr;
  decltype(socket(0, 0, 0)) sock;
  
  struct timeval timeout;
  fd_set setSelect;
  
  int sndDataLen;
  int cmdPckLen;

  char sndBuf[1024];
  int i, j = 0;
  int rcvdBytes;
  char rcvdBuf[1024];
  int numReTrans =0;

  if(RBCP_DEBUG) puts("\nCreate socket...\n");
  /* Create a Socket */
  sock = socket(AF_INET, SOCK_DGRAM, 0);

  sitcpAddr.sin_family      = AF_INET;
  sitcpAddr.sin_port        = htons(port);
  sitcpAddr.sin_addr.s_addr = inet_addr(ipAddr);

  sndDataLen = (int)sendHeader->length;

  if(RBCP_DEBUG) printf(" Length = %i\n",sndDataLen);

  /* Copy header data */
  memcpy(sndBuf,sendHeader, sizeof(struct rbcp_header));

  if(sendHeader->command==RBCP_CMD_WR){
    memcpy(sndBuf+sizeof(struct rbcp_header),sendData,sndDataLen);
    cmdPckLen=sndDataLen + sizeof(struct rbcp_header);
  }else{
    cmdPckLen=sizeof(struct rbcp_header);
  }

  if(RBCP_DEBUG){
    for(i=0; i< cmdPckLen;i++){
      if(j==0) {
	printf("\t[%.3x]:%.2x ",i,(unsigned char)sndBuf[i]);
	j++;
      }else if(j==3){
	printf("%.2x\n",(unsigned char)sndBuf[i]);
	j=0;
      }else{
	printf("%.2x ",(unsigned char)sndBuf[i]);
	j++;
      }
    }
    if(j!=3) printf("\n");
  }

  /* send a packet*/
  sendto(sock, sndBuf, cmdPckLen, 0, (struct sockaddr *)&sitcpAddr, sizeof(sitcpAddr));
  if(RBCP_DEBUG) puts("The packet have been sent!\n");

  /* Receive packets*/
  if(RBCP_DEBUG) puts("\nWait to receive the ACK packet...");
  while(numReTrans<3){

    FD_ZERO(&setSelect);
    FD_SET(sock, &setSelect);

    timeout.tv_sec  = 4;
    timeout.tv_usec = 0;
 
    if(select(sock+1, &setSelect, NULL, NULL,&timeout)==0){
      /* time out */
      puts("\n*****ACK packet timeout, resend data *****");
      m_id++;
      sendHeader->id = m_id;
      memcpy(sndBuf,sendHeader, sizeof(struct rbcp_header));
      sendto(sock, sndBuf, cmdPckLen, 0, (struct sockaddr *)&sitcpAddr, sizeof(sitcpAddr));
      numReTrans++;
      FD_ZERO(&setSelect);
      FD_SET(sock, &setSelect);
    } else {
      /* receive packet */
      if(FD_ISSET(sock,&setSelect)){
	rcvdBytes=recvfrom(sock, rcvdBuf, 2048, 0, NULL, NULL);

	if(rcvdBytes<(int)(sizeof(struct rbcp_header))){
	  puts("ERROR: ACK packet is too short");
	  close(sock);
	  return -1;
	}

	if((0x0f & rcvdBuf[1])!=0x8){
	  puts("ERROR: Detected bus error");
	  close(sock);
	  return -1;
	}

	rcvdBuf[rcvdBytes]=0;

	if(RBCP_CMD_RD){
	  memcpy(recvData,rcvdBuf+sizeof(struct rbcp_header),rcvdBytes-sizeof(struct rbcp_header));
	}

	if(RBCP_DEBUG){
	  puts("\n***** A pacekt is received ! *****.");
	  puts("Received data:");
	  j=0;
	  for(i=0; i<rcvdBytes; i++){
	    if(j==0) {
	      printf("\t[%.3x]:%.2x ",i,(unsigned char)rcvdBuf[i]);
	      j++;
	    }else if(j==3){
	      printf("%.2x\n",(unsigned char)rcvdBuf[i]);
	      j=0;
	    }else{
	      printf("%.2x ",(unsigned char)rcvdBuf[i]);
	      j++;
	    }
	    if(i==7) printf("\n Data:\n");
	  }

	  if(j!=3) puts(" ");
	}
	numReTrans = 4;
	close(sock);
	return(rcvdBytes);
      }
    }
  }
  close(sock);
  return -3;
}
