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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <signal.h>
#include <arpa/inet.h>

#include "linenoise.h"
#include "getopt.h"

#include "daqb.hh"


#include "TFile.h"
#include "TTree.h"

#define DEBUG_PRINT 0
#define dprintf(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)


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
--help                     : print usage information, and then quit
--file    <raw_data_path>  : file path of input raw data
--dir     <dir_path>       : default[ data ], directory path where output data file will be placed

'help' command in interactive mode provides detail usage information
)"
 );



int main(int argc, char **argv){

  std::string dataOutDir="data";
  std::string dataInFile;

  int do_verbose = 0;
  bool do_rootfile = false;
  {////////////getopt begin//////////////////
    struct option longopts[] = {{"help",      no_argument, NULL, 'h'},//option -W is reserved by getopt
                                {"verbose",   no_argument, NULL, 'v'},//val
                                {"i",   required_argument, NULL, 'i'},
                                {"o",   required_argument, NULL, 'o'},
                               
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
      case 'i':
        dataInFile = optarg;
        break;
      case 'o':
        dataOutDir = optarg;
        break;
      case 't':
        do_rootfile = true;

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

  std::filesystem::path rawdata_path(dataInFile);
  rawdata_path = std::filesystem::absolute(rawdata_path);
  std::filesystem::file_status st_file = std::filesystem::status(rawdata_path);
  if (!std::filesystem::exists(st_file)){
    std::fprintf(stderr, "Unable rawdata file does not esist:  %s\n\n", rawdata_path.c_str());
  }


  std::ifstream ifs(rawdata_path.c_str(), std::ios::binary);
  if(!ifs.good()){
    std::fprintf(stderr, "Unable open rawdata file: %s\n\n", rawdata_path.c_str());
    throw;
  }

  DataInBuffer inbuf;
  while( !ifs.eof() && !ifs.fail() && ifs.good() ){
    inbuf.getsome(ifs, 10000);
    if(inbuf.havepacket()){
      std::string packstr = inbuf.getpacket();
      DataPack dp(packstr);

    }
  }
}


//HEAD       DAQ_ID   TS_TID_H  TS_TID_L  LEN_H      LEN_L      TaichuRAW_32bit * N            ENDH       ENDL
//10101010  xxxxxxxx  xxxxxxxx  xxxxxxxx  xxxxxxxx   xxxxxxxx  {bit_last ---- bit_first} * N   11001100   11001100
class DataInBuffer{
public:
  int64_t readsome(istream &is, size_t length){
    if(m_temp_buf.length() < length){
      m_temp_buf.resize(length);
    }
    int64_t len_actual = m_is.readsome(m_temp_buf.data(), length);
    if(len_actual>0){
      append(len_actual, m_temp_buf.data());
    }
    return len_actual;
  }

  void append(size_t length, const char *data){
    m_buf += std::string(data, length);
    updatelength(false);
  };

  bool havepacket() const{
    return m_buf.length() >= m_len  && m_len!=0;
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
  void updatelength(bool force){
    if (force || m_len == 0) {
      if (m_buf.length() >= 6) {
        m_len = size_t(m_buf[4])<<8 + size_t(m_buf[5]);
      }
    }
  };
  //check 1st pack format in updatelength
  size_t m_len{0};
  std::string m_buf{""};
  std::string m_temp_buf{""};
};


struct PixelWord{
  PixelWord(const uint32_t v){ //BE32TOH
    // valid(1) tschip(8) xcol[9]  yrow[10] pattern[4]
    pattern =  v & 0xf;
    yrow    = (v>> 4) & 0x3ff;
    xcol    = (v>> (4+10)) & 0x1ff;
    tschip  = (v>> (4+10+9)) & 0xff;
    isvalid = (v>> (4+10+9+8)) & 0x1;
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
    uint8_t *p = packraw.data();
    packhead = *p;
    p++;
    daqid = *p;
    p++;
    tid = *p;
    p++;
    tid = itd<<8;
    tid += *p;
    p++;
    len = *p;
    p++;
    len = len<<8;
    len += *p;

    uint16_t pixelwordN = (len - 8)/ 4;
    for(size_t n = 0; n< datawordN; n++){
      p += 4;
      uint32_t v  = BE32TOH(*reinterpret_cast<const uint32_t*>(p));
      PixelWord pw(v);
    }
    p++;
    packend = *p;
    p++;
    packend = packend<<8;
    packend += *p;
  };

  std::vector<PixelWord> vecpixel;
  uint8_t packhead;
  uint8_t daqid;
  uint16_t tid;
  uint16_t len;
  uint16_t packend;
  std::string packraw;
};


  // bool testData(){
  //   uint64_t Bv  = BE64TOH(raw);;
  //   uint64_t Lv  = LE64TOH(raw);;

  //   std::bitset<64> Bvbit(Bv);
  //   std::cout<< "BE " << "iiiivffffffffffffffffffffffffffffsssssssscccccccccrrrrrrrrrrpppp"<<std::endl;
  //   std::cout<< "BE " << Bvbit <<std::endl;

  //   std::bitset<64> Lvbit(Lv);
  //   std::cout<< "LE " << "iiiivffffffffffffffffffffffffffffsssssssscccccccccrrrrrrrrrrpppp"<<std::endl;
  //   std::cout<< "LE " << Lvbit <<std::endl;
  // };
