
#include <unistd.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <iostream>
#include <thread>

#include "TcpConnection.hh"



namespace{
  std::string binToHexString(const char *bin, int len){
    constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    const unsigned char* data = (const unsigned char*)(bin);
    std::string s(len * 2, ' ');
    for (int i = 0; i < len; ++i) {
      s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
      s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return s;
  }

  std::string hexToBinString(const char *hex, int len){
    if(len%2){
      throw;
    }
    size_t blen  = len/2;
    const unsigned char* data = (const unsigned char*)(hex);
    std::string s(blen, ' ');
    for (int i = 0; i < blen; ++i){
      unsigned char d0 = data[2*i];
      unsigned char d1 = data[2*i+1];
      unsigned char v0;
      unsigned char v1;
      if(d0>='0' && d0<='9'){
        v0 = d0-'0';
      }
      else if(d0>='a' && d0<='f'){
        v0 = d0-'a'+10;
      }
      else if(d0>='A' && d0<='F'){
        v0 = d0-'A'+10;
      }
      else{
        std::fprintf(stderr, "wrong hex string\n");
        throw;
      }
      if(d1>='0' && d1<='9'){
        v1 = d1-'0';
      }
      else if(d1>='a' && d1<='f'){
        v1 = d1-'a'+10;
      }
      else if(d1>='A' && d1<='F'){
        v1 = d1-'A'+10;
      }
      else{
        std::fprintf(stderr, "wrong hex string\n");
        throw;
      }
      s[i]= (v0<<4) + v1;
    }
    return s;
  }

  std::string binToHexString(const std::string& bin){
    return binToHexString(bin.data(), bin.size());
  }

  std::string hexToBinString(const std::string& hex){
    return hexToBinString(hex.data(), hex.size());
  }

}






TcpConnection::TcpConnection(int sockfd, FunProcessMessage recvFun,  FunSendDeamon sendFun, void* pobj){
  m_sockfd = sockfd;
  if(m_sockfd>=0){
    m_isAlive = true;
    if(recvFun)
      m_fut = std::async(std::launch::async, &TcpConnection::threadConnRecv, this, recvFun, pobj);
    if(sendFun)
      m_fut_send = std::async(std::launch::async, sendFun, pobj, this);
  }
  printf("TcpConnnection construction done\n");
}

TcpConnection::~TcpConnection(){
  printf("TcpConnnection deconstructing\n");
  if(m_fut.valid()){
    m_isAlive = false;
    m_fut.get();
  }
  if(m_fut_send.valid()){
    m_fut_send.get();
  }
  closeSocket(m_sockfd);
  printf("TcpConnnection deconstruction done\n");
}

TcpConnection::operator bool() const {
  return m_isAlive;
}

uint64_t TcpConnection::threadConnRecv(FunProcessMessage processMessage, void* pobj){
  static const int MAX_BUFFER_SIZE = 10000;
  
  timeval tv_timeout;
  tv_timeout.tv_sec = 0;
  tv_timeout.tv_usec = 10;
  fd_set fds;
  m_isAlive = true;
  std::printf("listenning on the client connection recv\n");

  char buffer[MAX_BUFFER_SIZE + 1];
  while (m_isAlive){

    FD_ZERO(&fds);
    FD_SET(m_sockfd, &fds);
    FD_SET(0, &fds);
    if(!select(m_sockfd+1, &fds, NULL, NULL, &tv_timeout) || !FD_ISSET(m_sockfd, &fds) ){
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    int count = recv(m_sockfd, buffer, (unsigned int)MAX_BUFFER_SIZE,0);
    if(count == 0 && errno != EWOULDBLOCK && errno != EAGAIN){
      m_isAlive = false; // closed connection
      std::printf("connection is closed by remote peer\n");
      break;
    }
    if(count == 0){
      continue;
    }
    
    m_tcpbuf.append(count, buffer);    
    while (m_tcpbuf.havepacket()){
      int re = (*processMessage)(pobj, this, m_tcpbuf.getpacket());
      if(re < 0){
        std::fprintf(stderr, "error: processMessage return error \n");
        m_isAlive = false;
        break;
      }
    }
  }
  m_isAlive = false;
  printf("threadConnRecv exited\n");
  return 0;
}


void TcpConnection::sendRaw(const char* buf, size_t len){
  const char* remain_bufptr = buf;
  size_t remain_len = len;
  while (remain_len > 0 && m_isAlive){
    int written = write(m_sockfd, remain_bufptr, remain_len);
    if (written < 0) {
      fprintf(stderr, "ERROR writing to the TCP socket. errno: %d\n", errno);
      if (errno == EPIPE) {
        m_isAlive = false;
        break;
      }
    }
    else{
      remain_bufptr += written;
      remain_len -= written;
    }
  }
}

std::unique_ptr<TcpConnection> TcpConnection::connectToServer(const std::string& host,  short int port, FunProcessMessage recvFun, FunSendDeamon sendFun, void* pobj){
  auto now = std::chrono::system_clock::now();
  auto now_c = std::chrono::system_clock::to_time_t(now);
  printf("AsyncTcpClientConn is running...\n");

  int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0)
    fprintf(stderr, "ERROR opening socket");

  sockaddr_in serv_addr;
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = inet_addr(host.c_str());
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
    if(errno != EINPROGRESS){
      std::fprintf(stderr, "ERROR<%s>: unable to start TCP connection, error code %i \n", __func__, errno);
    }
    if(errno == 29){
      std::fprintf(stderr, "ERROR<%s>: TCP open timeout \n", __func__);
    }
    printf("\nConnection Failed \n");
    closeSocket(sockfd);
    return nullptr;
  }
  setupSocket(sockfd);
  return std::make_unique<TcpConnection>(sockfd, recvFun, sendFun, pobj);
}

int TcpConnection::createServerSocket(short int port){
  int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0)
    fprintf(stderr, "ERROR opening socket");

  setupSocket(sockfd);

  sockaddr_in serv_addr;
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
    fprintf(stderr, "ERROR on binding. errno=%d\n", errno);
    closeSocket(sockfd);
    return -1;
  }
  listen(sockfd, 1);
  return sockfd;
}

std::unique_ptr<TcpConnection> TcpConnection::waitForNewClient(int sockfd, const std::chrono::milliseconds &timeout, FunProcessMessage recvFun, FunSendDeamon sendFun, void* pobj){
  sockaddr_in cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  std::chrono::system_clock::time_point  tp_timeout = std::chrono::system_clock::now() + timeout;
  int sockfd_conn = -1;
  while(1){
    sockfd_conn = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); //wait for the connection
    if(sockfd_conn>=0){ // got new sockfd;
      break;
    }
    else{
      if(std::chrono::system_clock::now() > tp_timeout){
        break;
      }
      if((errno == EAGAIN  || errno == EWOULDBLOCK)){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      else{
        std::fprintf(stderr, "server socket error\n");
        break;
      }
    }
  }

  if(sockfd_conn<0){
    return nullptr;
  }
  else{
    printf("new connection from %03d.%03d.%03d.%03d\n",
         (cli_addr.sin_addr.s_addr & 0xFF), (cli_addr.sin_addr.s_addr & 0xFF00) >> 8,
         (cli_addr.sin_addr.s_addr & 0xFF0000) >> 16, (cli_addr.sin_addr.s_addr & 0xFF000000) >> 24);
    setupSocket(sockfd_conn);
    return std::make_unique<TcpConnection>(sockfd_conn, recvFun, sendFun, pobj);
  }
}

void TcpConnection::setupSocket(int& sockfd) {
  /// Set non-blocking mode
  int iof = fcntl(sockfd, F_GETFL, 0);
  if (iof != -1)
    fcntl(sockfd, F_SETFL, iof | O_NONBLOCK);

  /// Allow the socket to rebind to an address that is still shutting down
  /// Useful if the server is quickly shut down then restarted
  int one = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

  /// Periodically send packets to keep the connection open
  setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one);

  /// Try to send any remaining data when a socket is closed
  linger ling;
  ling.l_onoff = 1; ///< Enable linger mode
  ling.l_linger = 1; ///< Linger timeout in seconds
  setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &ling, sizeof ling);
}

int TcpConnection::createSocket(){
  return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

void TcpConnection::closeSocket(int& sockfd){
  if(sockfd>=0){
    close(sockfd);
    sockfd = -1;
  }
}



void TcpConnection::TCPResyncBuffer(){
    m_tcpbuf.resyncpacket();
    return;
}
