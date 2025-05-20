#pragma once

#include <mutex>
#include <future>
#include <memory>

#include <cstdio>
#include "mysystem.hh"
#include "TcpConnection.hh"



class TcpConnection;

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

//struct DataPack;
struct DataPack{
  std::vector<PixelWord> vecpixel;
  uint8_t packhead;
  uint8_t daqid;
  uint16_t tid;
  uint16_t len;
  uint16_t packend;
  std::string packraw; 
 
  int MakeDataPack(const std::string& str){
    static size_t n = 0;
    if(n<100){
      n++;
      std::cout<< "==============="<<std::endl;
      std::cout<< "0------0 1------1 2------2 3------3 4------4 5------5 6------6 7------7 "<<std::endl;
      for(auto e: str){
	std::bitset<8> rawbit(e);
	std::cout<< rawbit<<" ";
      }
      std::cout<<std::endl<<"==============="<<std::endl;
    }

    int miniPackLength=8;
    if(str.size()<miniPackLength){
      std::cout<<"DataPack::MakeDataPack: str size wrong="<<str.size()<<std::endl;
      throw;
      return -1;
    }

    packraw = str;
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

    return 4;
  }
  
  bool CheckDataPack(){
    bool check_pack = (packhead==0xaa) && (packend==0xcccc);
    bool check_pixel=true;
    for(size_t n=0; n<vecpixel.size(); n++){
      if(vecpixel[n].isvalid!=1 || vecpixel[n].pattern != 0b0000){
        check_pixel=false;
      }
    }
//    std::cout<<"check_pack:"<<check_pack<<", check_pixel:"<<check_pixel<<std::endl;
    return check_pack && check_pixel;
  };

};

using daqb_packSP = std::shared_ptr<DataPack>;

class daqb{
public:
  std::future<uint64_t> m_fut_async_watch;
  std::vector<daqb_packSP> m_vec_ring_ev;
  daqb_packSP m_ring_end;

  uint64_t m_size_ring{200000};
  std::atomic<uint64_t> m_count_ring_write;
  std::atomic<uint64_t> m_hot_p_read;
  uint64_t m_count_ring_read;
  bool m_is_async_watching{false};

  uint64_t m_extension{0};

  //status variable:
  std::atomic<uint64_t> m_st_n_tg_ev_now{0};
  std::atomic<uint64_t> m_st_n_ev_input_now{0};
  std::atomic<uint64_t> m_st_n_ev_output_now{0};
  std::atomic<uint64_t> m_st_n_ev_bad_now{0};
  std::atomic<uint64_t> m_st_n_ev_overflow_now{0};
  std::atomic<uint64_t> m_st_n_tg_ev_begin{0};

  uint64_t m_st_n_tg_ev_old{0};
  uint64_t m_st_n_ev_input_old{0};
  uint64_t m_st_n_ev_bad_old{0};
  uint64_t m_st_n_ev_overflow_old{0};

  std::string m_st_string;
  std::mutex m_mtx_st;

  uint32_t m_tg_expected{0};
  uint32_t m_flag_wait_first_event{true};

  bool m_isDataAccept{false};
  std::unique_ptr<TcpConnection> m_tcpcon;
public:
  daqb(const std::string& name, const std::string& host, short int port)
    :m_name(name), m_host(host), m_port(port){
  }

  ~daqb();

  void daq_start_run();
  void daq_stop_run();
  void daq_reset();
  void daq_conf_default();

  int perConnProcessRecvMesg(void* pconn, const std::string& pak);

  daqb_packSP& Front();
  void PopFront();
  uint64_t Size();
  void ClearBuffer();
  void ResyncBuffer(){
    m_tcpcon->TCPResyncBuffer();};

  std::string GetStatusString();
  uint64_t AsyncWatchDog();

  std::string m_name;
  std::string m_host;
  short int m_port;

  template<typename ... Args>
  static std::string FormatString( const std::string& format, Args ... args ){
    std::size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 );
  }



  
};
