#ifndef NCORE_ASIOINCLUDE_H
#define NCORE_ASIOINCLUDE_H

#ifdef _WIN32
   #undef WINAPI_FAMILY
   #define WIN32_WINNT 0x0A00
#endif

#ifdef NetStream_Enable_Log
   #define NCore_Log(txt) std::cout << txt;
#else
   #define NCore_Log(txt)
#endif

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <iostream>

using asio::ip::tcp;

#endif //NCORE_ASIOINCLUDE_H
