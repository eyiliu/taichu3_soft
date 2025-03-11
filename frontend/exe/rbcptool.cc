#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <iostream>
#include <filesystem>


#include <signal.h>

#include "Frontend.hh"

#include "getopt.h"
#include "linenoise.h"

static  const std::string help_usage
(R"(
Usage:
-s json_file: json filepath of sensor registers 
-f json_file: json filepath of firmware registers
-i ip_address: eg. 192.168.10.16 \n\
-h : print usage information, and then quit
)"
 );


static  const std::string help_usage_linenoise
(R"(

keyword: help, info, quit, sensor, firmware, set, get

example: 
  1) get firmware regiester
   > firmware get FW_REG_NAME

  2) set firmware regiester
   > firmware set FW_REG_NAME 10

  3) get sensor regiester
   > sensor get SN_REG_NAME

  4) set sensor regiester
   > sensor set SN_REG_NAME 10

  5) exit/quit command line
   > quit

  6) get ip address (base) from firmware regiesters
   > info

)"
 );


int main(int argc, char **argv){
  
  std::string f_opt;
  std::string s_opt;
  std::string i_opt;
  int c;
  while ( (c = getopt(argc, argv, "f:s:i:h")) != -1) {
    switch (c) {
    case 'f':
      f_opt = optarg;
      break;
    case 's':
      s_opt = optarg;
      break;
    case 'i':
      i_opt = optarg;
      break;
    case 'h':
      fprintf(stdout, "%s", help_usage.c_str());
      return 0;
      break;
    default:
      fprintf(stderr, "%s", help_usage.c_str());
      return 1;
    }
  }
  
  if (optind < argc) {
    fprintf(stderr, "\ninvalid options: ");
    while (optind < argc)
      fprintf(stderr, "%s\n", argv[optind++]);;
    return 1;
  }

  ////////////////////////
  //test if all opts
  if(s_opt.empty() || f_opt.empty() ||  i_opt.empty()){
    fprintf(stderr, "\ninsufficient options.\n%s\n\n\n",help_usage.c_str());
    return 1;
  }
  ///////////////////////
  
  std::string reglist_path_sensor = s_opt;
  std::string reglist_path_firmware = f_opt;
  std::string ip_address_str = i_opt;
  
  ///////////////////////
  std::string reglist_jsstr_sensor   = Frontend::LoadFileToString(reglist_path_sensor);
  std::string reglist_jsstr_firmware = Frontend::LoadFileToString(reglist_path_firmware);

  Frontend fw(reglist_jsstr_sensor, reglist_jsstr_firmware, ip_address_str);
  
  std::filesystem::path linenoise_history_path = std::filesystem::temp_directory_path() / "rbcptool.history.txt";
  
  linenoiseHistoryLoad(linenoise_history_path.string().c_str());
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "info",
                                      "quit", "sensor", "firmware", "set", "get",
                                      NULL};
                                   size_t i;
                                   for (i = 0;  examples[i] != NULL; ++i) {
                                     if (strncmp(prefix, examples[i], strlen(prefix)) == 0) {
                                       linenoiseAddCompletion(lc, examples[i]);
                                     }
                                   }
                                 } );
  
  const char* prompt = "\x1b[1;32mtaichu_rbcp\x1b[0m> ";
  while (1) {
    char* result = linenoise(prompt);
    if (result == NULL) {
      break;
    }    
    if ( std::regex_match(result, std::regex("\\s*(quit)\\s*")) ){
      printf("quiting \n");
      linenoiseHistoryAdd(result);
      free(result);
      break;
    }
    else if ( std::regex_match(result, std::regex("\\s*(help)\\s*")) ){
      fprintf(stdout, "%s", help_usage_linenoise.c_str());
      linenoiseHistoryAdd(result);
      free(result);
      break;
    }

    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      std::string name = mt[3].str();
      uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      fw.SetSensorRegister(name, value);
    }
    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      uint64_t value = fw.GetSensorRegister(name);
      fprintf(stderr, "%s = %u, %#x\n", name.c_str(), value, value);
    }
    
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      std::string name = mt[3].str();
      uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      fw.SetFirmwareRegister(name, value);
    }
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      uint64_t value = fw.GetFirmwareRegister(name);
      fprintf(stderr, "%s = %u, %#x\n", name.c_str(), value, value);
    }
    else if (!strncmp(result, "info", 5)){
      uint32_t ip0 = fw.GetFirmwareRegister("IP0");
      uint32_t ip1 = fw.GetFirmwareRegister("IP1");
      uint32_t ip2 = fw.GetFirmwareRegister("IP2");
      uint32_t ip3 = fw.GetFirmwareRegister("IP3");
      std::cout<<"\n\ncurrent ip  " <<ip0<<":"<<ip1<<":"<<ip2<<":"<<ip3<<"\n\n"<<std::endl;
    }
    linenoiseHistoryAdd(result);
    free(result);
  }

  linenoiseHistorySave(linenoise_history_path.string().c_str());
  linenoiseHistoryFree();
}
