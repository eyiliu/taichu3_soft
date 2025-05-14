#include <cstdio>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <ctime>

#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>
#include <regex>
#include <atomic>
#include <iostream>
#include <fstream>


#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <signal.h>
#include <arpa/inet.h>


#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>


#include "linenoise.h"
#include "getopt.h"
#include "mysystem.hh"

#include "TFile.h"
#include "TTree.h"

#define DEBUG_PRINT 0
#define dprintf(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)


std::string getnowstring(){
  char buffer[80];
  std::time_t t = std::time(nullptr);
  size_t n = std::strftime(buffer, sizeof(buffer), "%FT%H%M%S%Z", std::localtime(&t));
  return std::string(buffer, n);
}



int connectToServer(const std::string& host,  short int port){
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
    close(sockfd);
    sockfd = -1;
    return sockfd;
  }

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

  return sockfd;
}

size_t socketread( void* buffer, size_t bsize, size_t bcount,  int sockfd){
  timeval tv_timeout;
  tv_timeout.tv_sec = 0;
  tv_timeout.tv_usec = 10;
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(sockfd, &fds);
  FD_SET(0, &fds);

  int i = 0;
  while(!select(sockfd+1, &fds, NULL, NULL, &tv_timeout) || !FD_ISSET(sockfd, &fds) ){
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    i++;
    if(i>3){
      return 0;
    }
  }

  int length_actual = recv(sockfd, buffer, (unsigned int)bsize*bcount,0);
  if(length_actual == 0 && errno != EWOULDBLOCK && errno != EAGAIN){
      std::printf("connection is closed by remote peer\n");
  }
  return length_actual;
}



bool gBrokenData = false;




bool check_and_create_folder(std::filesystem::path path_dir){

  std::filesystem::path path_dir_output = std::filesystem::absolute(path_dir);
  std::filesystem::file_status st_dir_output =
    std::filesystem::status(path_dir_output);
  if (!std::filesystem::exists(st_dir_output)) {
    std::fprintf(stdout, "Folder does not exist: %s\n\n",
                 path_dir_output.c_str());
        std::filesystem::file_status st_parent =
          std::filesystem::status(path_dir_output.parent_path());
        if (std::filesystem::exists(st_parent) &&
            std::filesystem::is_directory(st_parent)) {
          if (std::filesystem::create_directory(path_dir_output)) {
            std::fprintf(stdout, "New folder is created: %s\n\n", path_dir_output.c_str());
          } else {
            std::fprintf(stderr, "Unable to create folder: %s\n\n", path_dir_output.c_str());
            throw;
          }
        } else {
          std::fprintf(stderr, "Unable to create folder: %s\n\n", path_dir_output.c_str());
          throw;
        }
      }
  return true;
}


std::FILE * create_and_open_file(std::filesystem::path filepath){

  std::filesystem::path filepath_abs = std::filesystem::absolute(filepath);
  std::filesystem::file_status st_file = std::filesystem::status(filepath_abs);
  if (std::filesystem::exists(st_file)) {
    std::fprintf(stderr, "File < %s > exists.\n\n", filepath_abs.c_str());
    throw;
  }

  std::FILE *fp = std::fopen(filepath_abs.c_str(), "w");
  if (!fp) {
    std::fprintf(stderr, "File opening failed: %s \n\n", filepath_abs.c_str());
    throw;
  }
  return fp;
}

TFile* create_and_open_rootfile(const std::filesystem::path& filepath){

  std::filesystem::path filepath_abs = std::filesystem::absolute(filepath);
  std::filesystem::file_status st_file = std::filesystem::status(filepath_abs);
  if (std::filesystem::exists(st_file)) {
    std::fprintf(stderr, "File < %s > exists.\n\n", filepath_abs.c_str());
    throw;
  }
  TFile *tf = TFile::Open(filepath_abs.c_str(),"recreate");
  if (!tf) {
    std::fprintf(stderr, "ROOT TFile opening failed: %s \n\n", filepath_abs.c_str());
    throw;
  }
  return tf;
}



static  const std::string help_usage
(R"(
Usage:
--help                         : print usage information, and then quit
--file0       <raw_data_path>  : file path of input raw data 0
--root        <dir_path>       : directory path where output root file will be placed. default: current path

'help' command in interactive mode provides detail usage information
)"
 );

// --file1       <raw_data_path>  : file path of input raw data 1



//HEAD       DAQ_ID   TS_TID_H  TS_TID_L  LEN_H      LEN_L      TaichuRAW_32bit * N            ENDH       ENDL
//10101010  xxxxxxxx  xxxxxxxx  xxxxxxxx  xxxxxxxx   xxxxxxxx  {bit_last ---- bit_first} * N   11001100   11001100
class DataInBuffer{
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
    if(!havepacket_possible()){
      return;
    }

    std::string packet(m_buf, 0, m_len);
    while(havepacket_possible() &&
          (m_buf[0]!=0b10101010 || (m_buf[m_len-2] != 0b11001100 || m_buf[m_len-1] != 0b11001100))){
      if(m_buf[0]!=0b10101010){
        std::string::size_type pos = m_buf.find(0b10101010);
        if(pos != std::string::npos){
          m_buf.erase(0, pos);
          updatelength(true);
        }else{
          m_buf.clear();
          updatelength(true);
        }
        continue;
      }
      if(m_buf[m_len-2] != 0b11001100 || m_buf[m_len-1] != 0b11001100) {
        std::string::size_type pos = m_buf.find("\xcc\xcc");
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
    fprintf(stdout, "extract datapack_string [%04d]:   ", m_len);
    for(size_t n = 0; n< packet.size(); n++){
      if(n!=0 && n%16==0){ std::cout<<"                           "; }
      uint16_t num = (uint8_t)packet[n];
      fprintf(stdout, "%02X ", num);
      if(n%2==1)   std::cout<<" - ";
      if(n%16==15) std::cout<<std::endl;
    }

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

private:
  void updatelength(bool force){
    // std::cout<< "m_len udpate"<<std::endl;
    if (force || m_len == 0) {
      m_len = 0;
      if (m_buf.length() >= 6) {
        m_len = ((size_t(uint8_t(m_buf[4]))<<8) + (size_t(uint8_t(m_buf[5]))))  * 4  + 8;
        // std::cout<< "len update "<< m_len<< std::hex<<" " << size_t(uint8_t(m_buf[4])) << "  "<< size_t(uint8_t(m_buf[5]))<<std::dec<<std::endl;
      }
    }
  };
  //check 1st pack format in updatelength
  size_t m_len{0};
  std::string m_buf;
  std::string m_temp_buf;
};


struct PixelWord{
  PixelWord(const uint32_t v){ //BE32TOH
    // valid(1) tschip(8) xcol[9]  yrow[10] pattern[4]
    pattern =  v & 0xf;
    yrow    = (v>> 4) & 0x3ff;
    xcol    = (v>> (4+10)) & 0x1ff;
    tschip  = (v>> (4+10+9)) & 0xff;
    isvalid = (v>> (4+10+9+8)) & 0x1;
    raw = v;
  }

  uint16_t xcol;
  uint16_t yrow;
  uint8_t  tschip;
  uint8_t  pattern;
  uint8_t  isvalid;
  uint32_t raw;
};

struct DataPack{
  DataPack(const std::string&  packstr){
    packraw = packstr;
    const uint8_t *p = reinterpret_cast<const uint8_t*>(packraw.data());
    packhead = *p;
    p++;
    daqid = *p;
    p++;
    tid = *p;
    p++;
    tid = tid<<8;
    tid += *p;
    p++;
    len = *p;
    p++;
    len = len<<8;
    len += *p;
    p++;
    uint16_t pixelwordN = len;
    for(size_t n = 0; n< pixelwordN; n++){
      uint32_t v  = BE32TOH(*reinterpret_cast<const uint32_t*>(p));
      vecpixel.emplace_back(v);
      p += 4;
    }
    packend = *p;
    p++;
    packend = packend<<8;
    packend += *p;

    if(packhead != 0xaa  || packend!=0xcccc){
      gBrokenData = true; // deteccted
    }
  };

  std::vector<PixelWord> vecpixel;
  uint8_t packhead;
  uint8_t daqid;
  uint16_t tid;
  uint16_t len;
  uint16_t packend;
  std::string packraw;

  void print(){
    fprintf(stdout, "-> HEAD %d   DAQID %d  TID %d   LEN  %d \n",
            (uint32_t)packhead, (uint32_t)daqid, (uint32_t)tid, (uint32_t)len);

    if(!vecpixel.empty()){
      fprintf(stdout, "[x, y, t, p, v]: ");
    }

    for(auto & pw:  vecpixel){
      fprintf(stdout, "[%d, %d, %d, %d, %d] ",
              (uint32_t)pw.xcol, (uint32_t)pw.yrow, (uint32_t)pw.tschip, (uint32_t)pw.pattern, (uint32_t)pw.isvalid);

    }
    fprintf(stdout,"\n");

    if(!vecpixel.empty()){
      std::cout<< "vttttttttxxxxxxxxxyyyyyyyyyypppp"<<std::endl;
    }
    for(auto & pw:  vecpixel){
      std::bitset<32> pwbit(pw.raw);
      std::cout<< pwbit <<std::endl;
    }

  };


  void dump(){
    for(size_t n = 0; n< packraw.length(); n++){
      uint16_t num = (uint8_t)packraw[n];
      fprintf(stdout, "%02X - ", num);
      if(n%2==1)   std::cout<<" = ";
      if(n%16==15) std::cout<<std::endl;
    }
  }

};


///////////////////////////////////////////////////////////////////////////////////


int main(int argc, char **argv){
  std::string rootOutDir;
  std::string dataInFile;
  std::string dataInFile1;

  std::string ipAddress;
  int do_verbose = 0;
  {////////////getopt begin//////////////////
    struct option longopts[] = {{"help",      no_argument, NULL, 'h'},//option -W is reserved by getopt
                                {"verbose",   no_argument, NULL, 'v'},//val
                                {"ip0",   required_argument, NULL, 'p'},
                                {"file0",   required_argument, NULL, 'i'},
                                // {"file1",   required_argument, NULL, 'j'},
                                {"root",   required_argument, NULL, 'o'},
                                {0, 0, 0, 0}};

    if(argc == 1){
      std::fprintf(stderr, "%s\n", help_usage.c_str());
      std::exit(1);
    }
    int c;
    int longindex;
    opterr = 1;
    // "-" prevents non-option argv
    while ((c = getopt_long_only(argc, argv, "-", longopts, &longindex)) != -1) {
      // if(!optopt && c!=0 && c!=1 && c!=':' && c!='?'){                                 //for debug
      //   std::fprintf(stdout, "opt:%s,\targ:%s\n", longopts[longindex].name, optarg);}  //
      switch (c) {
        // help and verbose
      case 'v':
        do_verbose=1;
        //option is set to no_argument
        if(optind < argc && *argv[optind] != '-'){
          do_verbose = std::stoul(argv[optind]);
          optind++;
        }
        break;
      case 'p':
        ipAddress = optarg;
        break;
      case 'i':
        dataInFile = optarg;
        break;
      case 'j':
        dataInFile1 = optarg;
        break;
      case 'o':
        rootOutDir = optarg;
        break;

      case 'h':
        std::fprintf(stdout, "%s\n", help_usage.c_str());
        std::exit(0);
        break;
        /////generic part below///////////
      case 0:
        // getopt returns 0 for not-NULL flag option, just keep going
        break;
      case 1:
        // If the first character of optstring is '-', then each nonoption
        // argv-element is handled as if it were the argument of an option
        // with character code 1.
        std::fprintf(stderr, "%s: unexpected non-option argument %s\n",
                     argv[0], optarg);
        std::exit(1);
        break;
      case ':':
        // If getopt() encounters an option with a missing argument, then
        // the return value depends on the first character in optstring:
        // if it is ':', then ':' is returned; otherwise '?' is returned.
        std::fprintf(stderr, "%s: missing argument for option %s\n",
                     argv[0], longopts[longindex].name);
        std::exit(1);
        break;
      case '?':
        // Internal error message is set to print when opterr is nonzero (default)
        std::exit(1);
        break;
      default:
        std::fprintf(stderr, "%s: missing getopt branch %c for option %s\n",
                     argv[0], c, longopts[longindex].name);
        std::exit(1);
        break;
      }
    }
  }/////////getopt end////////////////

  if( ( ipAddress.empty() && dataInFile.empty()) ||
      ( !ipAddress.empty() && !dataInFile.empty())
      ){
      std::fprintf(stderr, "please proivde ONLY 1 of the options [--file0] and [--ip0] \n");
  }

  std::FILE* fp0 = 0;
  if(!dataInFile.empty()){
      std::filesystem::path rawdata_path(dataInFile);
      rawdata_path = std::filesystem::absolute(rawdata_path);
      std::filesystem::file_status st_file = std::filesystem::status(rawdata_path);
      if (!std::filesystem::exists(st_file)){
	  std::fprintf(stderr, "Unable rawdata file does not exist:  %s\n\n", rawdata_path.c_str());
      }
      fp0 = std::fopen(rawdata_path.c_str(), "r");
      if(!fp0){
	  std::fprintf(stderr, "Unable open rawdata file: %s\n\n", rawdata_path.c_str());
	  throw;
      }
      std::fprintf(stdout, "Rawdata file0: %s\n\n", rawdata_path.c_str());
  }

  int socketfd0 = 0;
  if(!ipAddress.empty()){
      socketfd0 = connectToServer(ipAddress, 24);
  }


  std::filesystem::path root_folder_path = std::filesystem::current_path();
  if(!rootOutDir.empty()){
      std::filesystem::path rootOutDir_path(rootOutDir);
      root_folder_path = std::filesystem::absolute(rootOutDir_path);
      check_and_create_folder(root_folder_path);
  }
  std::string nowstr = getnowstring();
  std::string basename_datafile = std::string("data_")+nowstr;
  std::filesystem::path file_rootdata_path = root_folder_path/(basename_datafile+std::string(".root"));
  TFile *tfout = create_and_open_rootfile(file_rootdata_path);
  TTree *ttout = 0;

  std::vector<uint16_t> xc;    // x column
  std::vector<uint16_t> yr;    // y row
  std::vector<uint8_t>  tsc;   // timestamp of chip

  uint8_t  hid; // hardware id
  uint16_t tid; // tlu id
  uint16_t npw; // num of pixelword

  if(tfout){
    tfout->cd();
    std::cout<<"setup  rootfd"<<std::endl;
    ttout = tfout->Get<TTree>("tree_pixel");
    if(!ttout){
      std::cout<<"creat new ttree"<<std::endl;
      ttout = new TTree("tree_pixel","tree_pixel");
      ttout->SetDirectory(tfout);
    }
    ttout->Branch("xc", &xc);
    ttout->Branch("yr", &yr);
    ttout->Branch("tsc", &tsc);
    ttout->Branch("hid", &hid);
    ttout->Branch("tid", &tid);
    ttout->Branch("npw", &npw);
    
  }

  DataInBuffer inbuf;

  std::vector<char> buf(128*5);
  size_t len_actual;

  while ( (   len_actual = socketfd0? socketread(&buf[0], sizeof buf[0], buf.size(), socketfd0 ):  std::fread(&buf[0], sizeof buf[0], buf.size(), fp0 )      ) != 0 ){
    inbuf.append(len_actual, &buf[0]);
    while(inbuf.havepacket()){
      std::string packstr = inbuf.getpacket();
      DataPack dp(packstr);
      dp.print();

      if(gBrokenData){
        std::cout<<"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Broken data is detected!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;
        std::cout<<"==============================current datapack======================================="<<std::endl;
        dp.dump();
        std::cout<<std::endl;
        std::cout<<"==============================pending data==========================================="<<std::endl;
        inbuf.dump(100);
        throw;
      }

      tid = dp.tid;
      hid = dp.daqid;
      npw = dp.vecpixel.size();

      xc.clear();
      yr.clear();
      tsc.clear();
      for(const auto &pw : dp.vecpixel){
	  xc.push_back(pw.xcol);
	  yr.push_back(pw.yrow);
	  tsc.push_back(pw.tschip);
      }
      if(ttout){
	  ttout->Fill();
      }
    }
  }
  if(std::ferror(fp0)){
    std::fprintf(stdout,"\nI/O error when reading\n");
  }
  if(std::feof(fp0))
  {
    std::fprintf(stdout,"\nEnd of File\n");
  }
  std::fclose(fp0);

  if(tfout){
    ttout->Write();
    tfout->Close();
  }

  return 0;
}
