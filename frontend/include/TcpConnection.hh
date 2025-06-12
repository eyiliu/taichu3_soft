#pragma once

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <string>
#include <future>
#include <memory>
#include <chrono>


#include "StreamInBuffer.hh"

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

  void TCPResyncBuffer();

private:
  std::future<uint64_t> m_fut;
  std::future<int> m_fut_send;
  bool m_isAlive{false};
  int m_sockfd{-1};

  StreamInBuffer m_tcpbuf;
};
