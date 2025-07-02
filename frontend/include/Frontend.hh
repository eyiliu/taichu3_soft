#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <regex>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <algorithm>
#include <mutex>
#include <future>
#include <memory>
#include <cstdio>


#include "TcpConnection.hh"
#include "DataPack.hh"
#include "mysystem.hh"
#include "myrapidjson.h"

#include "Utility.hh"

class Frontend{
public:
  enum MaskType {MASK, CAL, UNMASK, UNCAL};

  Frontend(const std::string& netip,
           const std::string& name,
           const uint64_t daqid
    );


  Frontend(const std::string& sensor_jsstr,
           const std::string& firmware_jsstr,
           const std::string& netip,
           const std::string& name,
           const uint64_t daqid
    );


  const std::string& GetName(){return m_name;};

  // bool OpenTCP(const std::string& ip);
  // bool OpenUDP(const std::string& ip);

  void SetFirmwareRegister(const std::string& name, uint64_t value);
  uint64_t GetFirmwareRegister(const std::string& name);

  void SetSensorRegister(const std::string& name, uint64_t value);
  uint64_t GetSensorRegister(const std::string& name);

  void SetSensorRegisters(const std::map<std::string, uint64_t>& mapRegValue );

  void SetBoardDAC(uint32_t ch, double voltage);
  uint64_t SensorRegAddr2GlobalRegAddr(uint64_t addr);

  void FlushPixelMask(const std::set<std::pair<uint16_t, uint16_t>> &colMaskXY,
                      const MaskType maskType);
  std::set<std::pair<uint16_t, uint16_t>> ReadPixelMask_from_file(const std::string& filename);

private:
  void  WriteByte(uint64_t address, uint64_t value);
  uint64_t ReadByte(uint64_t address);

public:


  template<typename T>
  static const std::string Stringify(const T& o){
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    o.Accept(writer);
    return sb.GetString();
  }

  template<typename T>
  static void PrintJson(const T& o){
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
    o.Accept(w);
    std::fwrite(sb.GetString(), 1, sb.GetSize(), stdout);
  }


private:

  std::string m_netip;
  uint64_t m_daqid{0};
  std::string m_name{"unamed"};
  
  rapidjson::Document m_jsdoc_sensor;
  rapidjson::Document m_jsdoc_firmware;

  /////////////////////////////////////////////////////



public:
  std::future<uint64_t> m_fut_async_watch;
  std::vector<DataPackSP> m_vec_ring_ev;
  DataPackSP m_ring_end;

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

  ~Frontend();

  void daq_start_run();
  void daq_stop_run();
  void daq_reset();
  void daq_conf_default();

  int perConnProcessRecvMesg(void* pconn, const std::string& pak);

  DataPackSP& Front();
  void PopFront();
  uint64_t Size();

  void ClearBuffer();
  std::string GetStatusString();
  uint64_t AsyncWatchDog();

};
