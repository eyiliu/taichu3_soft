#include "DataPack.hh"

#include <iostream>
#include <bitset>

#include "mysystem.hh"

PixelWord::PixelWord(const uint32_t v){ //BE32TOH
    // valid(1) tschip(8) xcol[9]  yrow[10] pattern[4]
    pattern =  v & 0xf;
    yrow    = (v>> 4) & 0x3ff;
    xcol    = (v>> (4+10)) & 0x1ff;
    tschip  = (v>> (4+10+9)) & 0xff;
    isvalid = (v>> (4+10+9+8)) & 0x1;
    raw = v;
}

int DataPack::MakeDataPack(const std::string& str){
    static size_t n = 0;
    if(n<100){
        n++;
        std::cout<< "==============="<<std::endl;
        std::cout<< "0------0 1------1 2------2 3------3 4------4 5------5 6------6 7------7 "<<std::endl;
        for(auto e: str){
            std::bitset<8> rawbit(e);
            std::cout<< rawbit<<" ";
        }
        std::cout<<std::endl<<"==============="<<std::endl;
    }

    int miniPackLength=8;
    if(str.size()<miniPackLength){
        std::cout<<"DataPack::MakeDataPack: str size wrong="<<str.size()<<std::endl;
        throw;
        return -1;
    }

    packraw = str;
    const uint8_t *p = reinterpret_cast<const uint8_t*>(packraw.data());
    packhead = *p;
    p++;
    daqid = *p;
    p++;
    tid = *p;
    p++;
    tid = tid<<8;
    tid += *p;
    p++;
    len = *p;
    p++;
    len = len<<8;
    len += *p;
    p++;
    uint16_t pixelwordN = len;
    for(size_t n = 0; n< pixelwordN; n++){
        uint32_t v  = BE32TOH(*reinterpret_cast<const uint32_t*>(p));
        vecpixel.emplace_back(v);
        p += 4;
    }
    packend = *p;
    p++;
    packend = packend<<8;
    packend += *p;

    return 4;
}

bool DataPack::CheckDataPack(){
    bool check_pack = (packhead==0xaa) && (packend==0xcccc);
    bool check_pixel=true;
    for(size_t n=0; n<vecpixel.size(); n++){
        if(vecpixel[n].isvalid!=1 || vecpixel[n].pattern != 0b0000){
            check_pixel=false;
        }
    }

    if(!check_pack){
        std::string head_string=std::bitset<8>(packhead).to_string();
        std::string end_string =std::bitset<16>(packend).to_string();
        std::cout<<"CheckDataPack(): wrong pack, header="<<head_string<<", end="<<end_string<<std::endl;
    }
    if(!check_pixel){
        std::cout<<"CheckDataPack(): wrong pixel info"<<std::endl;
    }
    return check_pack && check_pixel;
};
