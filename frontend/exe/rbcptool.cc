#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <iostream>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <thread>

#include <signal.h>

#include "Frontend.hh"

#include "getopt.h"
#include "linenoise.h"

static  const std::string help_usage
(R"(
Usage:
-s json_file: sensor registers [path of  taichupix3_reg.json]  
-f json_file: firmware registers [path of firmware_reg.json]
-i ip_address: eg. 192.168.10.16 \n\
-h : print usage information, and then quit


reg.json files are located in folder frontend/resource

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
  std::string i_opt="192.168.10.16";
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
  std::string reglist_jsstr_sensor   = LoadFileToString(reglist_path_sensor);
  std::string reglist_jsstr_firmware = LoadFileToString(reglist_path_firmware);

  Frontend fw(reglist_jsstr_sensor, reglist_jsstr_firmware, ip_address_str, "rbcptool" ,0);
  
  std::filesystem::path linenoise_history_path = std::filesystem::temp_directory_path() / "rbcptool.history.txt";
  linenoiseHistoryLoad(linenoise_history_path.string().c_str());
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "info", "start", "config", "dac", "stop", "reset",
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
    if (std::regex_match(result, std::regex("\\s*(start)\\s*")) ){
      fw.SetFirmwareRegister("upload_data",1);
    }
    else if ( std::regex_match(result, std::regex("\\s*(stop)|(reset)\\s*")) ){
      std::cout<< "reset chip and fw"<<std::endl;
      fw.SetFirmwareRegister("upload_data", 0);
      fw.SetFirmwareRegister("chip_reset", 0);
      fw.SetFirmwareRegister("chip_reset", 1);
      fw.SetFirmwareRegister("global_reset", 1);
      fw.SetFirmwareRegister("all_buffer_reset", 1);
      // fw.SetFirmwareRegister("CHIP_RESTN_CLEAR", 1);
      // fw.SetFirmwareRegister("CHIP_RESTN_SET", 1);
      // fw.SetFirmwareRegister("CHIP_RESTN_CLEAR", 0);
      // fw.SetFirmwareRegister("CHIP_RESTN_SET", 0);

      // fw.SetFirmwareRegister("FW_SOFT_RESET", 0xff);
      std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
    }
    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(\\w+)\\s*"));
      std::string  name  = mt[3].str();
      uint64_t value = String2Uint64(mt[4].str());
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
    else if (std::regex_match(result, std::regex("\\s*(sensor)\\s+(setpixelmask)\\s+(\\w+\\.\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(setpixelmask)\\s+(\\w+\\.\\w+)\\s*"));
      std::string name = mt[3].str();
      std::set<std::pair<uint16_t, uint16_t>> mask_data = fw.ReadPixelMask_from_file(name);
      fw.FlushPixelMask(mask_data, Frontend::MaskType::MASK);
      fw.FlushPixelMask(mask_data, Frontend::MaskType::UNCAL);
      fprintf(stderr, "config mask file from file : %s\n", name.c_str());
    }
    else if (std::regex_match(result, std::regex("\\s*(sensor)\\s+(setpixel_openwindow)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(setpixel_openwindow)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)\\s*"));
      int pixel_row_low = std::stoi(mt[3].str());
      int pixel_row_high = std::stoi(mt[4].str());
      int pixel_col_low = std::stoi(mt[5].str());
      int pixel_col_high = std::stoi(mt[6].str());
      if(pixel_row_low>pixel_row_high || pixel_col_low>pixel_col_high || pixel_row_low<0 || pixel_row_high>1023 || pixel_col_low<0 || pixel_col_high>511){
        fprintf(stderr, "invalid pixel range: %d %d %d %d\n", pixel_row_low, pixel_row_high, pixel_col_low, pixel_col_high);
        continue;
      }
      std::set<std::pair<uint16_t, uint16_t>> mask_data;
      for(int xRow = 0; xRow<1024; xRow++){
        for(int yCol = 0; yCol<512; yCol++){
          if((xRow <pixel_row_low) || (xRow>pixel_row_high) || (yCol<pixel_col_low) || (yCol>pixel_col_high)){
            mask_data.insert({xRow, yCol});
          }
        }
      }
      fw.FlushPixelMask(mask_data, Frontend::MaskType::MASK);
      fw.FlushPixelMask(mask_data, Frontend::MaskType::UNCAL);
      fprintf(stderr, "successfully set open window : row %d to %d column %d to %d\n", pixel_row_low, pixel_row_high, pixel_col_low, pixel_col_high);
    }   
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(\\w+)\\s*"));
      std::string  name  = mt[3].str();
      uint64_t value = String2Uint64(mt[4].str());
      fprintf(stderr, "%s = %u, %#x\n", name.c_str(), value, value);
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
      
      // 
      //        N  N  N RW CM CM CM CM CH CH CH CH  D  D  D  D  D  D  D  D  D  D  D  D  D  D  D  D  M  M  M  M
      // code2 [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]
      // ch_A  0b0011000000100001   0.47 (of 2.5)
      //
      // code2 [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0]
      // ch_B  0b1010001111010110   1.6 (of 2.5)
      //
      // code2 [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0]
      // ch_C  0b1010100100010101   1.6512 (of 2.5)      
      
    }
    else if(std::regex_match(result, std::regex("\\s*(full)\\s*"))){
      fw.FlushPixelMask({}, Frontend::MaskType::MASK);
      fw.FlushPixelMask({}, Frontend::MaskType::UNCAL);      
    }
    
    else if(std::regex_match(result, std::regex("\\s*(config)\\s*"))){
      
      // fw.SetFirmwareRegister("CHIP_MODE", 0);
      
      // fw.SetFirmwareRegister("CHIP_RESTN_CLEAR", 1);
      // fw.SetFirmwareRegister("CHIP_RESTN_SET", 1);
      // fw.SetFirmwareRegister("CHIP_RESTN_CLEAR", 0);
      // fw.SetFirmwareRegister("CHIP_RESTN_SET", 0);

      fw.SetFirmwareRegister("upload_data", 0);
      fw.SetFirmwareRegister("chip_reset", 0);
      fw.SetFirmwareRegister("chip_reset", 1);
      fw.SetFirmwareRegister("global_reset", 1);
      fw.SetFirmwareRegister("all_buffer_reset", 1);
      fw.SetFirmwareRegister("set_daq_id", 2);
      fw.SetFirmwareRegister("global_work_mode", 1);
      // need to set daq id and trigger mode

      fw.SetSensorRegister("RCKI", 1);
      // BSEL 0 ISEL1 0 ISEL0 0 EXCKS 0 DSEL 0 CKESEL 0 RCKI (1) RCKO 0

      fw.SetSensorRegisters({{"TRIGN",1}, {"CPRN",1}, {"DOFREQ", 0b01} });
      // TRIGN (1) CPRN (1) DOFREQ 01 SMOD 0 CTM 0 SPI_D 0 TMOD 0
      // 11010000

      
      fw.SetSensorRegisters({{"REG_BGR_TC", 0b01},{"C_MASK_EN", 0}});
      // REG_BGR_TC 01 BPLDO 0 LDO_REG1 0 LDO_REG0 0 C_MASK_EN 0 ENTP 0 EN10B 0
      // 01000000
      
      fw.SetSensorRegisters({{"PSET", 1}, {"OISEL",0}, {"OPSEL", 0}});
      // RESERVED13N7_4 0000 RESERVED13N3 0 PSET (1) OISEL 0(x) OPSEL 0(x) 
      
      // fw.SetFirmwareRegister("SER_DELAY", 0x04); 
      fw.SetFirmwareRegister("load_m", 0xff);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      // fw.SetFirmwareRegister("SER_DELAY", 0x04);      

      // // disable all mask_en
      // for(size_t n = 0; n< 64; n++){
      // 	fw.SetSensorRegister("PIXELMASK_DATA", 0b00000000);
      // }
      // for(size_t n = 0; n< 1024; n++){
      // 	fw.SetSensorRegisters({{"LOADC_E", 0},{"LOADM_E", 0}});
      // }
      // fw.SetFirmwareRegister("load_m", 0);
      // fw.SetFirmwareRegister("load_m", 1);
      // fw.SetFirmwareRegister("load_m", 0);
      // //

      // // enable all cal_en
      // for(size_t n = 0; n< 64; n++){
      // 	fw.SetSensorRegister("PIXELMASK_DATA", 0b11111111);
      // }
      // for(size_t n = 0; n< 1024; n++){
      // 	fw.SetSensorRegisters({{"LOADC_E", 0},{"LOADM_E", 0}});
      // }
      // fw.SetFirmwareRegister("load_c", 0);
      // fw.SetFirmwareRegister("load_c", 1);
      // fw.SetFirmwareRegister("load_c", 0);
      
      fw.FlushPixelMask({}, Frontend::MaskType::MASK);
      fw.FlushPixelMask({}, Frontend::MaskType::UNCAL);
      
      // voltage
      // fw.SetBoardDAC(1, 1.6);
      // fw.SetBoardDAC(0, 0.47);
      // fw.SetBoardDAC(2, 1.6512);      
      
      fw.SetSensorRegisters({{"REG_CDAC0_2_0", 0b010}, {"ENIBG", 0}, {"REG_BGR_OFFSET", 0b010}, {"ENBGR", 1}});
      //  REG_CDAC0_2_0 (010) ENIBG 0 REG_BGR_OFFSET (010) ENBGR (1)

      fw.SetSensorRegisters({{"REG_CDAC1_0", 0}, {"EN_CDAC_T0", 1}, {"EN_CDAC0", 1}, {"REG_CDAC0_7_3", 0b00000}});
      //  REG_CDAC1_0 0  EN_CDAC_T0 1 EN_CDAC0 1 REG_CDAC0_7_3 00000
      
      fw.SetSensorRegisters({{"EN_CDAC1", 1}, {"REG_CDAC1_7_1", 0b0010000}}); ////TODO::  ITHR  32 + prefix 1
      //  EN_CDAC1 1 REG_CDAC1_7_1 (0010000)
      
      fw.SetSensorRegisters({{"REG_CDAC2_6_0", 0b0000101}, {"EN_CDAC_T1", 1}});
      //  REG_CDAC2_6_0 (0000101)  EN_CDAC_T1 1

      fw.SetSensorRegisters({{"REG_VDAC0_4_0", 0b00000}, {"EN_CDAC_T2", 1},  {"EN_CDAC2", 1}, {"REG_CDAC2_7", 0}});
      //  REG_VDAC0_4_0 00000  EN_CDAC_T2 1  EN_CDAC2 1 REG_CDAC2_7 0     
      
      fw.SetSensorRegisters({{"REG_VDAC0_C2_0", 0b000}, {"REG_VDAC0_9_5", 0b00000}});
      //  REG_VDAC0_C2_0 000 REG_VDAC0_9_5 00000

      fw.SetSensorRegisters({{"REG_VDAC1_2_0", 0b110}, {"EN_VDAC0", 1}, {"REG_VDAC0_T", 0b11}, {"REG_VDAC0_C4_3", 0b10}});
      //  REG_VDAC1_2_0 (110) EN_VDAC0 1 REG_VDAC0_T 11 REG_VDAC0_C4_3 10

      fw.SetSensorRegisters({{"REG_VDAC1_C0", 0},{"REG_VDAC1_9_3", 0b0000000}});
      //  REG_VDAC1_C0 0 REG_VDAC1_9_3 0000000
      
      fw.SetSensorRegisters({{"REG_VDAC2_0",1},{"EN_VDAC1", 1},{"REG_VDAC1_T",0b11},{"REG_VDAC1_C4_1",0b1000}});
      //  REG_VDAC2_0 (1) EN_VDAC1 1  REG_VDAC1_T 11 REG_VDAC1_C4_1 1000
      
      fw.SetSensorRegisters({{"REG_VDAC2_8_1", 0b10010101}});
      //  REG_VDAC2_8_1 10010101    
      
      fw.SetSensorRegisters({{"REG_VDAC2_T", 0b11}, {"REG_VDAC2_C", 0b10000}, {"REG_VDAC2_9", 0}});
      //  REG_VDAC2_T 11 REG_VDAC2_C 10000  REG_VDAC2_9 0 
      
      fw.SetSensorRegisters({{"REG_VDAC3_6_0", 0b1000100}, {"EN_VDAC2", 1}});
      //  REG_VDAC3_6_0 1000100  EN_VDAC2 1
      
      fw.SetSensorRegisters({{"REG_VDAC3_C", 0b10000},  {"REG_VDAC3_9_7", 0b010}});
      //  REG_VDAC3_C 10000  REG_VDAC3_9_7 010
      
      fw.SetSensorRegisters({{"REG_MUXO", 0b01}, {"REG_MUX", 0b010}, {"EN_VDAC3", 1}, {"REG_VDAC3_T", 0b11}});
      //  REG_MUXO 01 REG_MUX 010  EN_VDAC3 1 REG_VDAC2_T 11
      
      fw.SetSensorRegisters({{"REG_CDAC_8NA4_0", 0b00010}, {"EN_CDAC_8NA", 0}, {"REG_CDAC_8NA_BGR", 1}, {"REG_SEL_CDAC_8NA", 0}});
      //  REG_CDAC_8NA4_0 (00010)  EN_CDAC_8NA (0)   REG_CDAC_8NA_BGR 1   REG_SEL_CDAC_8NA (0)

      fw.SetSensorRegisters({{"REG_CDAC_8NA_TRIM", 0b00}, {"REG_CDAC_8NA5", 0}});
      //  00000 REG_CDAC_8NA_TRIM 00 REG_CDAC_8NA5 0
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
