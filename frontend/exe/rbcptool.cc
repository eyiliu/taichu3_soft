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
                                     {"help", "info", "start", "dac",
                                      "quit", "exit", "sensor", "firmware", "set", "get",
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

    if (std::regex_match(result, std::regex("\\s*(quit)|(exit)|(.q)\\s*")) ){
      linenoiseHistoryAdd(result);
      free(result);
      linenoiseHistorySave(linenoise_history_path.string().c_str());
      break;
    }
    else if ( std::regex_match(result, std::regex("\\s*(help)\\s*")) ){
      fprintf(stdout, "%s", help_usage_linenoise.c_str());
    }
    // else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(\\w+)\\s*"));
      std::string  name  = mt[3].str();
      uint64_t value = Frontend::String2Uint64(mt[4].str());
      fprintf(stderr, "%s = %u, %#x\n", name.c_str(), value, value);
      
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
    else if (std::regex_match(result, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      uint64_t value = fw.GetFirmwareRegister(name);
      fprintf(stderr, "%s = %u, %#x\n", name.c_str(), value, value);
    }
    else if ( std::regex_match(result, std::regex("\\s*(dac)\\s+(set)\\s+([a-dA-D])\\s+([-+]?[0-9]*\\.?[0-9]+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(dac)\\s+(set)\\s+([a-dA-D])\\s+([-+]?[0-9]*\\.?[0-9]+)\\s*"));
      uint32_t ch_n = toupper(mt[3].str()[0]) - 'A';
      double value = std::stod(mt[4].str());

      std::cout<< "set board dac "<<ch_n << " " << value << " v"<<std::endl;
      fw.SetBoardDAC(ch_n, value);
      
      //        N  N  N RW CM CM CM CM CH CH CH CH  D  D  D  D  D  D  D  D  D  D  D  D  D  D  D  D  M  M  M  M
      // code2 [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0]
      // ch_B  0b1010001111010110   1.6 (of 2.5)
      // 
      // code2 [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]
      // ch_A  0b0011000000100001   0.47 (of 2.5)
      //
      // code2 [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0]
      // ch_C  0b1010100100010101   1.6512 (of 2.5)      
      
    }
    else if(std::regex_match(result, std::regex("\\s*(start)\\s*"))){
      fw.SetFirmwareRegister("CHIP_MODE", 0);
      
      fw.SetFirmwareRegister("CHIP_RESTN_CLEAR", 1);
      fw.SetFirmwareRegister("CHIP_RESTN_SET", 1);
      fw.SetFirmwareRegister("CHIP_RESTN_CLEAR", 0);
      fw.SetFirmwareRegister("CHIP_RESTN_SET", 0);

      fw.SetSensorRegister("RCKI", 1);
      fw.SetSensorRegisters({{"TRIGN",1}, {"CPRN",1}, {"DOFREQ", 0b01} });
      fw.SetSensorRegister("DAC_REG113", 1);
      fw.SetSensorRegister("PSET", 1);

      fw.SetFirmwareRegister("SESR_DELAY", 0x04); 
      fw.SetFirmwareRegister("FW_SOFT_RESET", 0xff);
     
      //TODO; load mask
      //

      fw.SetSensorRegister("DAC_REG7_0", 0b01000101);
      fw.SetSensorRegister("DAC_REG15_8", 0b01100000);
      fw.SetSensorRegister("DAC_REG23_16", 0b10010000);  //TODO::  ITHR  32 + prefix 1 

      fw.SetSensorRegister("DAC_REG31_24", 0b00001011);
      fw.SetSensorRegister("DAC_REG39_32", 0b00000110);
      fw.SetSensorRegister("DAC_REG47_40", 0b00000110);
      fw.SetSensorRegister("DAC_REG55_48", 0b11011110);
      fw.SetSensorRegister("DAC_REG63_56", 0b00000000);
      
      fw.SetSensorRegister("DAC_REG71_64", 0b11111000);
      fw.SetSensorRegister("DAC_REG79_72", 0b11111000);
      fw.SetSensorRegister("DAC_REG87_80", 0b11111000);
      fw.SetSensorRegister("DAC_REG95_88", 0b11111000);
      fw.SetSensorRegister("DAC_REG103_96", 0b11111000);
      fw.SetSensorRegister("DAC_REG111_104", 0b11111000);
      fw.SetSensorRegisters({{"REG_CDAC_8NA4_0", 0b00010}, {"REG_CDAC_8NA4_0", 1}});
      fw.SetSensorRegisters({{"REG_CDAC_8NA_TRIM", 0}, {"REG_CDAC_8NA5", 0}});      
      
    }
    else if (!strncmp(result, "info", 5)){
      
    }
    else{
      std::cout<< "unknown command"<<std::endl;
    }

    linenoiseHistoryAdd(result);
    free(result);
    linenoiseHistorySave(linenoise_history_path.string().c_str());
    linenoiseHistoryFree();
    linenoiseHistoryLoad(linenoise_history_path.string().c_str());
  }
  linenoiseHistoryFree();
}
