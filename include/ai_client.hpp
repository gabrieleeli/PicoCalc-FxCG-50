#pragma once
#include <string>
namespace fx { enum class AIProvider { ChatGPT, Claude }; bool aiReady(); std::string aiChat(AIProvider provider,const std::string& prompt); }
