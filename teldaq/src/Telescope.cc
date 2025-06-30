#include <regex>
#include <iostream>
#include <thread>
#include "Telescope.hh"
#include "Frontend.hh"

using namespace taichu;

static const std::string builtin_tele_conf_str =
  // "";
#include "taichu_tel_conf.json.hh"
  ;


static const std::string builtin_layer_conf_str =
  // "";
#include "taichu_layer_conf.json.hh"
  ;



Telescope::Telescope(const std::string& tele_js_str, const std::string& layer_js_str){

  m_jsd_tele.Parse((tele_js_str=="builtin"||tele_js_str.empty())?builtin_tele_conf_str:tele_js_str);
  if(m_jsd_tele.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string position %lu) \n", rapidjson::GetParseError_En(m_jsd_tele.GetParseError()), m_jsd_tele.GetErrorOffset());
    throw;
  }
  if(!m_jsd_tele.HasMember("telescope")){
    fprintf(stderr, "JSON configure file error: no \"telescope\" section \n");
    throw;
  }
  const auto& js_telescope  = m_jsd_tele["telescope"];

  m_jsd_layer.Parse((layer_js_str=="builtin" || layer_js_str.empty())?builtin_layer_conf_str:layer_js_str);
  if(m_jsd_layer.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string position %lu) \n", rapidjson::GetParseError_En(m_jsd_layer.GetParseError()), m_jsd_layer.GetErrorOffset());
    throw;
  }
  if(!m_jsd_layer.HasMember("layers")){
    fprintf(stderr, "JSON configure file error: no \"layers\" section \n");
    std::cout<< layer_js_str<<std::endl;

    
    throw;
  }
  const auto& js_layers     = m_jsd_layer["layers"];

  std::map<std::string, double> layer_loc;
  std::multimap<double, std::string> loc_layer;

  if(!js_telescope.HasMember("locations")){
    fprintf(stderr, "JSON configure file error: no \"location\" section \n");
    throw;
  }

  // throw;
  for(const auto& l: js_telescope["locations"].GetObject()){
    std::string name = l.name.GetString();
    double loc = l.value.GetDouble();
    layer_loc[name] = loc;
    loc_layer.insert(std::pair<double, std::string>(loc, name));
  }

  uint16_t layer_n = 0;
  for(const auto& l: loc_layer){
    std::string layer_name = l.second;
    bool layer_found = false;
    for (const auto& js_layer : js_layers.GetArray()){
      if(js_layer.HasMember("name") && js_layer["name"]==layer_name){
        std::string ly_name=layer_name;
        std::string ly_host=js_layer["data_link"]["options"]["ip"].GetString();
        // short int ly_port=js_layer["data_link"]["options"]["port"].GetUint();
        //std::unique_ptr<Layer> l(new Layer(ly_name, ly_host, ly_port));
        std::unique_ptr<Frontend> l(new Frontend(ly_host));
	l->m_extension = layer_n;
	layer_n ++;
        m_vec_layer.push_back(std::move(l));
        layer_found = true;
        break;
      }
    }
    if(!layer_found){
      std::fprintf(stderr, "Layer %6s: is not found in configure file \n", layer_name.c_str());
      throw;
    }
    std::fprintf(stdout, "Layer %6s:     at location Z = %8.2f\n", layer_name.c_str(), l.first);
  }

}

Telescope::~Telescope(){
  Stop();
}

TelEventSP Telescope::ReadEvent(){
  if (!m_is_running) return nullptr;

  uint32_t trigger_n = -1;
  for(auto &l: m_vec_layer){
    if( l->Size() == 0){
      // TODO check cached size of all layers
      return nullptr;
    }
    else{
      uint32_t trigger_n_ev = l->Front()->tid;
      if(trigger_n_ev< trigger_n)
        trigger_n = trigger_n_ev;
    }
  }

  // std::vector<TelEventSP> sub_events;
  std::vector<DataPackSP> sub_datapacks;
  for(auto &l: m_vec_layer){
    auto &ev_front = l->Front();
    if(ev_front->tid == trigger_n){
      sub_datapacks.push_back(ev_front);
      l->PopFront();
    }
  }

  if(sub_datapacks.size() < m_vec_layer.size() ){
    std::cout<< "dropped assambed event with subevent less than requried "<< m_vec_layer.size() <<" sub events" <<std::endl;
    std::string dev_numbers;
    for(auto & ev : sub_datapacks){
      dev_numbers += std::to_string(ev->daqid);
      dev_numbers +=" ";
    }
    std::cout<< "  TID#"<<trigger_n<<" subevent= "<< dev_numbers <<std::endl;
    return nullptr;
  }


  std::vector<TelEventSP> sub_events;
  for(const auto &dp: sub_datapacks){
    uint16_t tid = dp->tid;
    uint16_t daqid = dp->daqid;
    TelEventSP subev = std::make_shared<taichu::TelEvent>(0, m_st_n_ev, daqid, tid);
    const auto &vecPixel = dp->vecpixel;
    for(const auto & pix: vecPixel){
      subev->MRs.emplace_back(pix.xcol, pix.yrow, daqid, tid);
    }
    subev->MHs = TelMeasHit::clustering_UVDCus(subev->MRs);
    sub_events.push_back(subev);
  }

  uint32_t runN = 0;
  uint32_t eventN =  m_st_n_ev;
  uint32_t deviceN = 0;
  uint32_t clockN = trigger_n;
  auto telev_sync = std::make_shared<taichu::TelEvent>(runN, eventN, deviceN, clockN);
  for(auto &subev: sub_events){
    telev_sync->MRs.insert(telev_sync->MRs.end(), subev->MRs.begin(),subev->MRs.end());
    telev_sync->MHs.insert(telev_sync->MHs.end(), subev->MHs.begin(),subev->MHs.end());
  }

  if(m_mon_ev_read == m_mon_ev_write){
    m_ev_last=telev_sync;
    m_mon_ev_write ++;
  }
  m_st_n_ev ++;
  return telev_sync;

}

TelEventSP Telescope::ReadEvent_Lastcopy(){
  if(m_mon_ev_write > m_mon_ev_read){
    auto re_ev_last = m_ev_last;
    m_mon_ev_read ++;
    return re_ev_last;
  }
  else
    return nullptr;
}

void Telescope::Init(){
  for(auto & l: m_vec_layer){
    l->daq_conf_default();
  }
}

void Telescope::Start(){
  m_st_n_ev = 0;
  m_mon_ev_read = 0;
  m_mon_ev_write = 0;

  for(auto & l: m_vec_layer){
    l->daq_start_run();
  }
  std::fprintf(stdout, "tel_start \n");

  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Telescope::AsyncWatchDog, this);
  }

  m_fut_async_rd = std::async(std::launch::async, &Telescope::AsyncRead, this);
  m_is_running = true;
}

void Telescope::Start_no_tel_reading(){ // TO be removed,
  m_st_n_ev = 0;
  m_mon_ev_read = 0;
  m_mon_ev_write = 0;

  for(auto & l: m_vec_layer){
    l->daq_start_run();
  }

  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Telescope::AsyncWatchDog, this);
  }
  //m_fut_async_rd = std::async(std::launch::async, &Telescope::AsyncRead, this);
  m_is_running = true;
}

void Telescope::Stop(){
  m_is_async_reading = false;
  if(m_fut_async_rd.valid())
    m_fut_async_rd.get();

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();

  for(auto & l: m_vec_layer){
    l->daq_stop_run();
  }

  m_is_running = false;
}

uint64_t Telescope::AsyncRead(){
  auto now = std::chrono::system_clock::now();
  auto now_c = std::chrono::system_clock::to_time_t(now);
  std::string now_str = TimeNowString("%y%m%d%H%M%S");
  std::string data_path = "data/alpide_"+now_str+".json";
  uint64_t n_ev = 0;
  m_is_async_reading = true;
  while (m_is_async_reading){
    auto telev = ReadEvent();
    if(!telev){
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }
    n_ev ++;
  }
  return n_ev;
}

uint64_t Telescope::AsyncWatchDog(){
  m_is_async_watching = true;
  while(m_is_async_watching){
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for(auto &l: m_vec_layer){
      std::string l_status = l->GetStatusString();
      std::fprintf(stdout, "%s\n", l_status.c_str());
    }
    uint64_t st_n_ev = m_st_n_ev;
    std::fprintf(stdout, "Tele: disk saved events(%lu) \n\n", st_n_ev);
  }
  //sleep and watch running time status;
  return 0;
}


void Telescope::BroadcastFirmwareRegister(const std::string& name, uint64_t value){
  for(auto &fe :  m_vec_layer){
    if(fe){
      fe->SetFirmwareRegister(name, value);
    }
  }
}

void Telescope::BroadcastSensorRegister(const std::string& name, uint64_t value){
  for(auto &fe :  m_vec_layer){
    if(fe){
      fe->SetSensorRegister(name, value);
    }
  }
}
