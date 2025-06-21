
#include "Utility.hh"
#include "mysystem.hh"
#include <regex>
#include <filesystem>
#include <fstream>

uint8_t LeastNoneZeroOffset(const uint64_t& value){
  static const uint8_t bitmap[]=
    {0, 0, 1, 39, 2, 15, 40, 23, 3, 12, 16, 59, 41, 19, 24, 54, 4,
     0, 13, 10, 17, 62, 60, 28, 42, 30, 20, 51, 25, 44, 55, 47, 5, 32,
     0, 38, 14, 22, 11, 58, 18, 53, 63, 9, 61, 27, 29, 50, 43, 46, 31,
     37, 21, 57, 52, 8, 26, 49, 45, 36, 56, 7, 48, 35, 6, 34, 33};
  return bitmap[(value^(value&(value-1)))%67];
}



uint64_t String2Uint64(const std::string& str){
  uint64_t v = -1;
  std::cmatch mt;
  bool matched = std::regex_match(str.c_str(), mt, std::regex("\\s*(?:(0[Xx])+([0-9a-fA-F]+))|(?:(0[Bb])+([0-1]+))|(?:([0-9]+))\\s*"));
  if(!matched){
    FormatPrint(std::cerr, "ERROR<%s>: unknown value format.<%s>\n", __func__, str.c_str());
    throw;
  }else if(!mt[1].str().empty()){
    v = std::stoull(mt[2].str(), 0, 16);
  }else if(!mt[3].str().empty()){
    v = std::stoull(mt[4].str(), 0, 2);
  }else if(!mt[5].str().empty()){
    v = std::stoull(mt[5].str(), 0, 10);
  }else{
    FormatPrint(std::cerr, "ERROR<%s>: value format matched<%s>\n", __func__, str.c_str());
    throw;
  }
  return v;
}



std::string LoadFileToString(const std::string& p){
  std::ifstream ifs(p);
  if(!ifs.good()){
    // try relative path to binary;
    std::filesystem::path alt_path = binaryPath();
    alt_path = alt_path / p;

    std::string alt_path_str = alt_path.string();
    std::ifstream ifs_bin(alt_path_str);
    if(ifs_bin.good()){
      ifs = std::move(ifs_bin);
    }
    else{
      std::cerr<<"LoadFileToString:: ERROR, unable to load file<"<<p<<">\n";
      std::cerr<<"LoadFileToString:: ERROR, unable to load file<"<<alt_path_str<<">\n";
      throw;
    }
  }

  std::string str;
  str.assign((std::istreambuf_iterator<char>(ifs) ),
             (std::istreambuf_iterator<char>()));
  return str;
}



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


std::string TimeNowString(const std::string& format){
  std::time_t time_now = std::time(nullptr);
  std::string str_buffer(100, char(0));
  size_t n = std::strftime(&str_buffer[0], sizeof(str_buffer.size()),
                           format.c_str(), std::localtime(&time_now));
  str_buffer.resize(n?(n-1):0);
  return str_buffer;
}
