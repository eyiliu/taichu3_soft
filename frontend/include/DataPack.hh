#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

struct PixelWord{
    PixelWord(const uint32_t v);
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

    int MakeDataPack(const std::string& str);
    bool CheckDataPack();
};


using DataPackSP = std::shared_ptr<DataPack>;
