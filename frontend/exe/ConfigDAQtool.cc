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

#include "Frontend.hh"

#include "TFile.h"
#include "TTree.h"

#define DEBUG_PRINT 0
#define dprintf(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)

//help message for command
static const std::string help_usage
(R"(
Usage:
--help                  : print usage information, and then quit

--dir     <data_path>   : default[ data ], directory path where data file will be placed
--host    <ip_address>  : default[ 192.168.10.16 ]
--port    <tcp_port>    : default[ 24 ]
--root                  : ttree tfile for data saving

-s json_file: sensor registers [path of  taichupix3_reg.json]
-f json_file: firmware registers [path of firmware_reg.json]
reg.json files are located in folder frontend/resource

'help' command in interactive mode provides detail usage information
)"
);

//help message for linenoise interface
static  const std::string help_usage_linenoise
(R"(
keyword:
common:
config:
data-taking:

example:
  0) quit command line
   > quit

  1) get firmware regiester
   > firmware get FW_REG_NAME

  2) set firmware regiester
   > firmware set FW_REG_NAME 10

  3) get sensor regiester
   > sensor get SN_REG_NAME

  4) set sensor regiester
   > sensor set SN_REG_NAME 10

  10) reset
   > reset

  11) config
   > config

  12) start data taking
   > start

  13) stop data taking
   > stop
)"
);

//get current time to string
std::string getnowstring(){
  char buffer[80];
  std::time_t t = std::time(nullptr);
  size_t n = std::strftime(buffer, sizeof(buffer), "%FT%H%M%S%Z", std::localtime(&t));
  return std::string(buffer, n);
}

//-------------------file functions---------------------
// dir to collect data
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
// txt file
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
// root file
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

//-------------------Async functions and counts-------------------
//count for (valid) packages collected
static  std::atomic_size_t  ga_dataFrameN = 0;
static  std::atomic_size_t  ga_dataFrameN_valid = 0;

static sig_atomic_t g_data_done = 0;
static sig_atomic_t g_watch_done = 0;

std::future<uint64_t> fut_async_watch;
std::future<uint64_t> fut_async_data;
uint64_t AsyncWatchDog();
uint64_t AsyncDataSave(std::FILE *p_fd, TFile *p_rootfd, Frontend *p_daqb);

//-------------------main-------------------------
int main(int argc, char **argv){
  signal(SIGINT, [](int){g_data_done+=1;  g_watch_done+=1;});

  std::string dataFolderPath="data";
  std::string daqbHost_ipstr="192.168.10.16";
  uint16_t daqbPortN = 24;
  int do_verbose = 0;
  bool do_rootfile = false;

  std::string frfile_opt;
  std::string srfile_opt;
  {////////////getopt begin//////////////////
    struct option longopts[] = {{"help",      no_argument, NULL, 'h'},//option -W is reserved by getopt
                                {"verbose",   no_argument, NULL, 'v'},//val
                                {"dir",   required_argument, NULL, 'd'},
                                {"root",  no_argument, NULL, 't'},
                                {"host",  required_argument, NULL, 'o'},
                                {"port",  required_argument, NULL, 'r'},

                                {"srfile",  required_argument, NULL, 's'},
                                {"frfile",  required_argument, NULL, 'f'},
                                {0, 0, 0, 0}};

    if(argc == 1){ //no options
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
      case 'd':
        dataFolderPath = optarg;
        break;
      case 'o':
        daqbHost_ipstr = optarg;
	break;
      case 'r':
        daqbPortN  = atoi(optarg);
        break;
        // help and verbose
      case 'v':
        do_verbose=1;
        //option is set to no_argument
        if(optind < argc && *argv[optind] != '-'){
          do_verbose = std::stoul(argv[optind]);
          optind++;
        }
        break;
      case 't':
	do_rootfile = true;
        break;
      case 'h':
        std::fprintf(stdout, "%s\n", help_usage.c_str());
        std::exit(0);
        break;

      case 'f':
        frfile_opt=optarg;
        break;
      case 's':
        srfile_opt=optarg;
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

  //set DAQ file
  std::filesystem::path data_folder_path(dataFolderPath);
  std::filesystem::path data_folder_path_abs = std::filesystem::absolute(data_folder_path);
  check_and_create_folder(data_folder_path_abs);
  std::fprintf(stdout, "\n\n\ndata dir:   %s\n", data_folder_path_abs.c_str());

  std::FILE *fp_data=0;
  TFile *tf_data=0;
  std::unique_ptr<Frontend> daqbup;
  try{
    daqbup.reset(new Frontend(daqbHost_ipstr, "ConfigDAQtool", 0));
  }catch(...){
    daqbup.reset();
    exit(-1);
  }

  //set register 
  if(srfile_opt.empty() || frfile_opt.empty() ){
    fprintf(stderr, "\ninsufficient options.\n%s\n\n\n",help_usage.c_str());
    return 1;
  }
  std::string reglist_path_sensor = srfile_opt;
  std::string reglist_path_firmware = frfile_opt;  
  std::string reglist_jsstr_sensor   = LoadFileToString(reglist_path_sensor);
  std::string reglist_jsstr_firmware = LoadFileToString(reglist_path_firmware);
  Frontend fw(reglist_jsstr_sensor, reglist_jsstr_firmware, daqbHost_ipstr, "ConfigDAQtool", 0);

  //-------------------linenoise-------------------------
  std::filesystem::path linenoise_history_path = std::filesystem::temp_directory_path() / "tcpcontool.history.txt";
  linenoiseHistoryLoad(linenoise_history_path.string().c_str());
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "quit", "exit",
                                      "reset", "config", "start", "stop",
                                      "sensor", "firmware", "dac", "full",
                                      NULL};
                                   size_t i;
                                   for (i = 0;  examples[i] != NULL; ++i) {
                                     if (strncmp(prefix, examples[i], strlen(prefix)) == 0) {
                                       linenoiseAddCompletion(lc, examples[i]);
                                     }
                                   }
                                 } );

  const char* prompt = "\x1b[1;32mConfigDAQ\x1b[0m> ";    
  while (1) {
    char* result = linenoise(prompt);
    if (result == NULL) {
      break;
    }
    if ( std::regex_match(result, std::regex("\\s*(help)\\s*")) ){
      fprintf(stdout, "%s", help_usage_linenoise.c_str());
    }
    else if ( std::regex_match(result, std::regex("\\s*(quit)|(exit)|(.q)\\s*")) ){
      printf("quiting \n");
      break;
    }

    //basic command: reset, config, start, stop
    else if ( std::regex_match(result, std::regex("\\s*(reset)\\s*")) ){ //reset
      printf("reset\n");
      daqbup->daq_reset();
      g_data_done = 1;
      if(fut_async_data.valid()){
        fut_async_data.get();
      }

      g_watch_done = 1;
      if(fut_async_watch.valid()){
        fut_async_data.get();
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(config)\\s*")) ){ //config
      printf("config\n");
      fw.daq_conf_default();
    }
    else if ( std::regex_match(result, std::regex("\\s*(start)\\s*")) ){ //start data-taking
      printf("start\n");
      std::string nowstr = getnowstring();
      std::string basename_datafile = std::string("data_")+nowstr;
      std::filesystem::path file_rawdata_path = data_folder_path_abs/(basename_datafile+std::string(".dat"));
      std::filesystem::path file_rootdata_path = data_folder_path_abs/(basename_datafile+std::string(".root"));
      if(do_rootfile){
        tf_data = create_and_open_rootfile(file_rootdata_path);
      }else{
        fp_data = create_and_open_file(file_rawdata_path);
      }
      fut_async_watch = std::async(std::launch::async, &AsyncWatchDog);
      fut_async_data = std::async(std::launch::async, &AsyncDataSave, fp_data, tf_data, daqbup.get());
      daqbup->daq_start_run();
    }
    else if ( std::regex_match(result, std::regex("\\s*(stop)\\s*")) ){
      printf("stop\n");
      daqbup->daq_stop_run();
      g_data_done = 1;
      if(fut_async_data.valid())
        fut_async_data.get();
    }
    //---sensor registers---
    else if(std::regex_match(result, std::regex("\\s*(full)\\s*"))){ //open full window
      fw.FlushPixelMask({}, Frontend::MaskType::MASK);
      fw.FlushPixelMask({}, Frontend::MaskType::UNCAL);
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

    //firmware registers
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

    //---no correct command---
    else{
      std::printf("Error, unknown command:    %s\n", result);
    }

    // linenoise history
    linenoiseHistoryAdd(result);
    free(result);
    linenoiseHistorySave(linenoise_history_path.string().c_str());
    linenoiseHistoryFree();
    linenoiseHistoryLoad(linenoise_history_path.string().c_str());
  } //end of while(1) for linenoise

  g_data_done = 1;
  if(fut_async_data.valid())
    fut_async_data.get();

  g_watch_done = 1;
  if(fut_async_watch.valid())
    fut_async_watch.get();
  return 0;
}

//---------------Async functions------------------
uint64_t AsyncWatchDog(){
  ga_dataFrameN = 0;
  ga_dataFrameN_valid = 0;

  auto tp_run_begin = std::chrono::system_clock::now();
  auto tp_old = tp_run_begin;
  size_t st_old_dataFrameN = 0;
  size_t st_old_dataFrameN_valid = 0;


  while(!g_watch_done){
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto tp_now = std::chrono::system_clock::now();
    std::chrono::duration<double> dur_period_sec = tp_now - tp_old;
    std::chrono::duration<double> dur_accu_sec = tp_now - tp_run_begin;
    double sec_period = dur_period_sec.count();
    double sec_accu = dur_accu_sec.count();

    size_t st_dataFrameN = ga_dataFrameN;
    double st_hz_pack_accu = st_dataFrameN / sec_accu;
    double st_hz_pack_period = (st_dataFrameN-st_old_dataFrameN) / sec_period;
    
    size_t st_dataFrameN_valid = ga_dataFrameN_valid;
    double st_hz_pack_accu_valid = st_dataFrameN_valid / sec_accu;
    double st_hz_pack_period_valid = (st_dataFrameN_valid-st_old_dataFrameN_valid) / sec_period;

    tp_old = tp_now;
    st_old_dataFrameN= st_dataFrameN;
    st_old_dataFrameN_valid= st_dataFrameN_valid;
    std::fprintf(stdout,
                 "       pack_avg(%8.2f hz) pack_inst(%8.2f hz) pixel_avg(%8.2f hz) pixel_inst(%8.2f hz) pixel_total(%llu)\r",
                 st_hz_pack_accu,  st_hz_pack_period, st_hz_pack_accu_valid,  st_hz_pack_period_valid, st_dataFrameN_valid);
    std::fflush(stdout);
  }
  std::fprintf(stdout, "\n\n");
  return 0;
}

uint64_t AsyncDataSave(std::FILE *p_fd, TFile *p_rootfd, Frontend *p_daqb){
  std::vector<uint16_t> xc;    // x column
  std::vector<uint16_t> yr;    // y row
  std::vector<uint8_t>  tsc;   // timestamp of chip

  uint8_t  hid; // hardware id
  uint16_t tid; // tlu id
  uint16_t npw; // num of pixelword

  TTree* p_ttree = 0;
  if(p_rootfd){
    p_rootfd->cd();
    std::cout<<"setup  rootfd"<<std::endl;
    p_ttree = p_rootfd->Get<TTree>("tree_pixel");
    if(!p_ttree){
      std::cout<<"creat new ttree"<<std::endl;
      p_ttree = new TTree("tree_pixel","tree_pixel");
      p_ttree->SetDirectory(p_rootfd);
    }

    p_ttree->Branch("xc", &xc);
    p_ttree->Branch("yr", &yr);
    p_ttree->Branch("tsc", &tsc);
    p_ttree->Branch("hid", &hid);
    p_ttree->Branch("tid", &tid);
    p_ttree->Branch("npw", &npw);

  }

  while(!g_data_done){
    auto pack = p_daqb->Front();
    if(!pack){
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    p_daqb->PopFront();

    tid = pack->tid;
    hid = pack->daqid;
    npw = pack->vecpixel.size();

    xc.clear();
    yr.clear();
    tsc.clear();
    for(const auto &pw : pack->vecpixel){
        xc.push_back(pw.xcol);
        yr.push_back(pw.yrow);
        tsc.push_back(pw.tschip);
    }

    ga_dataFrameN++;

    if(pack->CheckDataPack()){
      ga_dataFrameN_valid++;
      if(p_fd){
        for(const auto &pw : pack->vecpixel){ 
          std::fprintf(p_fd, "%hu  %hu  %hu  %lu \n", pw.xcol, pw.yrow, uint16_t(pw.tschip), pack->tid);
        }
        std::fflush(p_fd);
      }
      if(p_ttree){p_ttree->Fill();}
    }
  }


  if(p_fd){std::fclose(p_fd); p_fd = 0;}
  if(p_rootfd){
    p_ttree->Write();
    p_rootfd->Close();
    p_rootfd = 0;
    p_ttree=0;
  }

  return 0;
}
