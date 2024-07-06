#include <chrono>
#include <fstream>
#include "tftpserver.h"

using namespace std::chrono_literals;
static constexpr unsigned int NUM_OF_BLOCKS = 5;

static std::function<void(std::shared_ptr<TftpReceiver>)> dummyCallbackRec = [] (std::shared_ptr<TftpReceiver>) {};
static std::function<void(std::shared_ptr<Tftpsender>)> dummyCallbackSen = [] (std::shared_ptr<Tftpsender>) {};

int main(int argc, char* argv[])
{
}
