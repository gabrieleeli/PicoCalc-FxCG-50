#include "ai_client.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include <new>
namespace fx {
bool aiReady(){return false;}
std::string aiChat(AIProvider,const std::string& prompt){
 if(prompt.size()>aiRequestCapacity())return "Richiesta troppo lunga per la memoria disponibile.";
 return "AI non configurata: collega il PicoCalc a un relay HTTPS sicuro. Non inserire API key nel firmware.";
}
AiBuffer::AiBuffer(std::size_t capacity):text_(mem::resource(mem::Policy::PREFER_PSRAM)),capacity_(capacity){}
bool AiBuffer::append(const char* data,std::size_t length) noexcept{
 if(!data)return false;
 if(text_.size()+length>capacity_){truncated_=true;return false;}
 try{text_.append(data,length);}catch(const std::bad_alloc&){truncated_=true;return false;}
 return true;
}
std::size_t aiRequestCapacity(){return limits::aiRequestCapacity();}
std::size_t aiResponseCapacity(){return limits::aiResponseCapacity();}
}
