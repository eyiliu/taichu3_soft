#pragma once

#include<cstddef>
#include<string>

class StreamInBuffer{
public:
    void append(size_t length, const char *data);

    bool havepacket() const;

    bool havepacket_possible() const;
    void  resyncpacket();

    std::string getpacket();

    void dump(size_t maxN);

    void resyncpackethead();
private:
    void updatelength(bool force);
    size_t m_len{0};
    std::string m_buf;
};


