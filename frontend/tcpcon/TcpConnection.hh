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
    updatelength(false);
  };

  bool havepacket() const{
    return m_buf.length() >= m_len  && m_len!=0;
  };

  bool havepacket_possible() const{
    bool expectedPack = false;
    if(m_buf.length() >= m_len  && m_len!=0){
      expectedPack = true;
    }
    return expectedPack;
  };

  void  resyncpacket(){
    m_len=1;
    std::cout<<"resyncpacket"<<std::endl;
    if(!havepacket_possible()){
      return;
    }

    std::string packet(m_buf, 0, m_len);
    while(havepacket_possible() &&
          (m_buf[0]!=0b10101010 || (m_buf[m_len-2] != 0b11001100 || m_buf[m_len-1] != 0b11001100))){
      std::this_thread::sleep_for(std::chrono::milliseconds(1)); //avoid fast loop
      if(m_buf[0]!=0b10101010){
        std::string::size_type pos = m_buf.find(0b10101010);
        if(pos != std::string::npos){
          if(pos>0){
 	    std::cout<<"resyncpacket(): erase bytes to pack head="<<pos<<std::endl;
            m_buf.erase(0, pos);
	  }
          updatelength(true);
        }else{
          std::cout<<"resyncpacket(): no head found, clean buffer size="<<m_buf.size()<<std::endl;
          m_buf.clear();
          updatelength(true);
        }
        continue;
      }
      if(m_buf[m_len-2] != 0b11001100 || m_buf[m_len-1] != 0b11001100) {
        std::string::size_type pos = m_buf.find("\xcc\xcc");
        std::cout<<"resyncpacket(): erase bytes to pack tail="<<pos+2<<std::endl;
        m_buf.erase(0, pos+2);
        updatelength(true);
      }
    }
    return ;
  }

  std::string getpacket(){
    if (!havepacket()){
      std::cerr<<"havepacket return false\n";
      throw;
    }
    std::string packet(m_buf, 0, m_len);
//    fprintf(stdout, "extract datapack_string [%04d]:   ", m_len);
//    for(size_t n = 0; n< packet.size(); n++){
//      if(n!=0 && n%16==0){ std::cout<<"                           "; }
//      uint16_t num = (uint8_t)packet[n];
//      fprintf(stdout, "%02X ", num);
//      if(n%2==1)   std::cout<<" - ";
//      if(n%16==15) std::cout<<std::endl;
//    }

    m_buf.erase(0, m_len);
    updatelength(true);
    return packet;
  };

  void dump(size_t maxN){
    size_t dumpN = m_buf.size()<maxN ? m_buf.size(): maxN;
    for(size_t n = 0; n< dumpN; n++){
      uint16_t num = (uint8_t)m_buf[n];
      fprintf(stdout, "%02X - ", num);
      if(n%2==1)   std::cout<<" = ";
      if(n%16==15) std::cout<<std::endl;
    }
  };

  void resyncpackethead(){ //resync to make sure buffer starts from 0b10101010
    if(m_buf.length()==0) return;
    if(static_cast<unsigned char>(m_buf[0])!=0b10101010){ // match package header=0xaa
      std::string::size_type pos = m_buf.find(0b10101010);
      if(pos != std::string::npos){ //header is found
        std::cout<<"resyncpackethead(): erase bytes to pack head="<<pos<<std::endl;
        m_buf.erase(0, pos);
      }
      else{ //no head is found
        std::cout<<"resyncpackethead(): no head found, clean buffer, size="<<m_buf.size()<<std::endl;
        m_buf.clear();
      }
    }
  }

private:
  void updatelength(bool force){
    // std::cout<< "m_len udpate"<<std::endl;
    if (force || m_len == 0) {
      m_len = 0;
      resyncpackethead();
      if (m_buf.length() >= 6) {
        m_len = ((size_t(uint8_t(m_buf[4]))<<8) + (size_t(uint8_t(m_buf[5]))))  * 4  + 8;
        // std::cout<< "len update "<< m_len<< std::hex<<" " << size_t(uint8_t(m_buf[4])) << "  "<< size_t(uint8_t(m_buf[5]))<<std::dec<<std::endl;
      }
    }
  };
  //check 1st pack format in updatelength
  size_t m_len{0};
  std::string m_buf;
//  std::string m_temp_buf;
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

  void TCPResyncBuffer(){
    m_tcpbuf.resyncpacket();};

private:
  std::future<uint64_t> m_fut;
  std::future<int> m_fut_send;
  bool m_isAlive{false};
  int m_sockfd{-1};

  TcpBuffer m_tcpbuf;
};
