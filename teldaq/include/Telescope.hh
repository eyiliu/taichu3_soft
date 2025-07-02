#pragma once

#include <mutex>
#include <future>
#include <cstdio>
#include <set>

#include "myrapidjson.h"

#include "TelEvent.hpp"

class Frontend;

namespace taichu{
  using TelEventSP = std::shared_ptr<TelEvent>;

  class Telescope{
  public:
    std::vector<std::unique_ptr<Frontend>> m_vec_layer;
    std::future<uint64_t> m_fut_async_rd;
    std::future<uint64_t> m_fut_async_watch;
    bool m_is_async_reading{false};
    bool m_is_async_watching{false};
    bool m_is_running{false};

    TelEventSP m_ev_last;
    std::atomic<uint64_t> m_mon_ev_read{0};
    std::atomic<uint64_t> m_mon_ev_write{0};
    TelEventSP ReadEvent_Lastcopy();

    std::atomic<uint64_t> m_st_n_ev{0};

    ~Telescope();
    Telescope(const std::string& tele_js_str, const std::string& layer_js_str);
    TelEventSP ReadEvent();

    void BroadcastFirmwareRegister(const std::string& name, uint64_t value);
    void BroadcastSensorRegister(const std::string& name, uint64_t value);
    void FlushPixelMask(const std::map<std::string,  std::set<std::pair<uint16_t, uint16_t>>>& mask_col);

    void Init();
    void Start();
    void Stop();
    void Start_no_tel_reading();
    uint64_t AsyncRead();
    uint64_t AsyncWatchDog();

    rapidjson::Document m_jsd_tele;
    rapidjson::Document m_jsd_layer;

  };
}
