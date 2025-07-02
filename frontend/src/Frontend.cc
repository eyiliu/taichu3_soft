#include "Frontend.hh"

#include <regex>
#include <filesystem>
#include <array>
#include <iostream>
#include <thread>

#include "rbcp.hh"
#include "TcpConnection.hh"


#pragma GCC diagnostic ignored "-Wpmf-conversions"


#ifndef DEBUG_PRINT
#define DEBUG_PRINT 0
#endif
#define debug_print(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)

#ifndef INFO_PRINT
#define INFO_PRINT 0
#endif
#define info_print(fmt, ...)                                           \
  do { if (INFO_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)




static const std::string builtin_firmware_reg_str =
#include "firmware_reg.json.hh"
  ;


static const std::string builtin_taichupix3_reg_str =
#include "taichupix3_reg.json.hh"
  ;


Frontend::Frontend(const std::string& netip,
                   const std::string& name,
                   const uint64_t daqid
  ){
  m_jsdoc_firmware.Parse(builtin_firmware_reg_str.c_str());
  if(m_jsdoc_firmware.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(m_jsdoc_firmware.GetParseError()), m_jsdoc_firmware.GetErrorOffset());
    throw;
  }

  m_jsdoc_sensor.Parse(builtin_taichupix3_reg_str.c_str());
  if(m_jsdoc_sensor.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(m_jsdoc_sensor.GetParseError()), m_jsdoc_sensor.GetErrorOffset());
    throw;
  }
  m_netip = netip;
  m_name = name;
  m_daqid = daqid;
  m_extension = daqid;
}


Frontend::Frontend(const std::string& sensor_jsstr,
                   const std::string& firmware_jsstr,
                   const std::string& netip,
                   const std::string& name,
                   const uint64_t daqid
  ){
  m_jsdoc_firmware.Parse(firmware_jsstr.empty()? builtin_firmware_reg_str.c_str():firmware_jsstr.c_str());
  if(m_jsdoc_firmware.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(m_jsdoc_firmware.GetParseError()), m_jsdoc_firmware.GetErrorOffset());
    throw;
  }

  m_jsdoc_sensor.Parse(sensor_jsstr.empty()? builtin_taichupix3_reg_str.c_str():sensor_jsstr.c_str());
  if(m_jsdoc_sensor.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(m_jsdoc_sensor.GetParseError()), m_jsdoc_sensor.GetErrorOffset());
    throw;
  }
  m_netip = netip;
  m_name = name;
  m_daqid = daqid;
  m_extension = daqid;
}

void  Frontend::WriteByte(uint64_t address, uint64_t value){

  DebugFormatPrint(std::cout, "WriteByte( address= %#016x ,  value= %#016x )\n", address, value);

  std::cout<< address << "  " <<value<<std::endl;
  rbcp r(m_netip);
  std::string recvStr(100, 0);
  r.DispatchCommand("wrb",  address, value, NULL);
  // r.DispatchCommand("wrb",  address, value, NULL);
};

uint64_t Frontend::ReadByte(uint64_t address){
  DebugFormatPrint(std::cout, "ReadByte( address= %#016x)\n", address);
  uint8_t reg_value=0;
  rbcp r(m_netip);
  std::string recvStr(100, 0);
  // TODO: wait readback compatible firmware, always return zero
  r.DispatchCommand("rd", address, 1, &recvStr);
  reg_value=recvStr[0];
  DebugFormatPrint(std::cout, "ReadByte( address= %#016x) return value= %#016x\n", address, reg_value);
  return reg_value;
};

void Frontend::SetFirmwareRegister(const std::string& name, uint64_t value){
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name= %s ,  value= %#016x )\n", __func__, __func__, name.c_str(), value);
  static const std::string array_name("FIRMWARE_REG");
  auto& json_array = m_jsdoc_firmware[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(!json_addr.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format, requires a json string<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }

    uint64_t address = String2Uint64(json_addr.GetString());

    WriteByte(address, value);
    flag_found_reg = true;
    break;
  }

  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
}

void Frontend::SetSensorRegisters(const std::map<std::string, uint64_t>& mapRegValue ){
  static const std::string array_name("SENSOR_REG");
  auto& json_array = m_jsdoc_sensor[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }

  std::map<uint64_t, std::pair<uint64_t, uint64_t>> mapRegMaskValue;
  std::map<uint64_t, bool> mapRegReadable;
  for(auto & [name, value]: mapRegValue){
    bool flag_found_reg = false;
    for(auto& json_reg: json_array.GetArray()){
      if( json_reg["name"] != name )
        continue;
      auto& json_addr = json_reg["address"];
      if(!json_addr.IsString()){
        FormatPrint(std::cerr, "ERROR<%s>: unknown address format, requires a json string<%s>\n", __func__, Stringify(json_addr).c_str());
        throw;
      }
      uint64_t address = String2Uint64(json_addr.GetString());

      auto& json_mask = json_reg["mask"];
      if(!json_mask.IsString()){
        FormatPrint(std::cerr, "ERROR<%s>: unknown mask format, requires a json string<%s>\n", __func__, Stringify(json_mask).c_str());
        throw;
      }
      uint64_t mask = String2Uint64(json_mask.GetString());
      uint8_t offset = LeastNoneZeroOffset(mask);
      DebugFormatPrint(std::cout, "INFO<%s>:sensor reg  name=%s  mask=%#08x bitoffset=%u \n", __func__, name.c_str(),  mask, offset);

      auto& json_mode = json_reg["mode"];
      if(!json_mode.IsString()){
        FormatPrint(std::cerr, "ERROR<%s>: unknown mode format, requires a json string<%s>\n", __func__, Stringify(json_mode).c_str());
        throw;
      }
      if(std::string(json_mode.GetString()).find('r')||std::string(json_mode.GetString()).find('R')){
        mapRegReadable[address] = true;
      }
      else{
        mapRegReadable[address] = false;
      }

      if(mapRegMaskValue.find(address)==mapRegMaskValue.end()){
        mapRegMaskValue.insert({address, {mask, value<<offset}});
      }
      else{
        auto& [mask_ori, value_ori]  = mapRegMaskValue[address];
        if( (mask_ori & mask) != 0 ){
          FormatPrint(std::cerr, "ERROR<%s>: mask overlap\n", __func__);
          throw;
        }
        mapRegMaskValue[address] = {(mask | mask_ori) ,  ((value<<offset) & mask) | (value_ori & ~mask)};
      }
      flag_found_reg = true;
      break;
    }
    if(!flag_found_reg){
      FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
      throw;
    }
  }

  for(auto & [address, maskValue]: mapRegMaskValue){
    auto &[mask, value] = maskValue;

    uint64_t value_ori = 0;
    if(mapRegReadable[address]){
      // value_ori = ReadByte(SensorRegAddr2GlobalRegAddr(address));
      WriteByte(0x0022,SensorRegAddr2GlobalRegAddr(address));
      WriteByte(0x0023,0);
      WriteByte(0x0021,1);
      value_ori = ReadByte(0x0024);
    }
    // WriteByte(SensorRegAddr2GlobalRegAddr(address), (value & mask) | (value_ori & ~mask) );
    if(SensorRegAddr2GlobalRegAddr(address)==0b00110){
      WriteByte(0x0022,SensorRegAddr2GlobalRegAddr(address));
      WriteByte(0x0023,value);
      WriteByte(0x0021,0);
    }
    else
    {
      WriteByte(0x0022,SensorRegAddr2GlobalRegAddr(address));
      WriteByte(0x0023,(value & mask) | (value_ori & ~mask));
      WriteByte(0x0021,0);
    }
  }

}

void Frontend::SetSensorRegister(const std::string& name, uint64_t value){
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ,  value=%#016x )\n", __func__, __func__, name.c_str(), value);
  static const std::string array_name("SENSOR_REG");
  auto& json_array = m_jsdoc_sensor[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }

  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(!json_addr.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format, requires a json string<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }
    uint64_t address = String2Uint64(json_addr.GetString());

    auto& json_mask = json_reg["mask"];
    if(!json_mask.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>: unknown mask format, requires a json string<%s>\n", __func__, Stringify(json_mask).c_str());
      throw;
    }
    uint64_t mask = String2Uint64(json_mask.GetString());
    uint8_t offset = LeastNoneZeroOffset(mask);
    DebugFormatPrint(std::cout, "INFO<%s>:sensor reg  name=%s  mask=%#08x bitoffset=%u \n", __func__, name.c_str(),  mask, offset);

    auto& json_mode = json_reg["mode"];
    if(!json_mode.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>: unknown mode format, requires a json string<%s>\n", __func__, Stringify(json_mode).c_str());
      throw;
    }

    uint64_t value_ori = 0;
    if(std::string(json_mode.GetString()).find('r')||std::string(json_mode.GetString()).find('R')){
      // value_ori = ReadByte(SensorRegAddr2GlobalRegAddr(address));
      WriteByte(0x0022,SensorRegAddr2GlobalRegAddr(address));
      WriteByte(0x0023,0);
      WriteByte(0x0021,1);
      value_ori = ReadByte(0x0024);

    }

    // WriteByte(SensorRegAddr2GlobalRegAddr(address), ((value<<offset) & mask) | (value_ori & ~mask) );

    if(SensorRegAddr2GlobalRegAddr(address)==0b00110){
      WriteByte(0x0022,SensorRegAddr2GlobalRegAddr(address));
      WriteByte(0x0023,value);
      WriteByte(0x0021,0);
    }
    else{
      WriteByte(0x0022,SensorRegAddr2GlobalRegAddr(address));
      WriteByte(0x0023,((value<<offset) & mask) | (value_ori & ~mask));
      WriteByte(0x0021,0);
    }

    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
}

uint64_t Frontend::SensorRegAddr2GlobalRegAddr(uint64_t addr){

  // uint64_t addr_base = 0x00010000;
  // //0001 0000 0000 10xx xxx0
  // // wrap 5bit addr into    0b10xxxxx0
  // uint64_t addr_wrap =      0b10000000;
  // uint64_t addr_sensor = addr;
  uint64_t addr_sensor_mask = 0b11111;
  // uint64_t addr_sensor_offset = 1;

  // addr_wrap = addr_wrap | ((addr_sensor & addr_sensor_mask) <<addr_sensor_offset);

  uint64_t global_addr = addr_sensor_mask & addr;

  return global_addr;
}


uint64_t Frontend::GetFirmwareRegister(const std::string& name){
  DebugFormatPrint(std::cout, "INFO<%s>:  %s( name=%s )\n", __func__, __func__, name.c_str());
  static const std::string array_name("FIRMWARE_REG");
  auto& json_array = m_jsdoc_firmware[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  uint64_t value=-1;
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(!json_addr.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format, requires a json string<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    } 
    uint64_t address = String2Uint64(json_addr.GetString());
    value = ReadByte(address);
    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ) return value=%#016x \n", __func__, __func__, name.c_str(), value);
  return value;
}


uint64_t Frontend::GetSensorRegister(const std::string& name){
  DebugFormatPrint(std::cout, "INFO<%s>:  %s( name=%s )\n",__func__, __func__, name.c_str());
  static const std::string array_name("SENSOR_REG");
  auto& json_array = m_jsdoc_sensor[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  uint64_t value;
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(!json_addr.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format, requires a json string<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }
    uint64_t address = String2Uint64(json_addr.GetString());
    // uint64_t valueRead  = ReadByte(SensorRegAddr2GlobalRegAddr(address));
    WriteByte(0x0022,SensorRegAddr2GlobalRegAddr(address));
    WriteByte(0x0023,0);
    WriteByte(0x0021,1);
    uint64_t valueRead = ReadByte(0x0024);

    auto& json_mask = json_reg["mask"];
    if(!json_mask.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>: unknown mask format, requires a json string<%s>\n", __func__, Stringify(json_mask).c_str());
      throw;
    }
    uint64_t mask = String2Uint64(json_mask.GetString());
    uint8_t offset = LeastNoneZeroOffset(mask);

    value  = (valueRead & mask) >> offset;

    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ) return value=%#016x \n", __func__, __func__, name.c_str(), value);
  return value;
}

void Frontend::SetBoardDAC(uint32_t ch, double voltage){
  //da8004
  double ref_voltage = 2.5;
  uint32_t dacn =  0xffff * voltage/ref_voltage;
  uint32_t dacn_mask = 0xffff;
  uint32_t dacn_offset = 4;

  uint32_t ch_mask = 0b1111;
  uint32_t ch_offset = 20;

  uint32_t cmd = 0b0010;
  uint32_t cmd_mask = 0b1111;
  uint32_t cmd_offset = 24;

  uint32_t cmdword = ((dacn&dacn_mask)<<dacn_offset) | ((ch&ch_mask)<<ch_offset) | ((cmd&cmd_mask)<<cmd_offset);
  SetFirmwareRegister("DAC_NSYNC", 1);
  SetFirmwareRegister("DAC_NSYNC", 0);
  for(int i=31; i>=0; i--){ //MSB first
    SetFirmwareRegister("DAC_SCLK", 0);
    SetFirmwareRegister("DAC_DIN", ((cmdword>>i) & 0b1)?1:0 );
    SetFirmwareRegister("DAC_SCLK", 1);
  }
  SetFirmwareRegister("DAC_SCLK", 0);
  SetFirmwareRegister("DAC_NSYNC", 1);
}



void Frontend::FlushPixelMask(const std::set<std::pair<uint16_t, uint16_t>> &colMaskXY, MaskType maskType){
  std::array<std::array<bool, 512>, 1024> maskMat;
  for(auto &maskXCol : maskMat){
    for(auto &mask : maskXCol){
      mask = false;
    }
  }
  for(const auto& [xRow, yCol] : colMaskXY){
    maskMat[xRow][yCol] = true;
    std::cout<<xRow<<" "<<yCol<<std::endl;
  }

  // mask_en
  // std::cout<< "56    63 48    55 40    47 32    39 24    31 16    23 8     15 0      7"<<std::endl;
  // std::cout<< "0------- 1------- 2------- 3------- 4------- 5------- 6------- 7-------"<<std::endl;
  std::vector<uint8_t> vecXColMaskByte_latest;
  for(int xRow= 0;xRow<=1023; xRow++){
    std::vector<uint8_t> vecXColMaskByte;
    uint8_t  maskByte = 0;
    for(int yCol  = 511; yCol>=0; yCol--){
      uint8_t bitPos = yCol%8;
      uint8_t bitMask = 1<<bitPos;
      uint8_t bitValue = maskMat[xRow][yCol]; //get from config

      //revert
      if(maskType == MaskType::UNMASK || maskType == MaskType::UNCAL){
        bitValue = ~(bool(bitValue));
      }

      maskByte = (maskByte & (~bitMask)) | (bitValue << bitPos);
      if(bitPos==0){
        vecXColMaskByte.push_back(maskByte);
        maskByte=0;
      }
    }

    if(vecXColMaskByte != vecXColMaskByte_latest){
      for(const auto & maskByte:  vecXColMaskByte){
        // std::bitset<8> rawbit(maskByte);
        // std::cout<< rawbit<<" ";
        // SetSensorRegister("PIXELMASK_DATA", maskByte);
        WriteByte(0x0022,6);
        WriteByte(0x0023,maskByte);
        WriteByte(0x0021,0);
      }
      vecXColMaskByte_latest = vecXColMaskByte;
    }
    WriteByte(0x0022,7);
    WriteByte(0x0023,0);
    WriteByte(0x0021,0);
    // SetSensorRegisters({{"LOADC_E", 0},{"LOADM_E", 0}});
    // std::cout<<"  col #"<<xCol<<std::endl;
  }
  if(maskType == MaskType::CAL || maskType == MaskType::UNCAL ){
    SetFirmwareRegister("load_c", 0);
    SetFirmwareRegister("load_c", 1);
    SetFirmwareRegister("load_c", 0);
    std::cout<<"load c successfully"<<std::endl;
  }
  else{
    SetFirmwareRegister("load_m", 0);
    SetFirmwareRegister("load_m", 1);
    SetFirmwareRegister("load_m", 0);
    std::cout<<"load m successfully"<<std::endl;
  }
}

std::set<std::pair<uint16_t, uint16_t>> Frontend::ReadPixelMask_from_file(const std::string& filename)
{
  std::fstream file_read_stream;
  std::filesystem::path p(filename);
  if (p.extension() != ".txt")
  {
    std::cerr << "File: " << filename << " is not a txt file." << std::endl;
    exit(0);
  }
  if (std::filesystem::exists(filename))
  {
    try
    {
      file_read_stream.open(filename, std::ios::in);
      std::cout << "File: " << filename << " is open " << std::endl;
      // Use the file_read_stream object for reading the file
      // ...
    }
    catch (const std::exception &e)
    {
      std::cout << "Error opening file: " << e.what() << std::endl;
      exit(0);
    }
  }
  else
  {
    std::cout << "File: " << filename << " don't exist." << std::endl;
    exit(0);
  }
  std::set<std::pair<uint16_t, uint16_t>> pixel_data_set;
  std::string line;
  while (std::getline(file_read_stream, line))
  {
    std::istringstream iss(line);
    uint16_t pixel_mask_row;
    uint16_t pixel_mask_col;
    // iss >> pixel_mask_row >> pixel_mask_col;
    if (iss >> pixel_mask_row >> pixel_mask_col)
    {
      if(pixel_mask_row<0 || pixel_mask_row>1023 || pixel_mask_col<0 || pixel_mask_col>511){
        std::cerr << "Invalid pixel mask coordinates: " << pixel_mask_row << ", " << pixel_mask_col << std::endl;
        continue;
      }
      pixel_data_set.insert({pixel_mask_row, pixel_mask_col});
    }
    else
    {
      std::cerr << "Invalid line format: " << line << std::endl;
    }
  }
  file_read_stream.close();
  std::cout << "File: " << filename << " is closed " << std::endl;
  return pixel_data_set;
}

//===============================================




Frontend::~Frontend(){
  m_tcpcon.reset();

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();
}

void Frontend::daq_start_run(){
  m_vec_ring_ev.clear();
  m_vec_ring_ev.resize(m_size_ring);
  m_count_ring_write = 0;
  m_count_ring_read = 0;
  m_hot_p_read = m_size_ring -1; // tail

  m_flag_wait_first_event = true;

  m_st_n_tg_ev_now =0;
  m_st_n_ev_input_now =0;
  m_st_n_ev_output_now =0;
  m_st_n_ev_bad_now =0;
  m_st_n_ev_overflow_now =0;
  m_st_n_tg_ev_begin = 0;

  m_isDataAccept= true;
  m_tcpcon =  TcpConnection::connectToServer(m_netip,  24, reinterpret_cast<FunProcessMessage>(&Frontend::perConnProcessRecvMesg), nullptr, this);

  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Frontend::AsyncWatchDog, this);
  }

  SetFirmwareRegister("upload_data",1);
  return;
}

void Frontend::daq_stop_run(){
  SetFirmwareRegister("upload_data", 0);

  m_isDataAccept= false;
  m_is_async_watching = false;
  if(m_fut_async_watch.valid()){
    m_fut_async_watch.get();
  }

  SetFirmwareRegister("chip_reset", 0);
  SetFirmwareRegister("chip_reset", 1);
  SetFirmwareRegister("global_reset", 1);
  SetFirmwareRegister("all_buffer_reset", 1);
  return;
}

void Frontend::daq_reset(){

  SetFirmwareRegister("upload_data", 0);

  m_isDataAccept= false;
  m_is_async_watching = false;
  if(m_fut_async_watch.valid()){
    m_fut_async_watch.get();
  }

  SetFirmwareRegister("chip_reset", 0);
  SetFirmwareRegister("chip_reset", 1);
  SetFirmwareRegister("global_reset", 1);
  SetFirmwareRegister("all_buffer_reset", 1);

  return;
}


void Frontend::daq_conf_default(){
  SetFirmwareRegister("upload_data", 0);
  SetFirmwareRegister("chip_reset", 0);
  SetFirmwareRegister("chip_reset", 1);
  SetFirmwareRegister("global_reset", 1);
  SetFirmwareRegister("all_buffer_reset", 1);
  SetFirmwareRegister("set_daq_id", m_daqid);
  SetFirmwareRegister("global_work_mode", 0);
  // need to set daq id and trigger mode

  SetSensorRegister("RCKI", 1);
  // BSEL 0 ISEL1 0 ISEL0 0 EXCKS 0 DSEL 0 CKESEL 0 RCKI (1) RCKO 0

  SetSensorRegisters({{"TRIGN",1}, {"CPRN",1}, {"DOFREQ", 0b01} });
  // TRIGN (1) CPRN (1) DOFREQ 01 SMOD 0 CTM 0 SPI_D 0 TMOD 0
  // 11010000

  SetSensorRegisters({{"REG_BGR_TC", 0b01},{"C_MASK_EN", 0}});
  // REG_BGR_TC 01 BPLDO 0 LDO_REG1 0 LDO_REG0 0 C_MASK_EN 0 ENTP 0 EN10B 0
  // 01000000

  SetSensorRegisters({{"PSET", 1}, {"OISEL",0}, {"OPSEL", 0}});
  // RESERVED13N7_4 0000 RESERVED13N3 0 PSET (1) OISEL 0(x) OPSEL 0(x)

  // SetFirmwareRegister("SER_DELAY", 0x04);
  SetFirmwareRegister("load_m", 0xff);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  FlushPixelMask({}, Frontend::MaskType::MASK);
  FlushPixelMask({}, Frontend::MaskType::UNCAL);

  // voltage
  // SetBoardDAC(1, 1.6);
  // SetBoardDAC(0, 0.47);
  // SetBoardDAC(2, 1.6512);

  SetSensorRegisters({{"REG_CDAC0_2_0", 0b010}, {"ENIBG", 0}, {"REG_BGR_OFFSET", 0b010}, {"ENBGR", 1}});
  //  REG_CDAC0_2_0 (010) ENIBG 0 REG_BGR_OFFSET (010) ENBGR (1)

  SetSensorRegisters({{"REG_CDAC1_0", 0}, {"EN_CDAC_T0", 1}, {"EN_CDAC0", 1}, {"REG_CDAC0_7_3", 0b00000}});
  //  REG_CDAC1_0 0  EN_CDAC_T0 1 EN_CDAC0 1 REG_CDAC0_7_3 00000

  SetSensorRegisters({{"EN_CDAC1", 1}, {"REG_CDAC1_7_1", 0b0010000}}); ////TODO::  ITHR  32 + prefix 1
  //  EN_CDAC1 1 REG_CDAC1_7_1 (0010000)

  SetSensorRegisters({{"REG_CDAC2_6_0", 0b0000101}, {"EN_CDAC_T1", 1}});
  //  REG_CDAC2_6_0 (0000101)  EN_CDAC_T1 1

  SetSensorRegisters({{"REG_VDAC0_4_0", 0b00000}, {"EN_CDAC_T2", 1},  {"EN_CDAC2", 1}, {"REG_CDAC2_7", 0}});
  //  REG_VDAC0_4_0 00000  EN_CDAC_T2 1  EN_CDAC2 1 REG_CDAC2_7 0

  SetSensorRegisters({{"REG_VDAC0_C2_0", 0b000}, {"REG_VDAC0_9_5", 0b00000}});
  //  REG_VDAC0_C2_0 000 REG_VDAC0_9_5 00000

  SetSensorRegisters({{"REG_VDAC1_2_0", 0b110}, {"EN_VDAC0", 1}, {"REG_VDAC0_T", 0b11}, {"REG_VDAC0_C4_3", 0b10}});
  //  REG_VDAC1_2_0 (110) EN_VDAC0 1 REG_VDAC0_T 11 REG_VDAC0_C4_3 10

  SetSensorRegisters({{"REG_VDAC1_C0", 0},{"REG_VDAC1_9_3", 0b0000000}});
  //  REG_VDAC1_C0 0 REG_VDAC1_9_3 0000000

  SetSensorRegisters({{"REG_VDAC2_0",1},{"EN_VDAC1", 1},{"REG_VDAC1_T",0b11},{"REG_VDAC1_C4_1",0b1000}});
  //  REG_VDAC2_0 (1) EN_VDAC1 1  REG_VDAC1_T 11 REG_VDAC1_C4_1 1000

  SetSensorRegisters({{"REG_VDAC2_8_1", 0b10010101}});
  //  REG_VDAC2_8_1 10010101

  SetSensorRegisters({{"REG_VDAC2_T", 0b11}, {"REG_VDAC2_C", 0b10000}, {"REG_VDAC2_9", 0}});
  //  REG_VDAC2_T 11 REG_VDAC2_C 10000  REG_VDAC2_9 0

  SetSensorRegisters({{"REG_VDAC3_6_0", 0b1000100}, {"EN_VDAC2", 1}});
  //  REG_VDAC3_6_0 1000100  EN_VDAC2 1

  SetSensorRegisters({{"REG_VDAC3_C", 0b10000},  {"REG_VDAC3_9_7", 0b010}});
  //  REG_VDAC3_C 10000  REG_VDAC3_9_7 010

  SetSensorRegisters({{"REG_MUXO", 0b01}, {"REG_MUX", 0b010}, {"EN_VDAC3", 1}, {"REG_VDAC3_T", 0b11}});
  //  REG_MUXO 01 REG_MUX 010  EN_VDAC3 1 REG_VDAC2_T 11

  SetSensorRegisters({{"REG_CDAC_8NA4_0", 0b00010}, {"EN_CDAC_8NA", 0}, {"REG_CDAC_8NA_BGR", 1}, {"REG_SEL_CDAC_8NA", 0}});
  //  REG_CDAC_8NA4_0 (00010)  EN_CDAC_8NA (0)   REG_CDAC_8NA_BGR 1   REG_SEL_CDAC_8NA (0)

  SetSensorRegisters({{"REG_CDAC_8NA_TRIM", 0b00}, {"REG_CDAC_8NA5", 0}});
  //  00000 REG_CDAC_8NA_TRIM 00 REG_CDAC_8NA5 0

  return;
}

int Frontend::perConnProcessRecvMesg(void* pconn, const std::string& str){
  static size_t s_n = 0;
  if(!m_isDataAccept){
    std::cout<< "msg is dropped"<<std::endl;
    return 0;
  }

  DataPackSP df(new DataPack);

  df->MakeDataPack(str);
  s_n ++;

  m_st_n_ev_input_now ++;
  uint64_t next_p_ring_write = m_count_ring_write % m_size_ring;
  if(next_p_ring_write == m_hot_p_read){
    // buffer full, permanent data lose
    m_st_n_ev_overflow_now ++;
    return 0;
  }

  m_vec_ring_ev[next_p_ring_write] = df;
  m_count_ring_write ++;
  return 1;
}

std::string Frontend::GetStatusString(){
  std::unique_lock<std::mutex> lk(m_mtx_st);
  return m_st_string;
}

DataPackSP& Frontend::Front(){
  if(m_count_ring_write > m_count_ring_read) {
    uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
    m_hot_p_read = next_p_ring_read;
    // keep hot read to prevent write-overlapping
    return m_vec_ring_ev[next_p_ring_read];
  }
  else{
    return m_ring_end;
  }
}

void Frontend::PopFront(){
  if(m_count_ring_write > m_count_ring_read) {
    uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
    m_hot_p_read = next_p_ring_read;
    // keep hot read to prevent write-overlapping
    m_vec_ring_ev[next_p_ring_read].reset();
    m_count_ring_read ++;
  }
}

uint64_t Frontend::Size(){
  return  m_count_ring_write - m_count_ring_read;
}




void Frontend::ClearBuffer(){
  m_count_ring_write = m_count_ring_read;
  m_vec_ring_ev.clear();
}




uint64_t Frontend::AsyncWatchDog(){
  std::chrono::system_clock::time_point m_tp_old;
  std::chrono::system_clock::time_point m_tp_run_begin;

  m_tp_run_begin = std::chrono::system_clock::now();
  m_tp_old = m_tp_run_begin;
  m_is_async_watching = true;

  m_st_n_tg_ev_old =0;
  m_st_n_ev_input_old = 0;
  m_st_n_ev_bad_old =0;
  m_st_n_ev_overflow_old = 0;

  while(m_is_async_watching){
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint64_t st_n_tg_ev_begin = m_st_n_tg_ev_begin;
    uint64_t st_n_tg_ev_now = m_st_n_tg_ev_now;
    uint64_t st_n_ev_input_now = m_st_n_ev_input_now;
    uint64_t st_n_ev_bad_now = m_st_n_ev_bad_now;
    uint64_t st_n_ev_overflow_now = m_st_n_ev_overflow_now;

    // time
    auto tp_now = std::chrono::system_clock::now();
    std::chrono::duration<double> dur_period_sec = tp_now - m_tp_old;
    std::chrono::duration<double> dur_accu_sec = tp_now - m_tp_run_begin;
    double sec_period = dur_period_sec.count();
    double sec_accu = dur_accu_sec.count();

    // period
    uint64_t st_n_tg_ev_period = st_n_tg_ev_now - m_st_n_tg_ev_old;
    uint64_t st_n_ev_input_period = st_n_ev_input_now - m_st_n_ev_input_old;
    uint64_t st_n_ev_bad_period = st_n_ev_bad_now - m_st_n_ev_bad_old;
    uint64_t st_n_ev_overflow_period = st_n_ev_overflow_now - m_st_n_ev_overflow_old;

    // ratio
    //double st_output_vs_input_accu = st_n_ev_input_now? st_ev_output_now / st_ev_input_now : 1;
    double st_bad_vs_input_accu = st_n_ev_input_now? 1.0 * st_n_ev_bad_now / st_n_ev_input_now : 0;
    double st_overflow_vs_input_accu = st_n_ev_input_now? 1.0 *  st_n_ev_overflow_now / st_n_ev_input_now : 0;
    double st_input_vs_trigger_accu = st_n_ev_input_now? 1.0 * st_n_ev_input_now / (st_n_tg_ev_now - st_n_tg_ev_begin + 1) : 1;
    //double st_output_vs_input_period = st_ev_input_period? st_ev_output_period / st_ev_input_period : 1;
    double st_bad_vs_input_period = st_n_ev_input_period? 1.0 * st_n_ev_bad_period / st_n_ev_input_period : 0;
    double st_overflow_vs_input_period = st_n_ev_input_period? 1.0 *  st_n_ev_overflow_period / st_n_ev_input_period : 0;
    double st_input_vs_trigger_period = st_n_tg_ev_period? 1.0 *  st_n_ev_input_period / st_n_tg_ev_period : 1;

    // hz
    double st_hz_tg_accu = (st_n_tg_ev_now - st_n_tg_ev_begin + 1) / sec_accu ;
    double st_hz_input_accu = st_n_ev_input_now / sec_accu ;

    double st_hz_tg_period = st_n_tg_ev_period / sec_period ;
    double st_hz_input_period = st_n_ev_input_period / sec_period ;

    std::string st_string_new =
      FormatString("L<%u> event(%d)/trigger(%d - %d)=Ev/Tr(%.4f) dEv/dTr(%.4f) tr_accu(%.2f hz) ev_accu(%.2f hz) tr_period(%.2f hz) ev_period(%.2f hz)",
                             m_extension, st_n_ev_input_now, st_n_tg_ev_now, st_n_tg_ev_begin, st_input_vs_trigger_accu, st_input_vs_trigger_period,
                             st_hz_tg_accu, st_hz_input_accu, st_hz_tg_period, st_hz_input_period
        );

    {
      std::unique_lock<std::mutex> lk(m_mtx_st);
      m_st_string = std::move(st_string_new);
    }

    //write to old
    m_st_n_tg_ev_old = st_n_tg_ev_now;
    m_st_n_ev_input_old = st_n_ev_input_now;
    m_st_n_ev_bad_old = st_n_ev_bad_now;
    m_st_n_ev_overflow_old = st_n_ev_overflow_now;
    m_tp_old = tp_now;
  }
  return 0;
}
