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

/*!
 * \brief TransactionOptionValues::getOptionsAsMap
 * \return map in TFTP format that corresponds to the internal variables
 */
std::map<std::string, std::string> TransactionOptionValues::getOptionsAsMap() const
{
    std::map<std::string, std::string> ret_val;
    ret_val["blksize"] = std::to_string(mBlocksize);

    return ret_val;
}

void TransactionOptionValues::setOptionsFromMap(const std::map<std::string, std::string>& IN_map)
{
    if(IN_map.find("blksize") != IN_map.end())
    {
        mBlocksize = atoi(IN_map.at("blksize").c_str());
    }
}

/*!
 * \brief Checks if all option values are the default values
 * \return
 */
bool TransactionOptionValues::isDefault() const
{
    bool isDefault = true;;
    isDefault = isDefault && (mBlocksize == DEFAULT_BLOCKSIZE);

    return isDefault;
}
