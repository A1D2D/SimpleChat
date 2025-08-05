#ifndef PORTBRIDGE_ASIOINCLUDE_H
#define PORTBRIDGE_ASIOINCLUDE_H

#ifdef _WIN32
   #undef WINAPI_FAMILY
   #define WIN32_WINNT 0x0A00
#endif

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

using asio::ip::tcp;

#endif //PORTBRIDGE_ASIOINCLUDE_H
