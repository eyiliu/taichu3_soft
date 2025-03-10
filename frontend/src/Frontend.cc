#include "Frontend.hh"


#include <regex>
#include <filesystem>

#include "rbcp.hh"


#define OFFSET_ADDR_SENSOR_IN_FIRMWARE 0x00010000

uint8_t LeastNoneZeroOffset(const uint64_t& value){
  static const uint8_t bitmap[]=
    {0, 0, 1, 39, 2, 15, 40, 23, 3, 12, 16, 59, 41, 19, 24, 54, 4,
     0, 13, 10, 17, 62, 60, 28, 42, 30, 20, 51, 25, 44, 55, 47, 5, 32,
     0, 38, 14, 22, 11, 58, 18, 53, 63, 9, 61, 27, 29, 50, 43, 46, 31,
     37, 21, 57, 52, 8, 26, 49, 45, 36, 56, 7, 48, 35, 6, 34, 33};
  return bitmap[(value^(value&(value-1)))%67];
}

uint64_t Frontend::String2Uint64(const std::string& str){
  uint64_t v = -1;
  std::cmatch mt;
  bool matched = std::regex_match(str.c_str(), mt, std::regex("\\s*(?:(0[Xx])+([0-9a-fA-F]+))|(?:(0[Bb])+([0-9]+))|(?:([0-9]+))\\s*"));
  if(!matched){
    FormatPrint(std::cerr, "ERROR<%s>: unknown format<%s>\n", __func__, str.c_str());
    throw;
  }else if(!mt[1].str().empty()){
    v = std::stoull(mt[2].str(), 0, 16);	
  }else if(!mt[3].str().empty()){
    v = std::stoull(mt[4].str(), 0, 2);	
  }else if(!mt[5].str().empty()){
    v = std::stoull(mt[5].str(), 0, 10);
  }else{
    FormatPrint(std::cerr, "ERROR<%s>: unknown format matched<%s>\n", __func__, str.c_str());
    throw;
  }
  return v;
}


Frontend::Frontend(const std::string& sensor_jsstr,
		   const std::string& firmware_jsstr,
		   const std::string& netip){
  m_jsdoc_firmware.Parse(firmware_jsstr.c_str());
  if(m_jsdoc_firmware.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(m_jsdoc_firmware.GetParseError()), m_jsdoc_firmware.GetErrorOffset());
    throw;
  }

  m_jsdoc_sensor.Parse(sensor_jsstr.c_str());
  if(m_jsdoc_sensor.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(m_jsdoc_sensor.GetParseError()), m_jsdoc_sensor.GetErrorOffset());
    throw;
  }
  m_netip = netip;
}

void  Frontend::WriteByte(uint64_t address, uint64_t value){
  DebugFormatPrint(std::cout, "WriteByte( address=%#016x ,  value=%#016x )\n", address, value);
  rbcp r(m_netip);
  std::string recvStr(100, 0);
  r.DispatchCommand("wrb",  address, value, NULL);
};

uint64_t Frontend::ReadByte(uint64_t address){
  DebugFormatPrint(std::cout, "ReadByte( address=%#016x)\n", address);
  uint8_t reg_value;
  rbcp r(m_netip);
  std::string recvStr(100, 0);
  r.DispatchCommand("rd", address, 1, &recvStr); 
  reg_value=recvStr[0];
  DebugFormatPrint(std::cout, "ReadByte( address=%#016x) return value=%#016x\n", address, reg_value);
  return reg_value;
};

void Frontend::SetFirmwareRegister(const std::string& name, uint64_t value){
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ,  value=%#016x )\n", __func__, __func__, name.c_str(), value);
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
    
    auto& json_mode = json_reg["mode"];
    if(!json_mode.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>: unknown mode format, requires a json string<%s>\n", __func__, Stringify(json_mode).c_str());
      throw;
    }
    
    uint64_t value_ori = 0;
    if(std::string(json_mode.GetString()).find('r')||std::string(json_mode.GetString()).find('R')){
      value_ori = ReadByte(OFFSET_ADDR_SENSOR_IN_FIRMWARE + address);
    }
    
    WriteByte(OFFSET_ADDR_SENSOR_IN_FIRMWARE + address, ((value<<offset) & mask) | (value_ori & ~mask) );
    
    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
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
    uint64_t valueRead  = ReadByte(OFFSET_ADDR_SENSOR_IN_FIRMWARE + address);
    
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

std::string Frontend::LoadFileToString(const std::string& p){
  std::ifstream ifs(p);
  if(!ifs.good()){
    // try relative path to binary;
    std::filesystem::path alt_path = binaryPath();
    alt_path = alt_path / p;
    
    std::string alt_path_str = alt_path.string();
    std::ifstream ifs_bin(alt_path_str);
    if(ifs_bin.good()){
      ifs = std::move(ifs_bin);
    }
    else{
      std::cerr<<"LoadFileToString:: ERROR, unable to load file<"<<p<<">\n";
      std::cerr<<"LoadFileToString:: ERROR, unable to load file<"<<alt_path_str<<">\n";
      throw;
    }
  }

  std::string str;
  str.assign((std::istreambuf_iterator<char>(ifs) ),
             (std::istreambuf_iterator<char>()));
  return str;
}
