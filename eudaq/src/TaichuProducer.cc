#include "eudaq/Producer.hh"

#include <list>
#include <iostream>
#include <chrono>
#include <thread>
#include <regex>

#include "mysystem.hh"
#include "myrapidjson.h"

#include "Telescope.hh"

#define MAGIC_EUDET_PIVOT (9152)

template<typename ... Args>
static std::string FormatString( const std::string& format, Args ... args ){
  std::size_t size = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
  std::unique_ptr<char[]> buf( new char[ size ] );
  std::snprintf( buf.get(), size, format.c_str(), args ... );
  return std::string( buf.get(), buf.get() + size - 1 );
}


namespace taichu{
  class TaichuProducer : public eudaq::Producer {
  public:
    using eudaq::Producer::Producer;
    ~TaichuProducer() override {};
    void DoInitialise() override;
    void DoConfigure() override;
    void DoStartRun() override;
    void DoStopRun() override;
    void DoTerminate() override;
    void DoReset() override;
    void DoStatus() override;

    static const uint32_t m_id_factory = eudaq::cstr2hash("TaichuProducer");
    static const uint32_t m_id1_factory = eudaq::cstr2hash("TaichuNiProducer");

    void RunLoop() override;
  private:
    bool m_exit_of_run;
    std::unique_ptr<taichu::Telescope> m_tel;

    std::atomic<uint64_t> m_tg_n_begin;
    std::atomic<uint64_t> m_tg_n_last;

    std::chrono::system_clock::time_point m_tp_run_begin;

    uint64_t m_st_n_tg_old;
    std::chrono::system_clock::time_point m_st_tp_old;
  };
}

namespace{
  auto _reg_ = eudaq::Factory<eudaq::Producer>::
    Register<taichu::TaichuProducer, const std::string&, const std::string&>(taichu::TaichuProducer::m_id_factory);

  auto _reg1_ = eudaq::Factory<eudaq::Producer>::
    Register<taichu::TaichuProducer, const std::string&, const std::string&>(taichu::TaichuProducer::m_id1_factory);
}


void taichu::TaichuProducer::DoInitialise(){
  m_tel.reset();
  const eudaq::Configuration &param = *GetInitConfiguration();
  param.Print();

  std::vector<std::string> vecLayerName;
  std::string tel_json_str;
  
  if(param.Has("GEOMETRY_SETUP")){
    std::map<std::string, double> mapLayerPos;
    std::string str_GEOMETRY_SETUP;
    str_GEOMETRY_SETUP = param.Get("GEOMETRY_SETUP", "");
    std::regex block_regex("([a-zA-Z0-9]+)\\:([0-9]+)"); // sm[1]  name, sm[2]  pos
    auto blocks_begin = std::sregex_iterator(str_GEOMETRY_SETUP.begin(), str_GEOMETRY_SETUP.end(), block_regex);
    auto blocks_end = std::sregex_iterator();
    std::cout << "Ini file: found" << std::distance(blocks_begin, blocks_end) << " telescope layers"<<std::endl;
    for (std::sregex_iterator ism = blocks_begin; ism != blocks_end; ++ism){
      // std::smatch &sm= *ism;
      std::string sm_str = (*ism).str();
      std::string layer_name = (*ism)[1].str();
      vecLayerName.push_back(layer_name);
      double layer_pos = std::stod((*ism)[2].str());
      mapLayerPos[layer_name] = layer_pos;
    }

    rapidjson::Document jsdoc;
    rapidjson::Document::AllocatorType& a = jsdoc.GetAllocator();
    jsdoc.SetObject();
    jsdoc.AddMember("telescope", rapidjson::Value(rapidjson::kObjectType), a);
    jsdoc["telescope"].AddMember("locations", rapidjson::Value(rapidjson::kObjectType), a);
    jsdoc["telescope"].AddMember("config", rapidjson::Value(rapidjson::kObjectType), a);
    for(const auto pairLayerPos : mapLayerPos){
      rapidjson::Value name_js(pairLayerPos.first.c_str(), a);
      rapidjson::Value pos_js(pairLayerPos.second);
      jsdoc["telescope"]["locations"].AddMember(name_js, pos_js, a);
    }
    rapidjson::StringBuffer sb;
    // rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    jsdoc.Accept(writer);
    tel_json_str = sb.GetString();
  }


  std::map<std::string,  std::vector<std::pair<uint16_t, uint16_t>>> mask_col;
  for(const auto & lname : vecLayerName){
    std::string pmask_para_key("PIXEL_MASK_OVERRIDE_");
    pmask_para_key+=lname;
    if(param.Has(pmask_para_key)){
      std::vector<std::pair<uint16_t, uint16_t>> maskXYs;
      std::string str_PIXEL_MASK_OVERRIDE_x;
      str_PIXEL_MASK_OVERRIDE_x = param.Get(pmask_para_key, "");
      std::regex block_regex("([0-9]+)\\:([0-9]+)"); // X:Y
      auto blocks_begin = std::sregex_iterator(str_PIXEL_MASK_OVERRIDE_x.begin(), str_PIXEL_MASK_OVERRIDE_x.end(), block_regex);
      auto blocks_end = std::sregex_iterator();
      std::cout << "found layer <"<<lname<<"> mask size: "  << std::distance(blocks_begin, blocks_end) << " "<<std::endl;
      for (std::sregex_iterator ism = blocks_begin; ism != blocks_end; ++ism){
        const std::smatch &sm= *ism;
        uint16_t maskx = (uint16_t)std::stoul(sm[1].str());
        uint16_t masky = (uint16_t)std::stoul(sm[2].str());
        maskXYs.emplace_back(maskx, masky);
      }
      mask_col.emplace(lname, std::move(maskXYs));
    }
  }

  if(!tel_json_str.empty()){
    m_tel.reset(new taichu::Telescope("builtin", tel_json_str));
  }else{
    std::cout<<"not able to create tele_json from eudaq init file"<<std::endl;
    m_tel.reset(new taichu::Telescope("builtin", "builtin"));
  }


  if(m_tel)  m_tel->Init();

  //TODO forword mask to mtel
  
}

struct REG_CONF{
  enum        TYPE{FIRMWARE, SENSOR, DELAY};
  TYPE        regtype;
  std::string regname;
  uint64_t    value;
};

void taichu::TaichuProducer::DoConfigure(){
  const eudaq::Configuration &param = *GetConfiguration();
  param.Print();
  std::vector<REG_CONF> vecRegconf;
  if(param.Has("REG_OVERRIDE")){
    std::string str_REG_OVERRIDE;
    str_REG_OVERRIDE = param.Get("REG_OVERRIDE", "");
    std::regex block_regex("((?:fw)|(?:FW)|(?:sn)|(?:SN))\\:([a-zA-Z0-9]+)\\:(?:(?:([0-9]+))|(?:(0[Xx])+([0-9a-fA-F]+))|(?:(0[Bb])+([0-1]+)))");
    ////////////////////////^1                              ^2                     ^3           ^4      ^5                 ^6      ^7/
    auto blocks_begin = std::sregex_iterator(str_REG_OVERRIDE.begin(), str_REG_OVERRIDE.end(), block_regex);
    auto blocks_end = std::sregex_iterator();
    std::cout<< "conf file reg: found" << std::distance(blocks_begin, blocks_end) << " blocks"<<std::endl;

    for (std::sregex_iterator ism = blocks_begin; ism != blocks_end; ++ism){
      // std::smatch &sm= *ism;
      std::string sm_str = (*ism).str();
      auto& sm = *ism;
      REG_CONF regconf;
      std::string regtype_str = sm[1].str();
      if(regtype_str=="fw"||regtype_str=="FW"){
        regconf.regtype=REG_CONF::TYPE::FIRMWARE;
      }else if(regtype_str=="sn"||regtype_str=="SN"){
        regconf.regtype=REG_CONF::TYPE::SENSOR;
      }
      regconf.regname = sm[2].str();
      if(!sm[3].str().empty()){
        regconf.value = std::stoull(sm[5].str(), 0, 10);
      }else if(!sm[4].str().empty()){
        regconf.value = std::stoull(sm[5].str(), 0, 16);
      }else if(!sm[6].str().empty()){
        regconf.value = std::stoull(sm[7].str(), 0, 2);
      }
      vecRegconf.push_back(regconf);
    }
  }

  for(auto regconf: vecRegconf){
    if(regconf.regtype==REG_CONF::TYPE::FIRMWARE){
      if(m_tel) m_tel->BroadcastFirmwareRegister(regconf.regname, regconf.value);
    }
    else if(regconf.regtype==REG_CONF::TYPE::SENSOR){
      if(m_tel) m_tel->BroadcastSensorRegister(regconf.regname, regconf.value);
    }
  }
}



void taichu::TaichuProducer::DoStartRun(){
  if(m_tel) m_tel->Start_no_tel_reading();
}

void taichu::TaichuProducer::DoStopRun(){
  if(m_tel) m_tel->Stop();
  m_exit_of_run = true;
}

void taichu::TaichuProducer::DoReset(){
  m_tel.reset();
}

void taichu::TaichuProducer::DoTerminate(){
  std::terminate();
}


void taichu::TaichuProducer::RunLoop(){
  m_tp_run_begin = std::chrono::system_clock::now();
  auto tp_start_run = std::chrono::steady_clock::now();
  m_exit_of_run = false;
  bool is_first_event = true;
  while(!m_exit_of_run){
    TelEventSP telev;
    if(m_tel) telev = m_tel->ReadEvent();
    if(!telev){
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }

    uint64_t trigger_n = telev->clkN();

    auto ev_eudaq = eudaq::Event::MakeUnique("NiRawDataEvent");
    ev_eudaq->SetTriggerN(trigger_n);

    std::map<uint32_t,  std::vector<std::shared_ptr<taichu::TelMeasHit>>> map_layer_measHits;

    std::map<uint32_t,  std::vector<uint32_t>> map_layer_ni_pixels;

    for(auto& mh: telev->measHits()){
      if(!mh){
        continue;
      }
      uint32_t detN = mh->detN();
      map_layer_measHits[detN].push_back(mh);
    }

    std::vector<uint32_t>  ni_block;
    ni_block.reserve(200);

    for(uint32_t detN = 0; detN<6; detN++){
      auto& mhs = map_layer_measHits[detN];
      size_t npixel_on_detN = 0;
      for(auto& mh : mhs){
        npixel_on_detN += mh->measRaws().size();
      }
      ni_block.push_back(detN); // header
      if(ni_block.size()==1){
        ni_block.push_back(MAGIC_EUDET_PIVOT | ((trigger_n & 0xff)<<16)); // pivot, tid
      }

      ni_block.push_back(0);//fcnt
      ni_block.push_back( (npixel_on_detN &0xffff) | ((npixel_on_detN &0xffff)<<16) );//lent and lent_h

      for(auto &mh : mhs){
        for(auto &mr : mh->measRaws()){
          //col-x-u:    [11+2+16-1:2+16-1]
          //row-y-v:    [11+4-1:4-1]
          //nstate:     [4-1:0]
          //morepixel:  [2+16-1:16-1]
          // [x31, x30, x29, x28, col-x-u, morepixel[1:0]=00, x15, row-y-v, nstate[3:0]=0001]
          ni_block.push_back( ((mr.u()&0x7ff)<<(2+16)) | ((mr.v()&0x7ff)<<4) | 0x1 ); // pixel
        }
      }
      ni_block.push_back(0);//layer tail0;
      // ni_block.push_back(0);//layer tail1;
      // ni_block.push_back(0);//layer tail3;
    }
    ev_eudaq->AddBlock(0, ni_block);
    ev_eudaq->AddBlock(1, ni_block);

    SendEvent(std::move(ev_eudaq));
    if(is_first_event){
      is_first_event = false;
      m_tg_n_begin = trigger_n;
    }
    m_tg_n_last = trigger_n;
  }
}

void taichu::TaichuProducer::DoStatus() {
  if(m_exit_of_run){
    auto tp_now = std::chrono::system_clock::now();
    m_st_tp_old = tp_now;
    m_st_n_tg_old = 0;
  }

  if (!m_exit_of_run) {
    auto tp_now = std::chrono::system_clock::now();
    std::chrono::duration<double> dur_period_sec = tp_now - m_st_tp_old;
    std::chrono::duration<double> dur_accu_sec = tp_now - m_tp_run_begin;
    uint64_t st_n_tg_now = m_tg_n_last;
    uint64_t st_n_tg_begin = m_tg_n_begin;

    double sec_accu = dur_accu_sec.count();
    double sec_period = dur_period_sec.count();

    double st_hz_tg_accu = (st_n_tg_now - st_n_tg_begin) / sec_accu ;
    double st_hz_tg_period = (st_n_tg_now - m_st_n_tg_old) / sec_period ;

    SetStatusTag("TriggerID(latest:first)", FormatString("%u:%u", st_n_tg_now, st_n_tg_begin));
    SetStatusTag("TriggerHz(per:avg)", FormatString("%.1f:%.1f", st_hz_tg_period, st_hz_tg_accu));
    // SetStatusTag("EventHz(per,avg)", std::to_string());
    // SetStatusTag("Cluster([layer:avg:per])", std::to_string());

    m_st_tp_old = tp_now;
    m_st_n_tg_old = st_n_tg_now;
  }
}
