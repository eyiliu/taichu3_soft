#pragma once

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <string>
#include <future>
#include <memory>
#include <chrono>


class TcpBuffer{
public:
  void append(size_t length, const char *data){
    m_buf += std::string(data, length);
    updatelength();
  };
  
  bool havepacket() const{
    return m_buf.length() >= m_len;
  };
  std::string getpacket(){
    if (!havepacket()){
      std::cerr<<"havepacket return false\n";
      throw;
    }
    std::string packet(m_buf, 0, m_len);
    m_buf.erase(0, m_len);
    updatelength(true);
    return packet;
  };

private:
  void updatelength(bool = false){m_len =8;}; //for fixed length
  //check 1st pack format in updatelength
  
  size_t m_len{8};
  std::string m_buf{""};
};


class TcpConnection;

//callback
typedef int (*FunProcessMessage)(void* pobj, void* pconn,  const std::string& pak);
typedef int (*FunSendDeamon)(void* pobj, void*pconn);

class TcpConnection{
public:
  TcpConnection() = delete;
  TcpConnection(const TcpConnection&) =delete;
  TcpConnection& operator=(const TcpConnection&) =delete;
  TcpConnection(int sockfd, FunProcessMessage recvFun, FunSendDeamon sendFun, void* pobj);

  ~TcpConnection();

  operator bool() const;

  //forked thread
  uint64_t threadConnRecv(FunProcessMessage recvFun, void* pobj);

  void sendRaw(const char* buf, size_t len);

  static int createSocket();
  static void setupSocket(int& sockfd);
  static void closeSocket(int& sockfd);

  //client side
  static std::unique_ptr<TcpConnection> connectToServer(const std::string& host,  short int port, FunProcessMessage recvFun, FunSendDeamon sendFun, void* pobj);
  //server side
  static int createServerSocket(short int port);
  static std::unique_ptr<TcpConnection> waitForNewClient(int sockfd, const std::chrono::milliseconds &timeout, FunProcessMessage recvFun, FunSendDeamon sendFun, void* pobj);

  static std::string binToHexString(const char *bin, int len);

private:
  std::future<uint64_t> m_fut;
  std::future<int> m_fut_send;
  bool m_isAlive{false};
  int m_sockfd{-1};

  TcpBuffer m_tcpbuf;
};
