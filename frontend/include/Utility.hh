
#include <cstddef>
#include <cstdio>
#include <string>
#include <memory>
#include <iostream>


std::string LoadFileToString(const std::string& p);
uint64_t String2Uint64(const std::string& str);

uint8_t LeastNoneZeroOffset(const uint64_t& value);

std::string TimeNowString(const std::string& format);


template<typename ... Args>
static std::string FormatString( const std::string& format, Args ... args ){
  std::size_t size = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
  std::unique_ptr<char[]> buf( new char[ size ] );
  std::snprintf( buf.get(), size, format.c_str(), args ... );
  return std::string( buf.get(), buf.get() + size - 1 );
}

template<typename ... Args>
static std::size_t FormatPrint(std::ostream &os, const std::string& format, Args ... args ){
  std::size_t size = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
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
