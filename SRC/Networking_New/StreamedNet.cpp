#include "StreamedNet.h"

/*---------------------------NET_STREAM---------------------------*/
SNImpl::NetStream::NetStream(std::shared_ptr<asio::io_context> context, tcp::socket& socket) :
   context_(context), socket_(std::move(socket)), readBuffer(20 * 1024), writeBuffer(20 * 1024) {
   doTick();
}

SNImpl::NetStream::NetStream(std::shared_ptr<asio::io_context> context) :
   context_(context), socket_(*context_), readBuffer(20 * 1024), writeBuffer(20 * 1024) {
   doTick();
}

void SNImpl::NetStream::startRead() {
   if(HasFlag(state, SNI_IN_READ)) return;

   std::scoped_lock lock(mutex);
   asio::post(*context_, [this]() {
      doRead();
   });
}

void SNImpl::NetStream::startWrite() {
   if(HasFlag(state, SNI_IN_WRITE)) return;

   std::scoped_lock lock(mutex);
   asio::post(*context_, [this]() {
      doWrite();
   });
}

void SNImpl::NetStream::stopRead() {
   if(HasNoFlag(state, SNI_IN_READ)) return;
   AddFlag(state, SNI_STOP_READ_R);
}

void SNImpl::NetStream::stopWrite() {
   if(HasNoFlag(state, SNI_IN_WRITE)) return;
   AddFlag(state, SNI_STOP_WRITE_R);
}

void SNImpl::NetStream::disconnect() {
}

void SNImpl::NetStream::doRead() {
   std::scoped_lock lock(mutex);
   auto readLambda = [this](std::error_code ec, std::size_t length) {
      std::scoped_lock lock(this->mutex);
      if(ec) {
         RemoveFlag(state, SNI_IN_READ);
         return;
      }

      for (int i = 0; i < length; ++i) readQ.push(readBuffer[i]);
      onRead();
      if(HasFlag(state, SNI_STOP_READ_R)) {
         RemoveFlag(state, SNI_IN_READ);
      } else doRead();
   };

   socket_.async_read_some(asio::buffer(readBuffer.data(), readBuffer.size()), readLambda);
}

void SNImpl::NetStream::doWrite() {
   std::scoped_lock lock(mutex);

   auto writeLambda = [this](std::error_code ec, std::size_t length) {
      std::scoped_lock lock(this->mutex);
      if(ec) {
         RemoveFlag(state, SNI_IN_WRITE);
         return;
      }

      onWrite();
      if(HasFlag(state, SNI_STOP_WRITE_R)) {
         RemoveFlag(state, SNI_IN_WRITE);
      } else doWrite();
   };

   socket_.async_write_some(asio::buffer(writeBuffer.data(), writeBuffer.size()), writeLambda);
}

void SNImpl::NetStream::doTick() {
   asio::post(*context_, [this]() {
      doTick();
      onTick();
   });
}

SNImpl::NetStream::~NetStream() {
}


/*---------------------------CLIENT---------------------------*/
SNImpl::Client::Client(std::shared_ptr<asio::io_context> context) : NetStream(context), resolver(*context_) {

}


void SNImpl::Client::resolve(const std::string& host, ushort_16 port) {
   if(HasFlag(state, SNI_ONLINE) || HasFlag(state, SNI_RESOLVEING) || HasFlag(state, SNI_CONNECTING)) return;

   auto resolveLambda = [this](const std::error_code& ec, tcp::resolver::results_type resultEndpoints){
      endpoints.clear();
      for (auto it = resultEndpoints.begin(); it != resultEndpoints.end(); ++it)
         endpoints.push_back(it->endpoint());
      RemoveFlag(state, SNI_RESOLVEING);

      onResolve();
   };

   AddFlag(state, SNI_RESOLVEING);
   resolver.async_resolve(host, std::to_string(port), resolveLambda);
}

void SNImpl::Client::addEndpoint(const std::string& host, ushort_16 port) {
   endpoints.push_back(tcp::endpoint(asio::ip::make_address(host), port));
}


void SNImpl::Client::connect() {
   if(HasFlag(state, SNI_ONLINE) || HasFlag(state, SNI_CONNECTING)) return;

   auto connectLambda = [this](const std::error_code& ec, const tcp::endpoint& connectedEndpoint){
      if(ec) {

      }
      RemoveFlag(state, SNI_CONNECTING);
      AddFlag(state, SNI_ONLINE);

      onConnect();
   };
   AddFlag(state, SNI_CONNECTING);

   asio::async_connect(socket_, endpoints, connectLambda);
}