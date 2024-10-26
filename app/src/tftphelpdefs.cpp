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

bool TransactionOptionValues::setOptionsFromMap(const std::map<std::string, std::string>& IN_map)
{
    if(IN_map.find("blksize") != IN_map.end())
    {
        int blksize = atoi(IN_map.at("blksize").c_str());
        if(blksize >= 8 && blksize <= 65464)
            mBlocksize = blksize;
        else
        {
            //TOOD: What to do with invalid values? Throw exception? Restore default values? Return a bool?
            return false;
        }
    }


    return true;
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


std::string error_message_from_code(TftpUserFacingErrorCode errorcode)
{
    switch(errorcode)
    {
    case TftpUserFacingErrorCode::ERR_NOERR:
    {
        return "No error";
    }
    case TftpUserFacingErrorCode::ERR_INPUT_FILE_OPEN:
    {
        return "Could not open file for input";
    }
    case TftpUserFacingErrorCode::ERR_OUTPUT_FILE_OPEN:
    {
        return "Could not open file for output";
    }
    case TftpUserFacingErrorCode::ERR_OACK_TIMEOUT:
    {
        return "Timed out waiting for OACK";
    }
    case TftpUserFacingErrorCode::ERR_OPCODE:
    {
        return "Received message with wrong or unexpected opcode";
    }
    case TftpUserFacingErrorCode::ERR_OPT_NEGOTIATION:
    {
        return "Error during option negotiation";
    }
    case TftpUserFacingErrorCode::ERR_REQUEST:
    {
        return "Invalid request";
    }
    case TftpUserFacingErrorCode::ERR_WRONG_BLOCK:
    {
        return "Received wrong block during transfer";
    }
    }
    return "UNHANDLED ERROR";
}
