#ifndef NCORE_STREAMED_NET_H
#define NCORE_STREAMED_NET_H

#include <memory>
#include <string>
#include <queue>

#include "AsioInclude.h"

#define FlagDef(ID) (1LL << ((ID)-1))
#define HasFlag(flags, flag) (((flags) & (flag)) != 0)
#define HasNoFlag(flags, flag) (((flags) & (flag)) == 0)
#define AddFlag(flags, flag) ((flags) |= (flag))
#define RemoveFlag(flags, flag) ((flags) &= ~(flag))

#define SNI_OFFLINE 0
#define SNI_ONLINE 1
#define SNI_RESOLVEING 2
#define SNI_CONNECTING 4
#define SNI_IN_READ 8
#define SNI_STOP_READ_R 16
#define SNI_IN_WRITE 32
#define SNI_STOP_WRITE_R 64
#define SNI_IN_ACCEPT 128
#define SNI_STOP_ACCEPT_R 256

namespace SN {
   enum class NetworkMode {
      TCP,
      UDP
   };

   enum class SNI {
      Offline = 0,
      Online = 1,
      Resolveing = 2,
      Connecting = 4,
      InRead = 8,
      StopReadR = 16,
      InWrite = 32,
      StopWriteR = 64,
      InAccept = 128,
      StopAcceptR = 256
   };

   enum class Event {
      OnStart,
      Aborted,
      Connected,
      Resolved,
      DataSent,
      DataReceived,
      Disconnected
   };
   
   enum class Error {
      AlreadyStarted,
      AlreadyResolved,
      AlreadyConnected,
      ConnectFailed,
      ResolveFailed,
      AcceptFailed,
      ConnectionClosed,
      Aborted,
      WriteFailed,
      ReadFailed,
      AbortShutdownFailed,
      AbortCloseFailed,
      AcceptorAbortCancelFailed,
      AcceptorAbortCloseFailed,
      InvalidAddress
   };

   class Resolver {
      
   };

   template<NetworkMode Mode>
   class Server;

   template<NetworkMode Mode>
   class Client;


}

#endif // ~NCORE_STREAMED_NET_H