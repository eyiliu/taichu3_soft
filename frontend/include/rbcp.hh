#ifndef _RBCP_H_
#define _RBCP_H_ 1

#include <string>

class rbcp{
public:
  struct rbcp_header{
    unsigned char type;
    unsigned char command;
    unsigned char id;
    unsigned char length;
    unsigned int address;
  };

  rbcp(const std::string& ip, unsigned int udp, unsigned char id);
  rbcp(const std::string& ip);
  rbcp() = delete;
  rbcp(const rbcp&) =delete;
  rbcp& operator=(const rbcp&) =delete;
  int rbcp_com(const char* ipAddr, unsigned int port,
	       struct rbcp_header* sendHeader,
	       char* sendData, char* recvData);
  
  int DispatchCommand(const std::string &pszVerb,
		      unsigned int addrReg,
		      unsigned int dataReg,
		      std::string *recvStr
		      );

private:
  std::string  m_ip;
  unsigned int m_udp;
  unsigned char m_id;
};

#endif
