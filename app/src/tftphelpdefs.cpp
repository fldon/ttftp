#include "tftphelpdefs.h"

TftpMode str2mode(std::string mode)
{
    if(mode == "octet")
        return TftpMode::OCTET;    //TODO: Check how the mode is actually supplied over the network. Does it even make sense to make this a string??? It is probably just a byte
    return TftpMode::INVALID;
}
std::string mode2str(TftpMode mode)
{
    switch(mode)
    {
    case TftpMode::OCTET:
    {
        return "octet";
    }
    default:
        return "invalid";
    }
}
