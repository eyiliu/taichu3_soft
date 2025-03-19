#pragma once

#include <mutex>
#include <future>
#include <memory>

#include <cstdio>
#include "mysystem.hh"
#include "TcpConnection.hh"



class TcpConnection;

struct DataPack;
  
struct DataPack{
  uint16_t xcol; 
  uint16_t yrow;
  uint8_t  tschip;
  uint8_t  isvalid;
  uint8_t  pattern;
  uint8_t  idchip;
  uint32_t tsfpga;
  uint64_t raw;
  
  static int MakeDataPack(const std::string& str, DataPack& pack){
    if(str.size()!=8){
      return -1;
    }

    pack.raw = *reinterpret_cast<const uint64_t*>(str.data());
    uint64_t v  = BE64TOH(*reinterpret_cast<const uint64_t*>(str.data()));
    
    
    pack.pattern =  v & 0xf;
    pack.yrow    = (v>> 4) & 0x3ff;
    pack.xcol    = (v>> (4+10)) & 0x1ff;
    pack.tschip  = (v>> (4+10+9)) & 0xff;
    pack.tsfpga  = (v>> (4+10+9+8)) & 0xfffffff;
    pack.isvalid = (v>> (4+10+9+8+28)) & 0x1;
    pack.idchip  = (v>> (4+10+9+8+28+1)) & 0xf;
    return 4;
  }
  
  bool CheckDataPack(){
    return (idchip == 0b1101) && (isvalid == 1) && (pattern == 0b0000);
  };

  bool testData(){
    uint64_t Bv  = BE64TOH(raw);;
    uint64_t Lv  = LE64TOH(raw);;

    std::bitset<64> Bvbit(Bv);
    std::cout<< "BE " << "iiiivffffffffffffffffffffffffffffsssssssscccccccccrrrrrrrrrrpppp"<<std::endl;
    std::cout<< "BE " << Bvbit <<std::endl;
  
    return 0;
  }
  
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
