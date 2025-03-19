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
#include <vector>
#include <utility>
#include <algorithm>

#include "mysystem.hh"
#include "myrapidjson.h"

class Frontend{
public:
  Frontend(const std::string& sensor_jsstr,
	   const std::string& firmware_jsstr,
	   const std::string& netip);


  // bool OpenTCP(const std::string& ip);
  // bool OpenUDP(const std::string& ip);

  void SetFirmwareRegister(const std::string& name, uint64_t value);
  uint64_t GetFirmwareRegister(const std::string& name);

  void SetSensorRegister(const std::string& name, uint64_t value);
  uint64_t GetSensorRegister(const std::string& name);  

  void SetSensorRegsters(const std::map<std::string, uint64_t>& mapRegValue );
  
private:
  void  WriteByte(uint64_t address, uint64_t value);
  uint64_t ReadByte(uint64_t address);

public:
  static std::string LoadFileToString(const std::string& p);
  static uint64_t String2Uint64(const std::string& str);

  
  template<typename ... Args>
  static std::string FormatString( const std::string& format, Args ... args ){
    std::size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
    std::unique_ptr<char[]> buf( new char[ size ] ); 
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 );
  }

  template<typename ... Args>
  static std::size_t FormatPrint(std::ostream &os, const std::string& format, Args ... args ){
    std::size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
    std::unique_ptr<char[]> buf( new char[ size ] ); 
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    std::string formated_string( buf.get(), buf.get() + size - 1 );
    os<<formated_string<<std::flush;
    return formated_string.size();
  }

  template<typename ... Args>
  static std::size_t DebugFormatPrint(std::ostream &os, const std::string& format, Args ... args ){  
    // return 0;
    std::size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
    std::unique_ptr<char[]> buf( new char[ size ] ); 
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    std::string formated_string( buf.get(), buf.get() + size - 1 );
    os<<formated_string<<std::flush;
    return formated_string.size();
  }
  
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
  
  // rapidjson::CrtAllocator m_jsa;
  // rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> m_js_conf;
  // rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> m_js_reg_cmd;  
  rapidjson::Document m_jsdoc_sensor;
  rapidjson::Document m_jsdoc_firmware;
};
