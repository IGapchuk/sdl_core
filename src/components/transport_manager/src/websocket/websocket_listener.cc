#include "transport_manager/websocket/websocket_listener.h"
#include "transport_manager/transport_adapter/transport_adapter_controller.h"
#include "transport_manager/websocket/websocket_device.h"

namespace transport_manager {
namespace transport_adapter {
CREATE_LOGGERPTR_GLOBAL(logger_, "WebSocketListener")

WebSocketListener::WebSocketListener(TransportAdapterController* controller,
                                     const int num_threads)
    : controller_(controller)
    , ioc_(num_threads)
    , ctx_(ssl::context::sslv23)
    , acceptor_(ioc_)
    , socket_(ioc_)
    , io_pool_(num_threads)
    , shutdown_(false) {}

WebSocketListener::~WebSocketListener() {
  Terminate();
}

TransportAdapter::Error WebSocketListener::Init() {
  return StartListening();
}

void WebSocketListener::Terminate() {
  LOG4CXX_AUTO_TRACE(logger_);
  Shutdown();
}

TransportAdapter::Error WebSocketListener::StartListening() {
  LOG4CXX_AUTO_TRACE(logger_);
  if (acceptor_.is_open()) {
    return TransportAdapter::OK;
  }

  boost::system::error_code ec;

  auto const address = boost::asio::ip::make_address("127.0.0.1");
  auto const port = 2134;
  tcp::endpoint endpoint = {address, port};

  // Open the acceptor
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    auto str_err = "ErrorOpen: " + ec.message();
    LOG4CXX_ERROR(logger_, str_err << " host/port: " << address << "/" << port);
    return TransportAdapter::FAIL;
  }

  acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    std::string str_err = "ErrorSetOption: " + ec.message();
    LOG4CXX_ERROR(logger_, str_err << " host/port: " << address << "/" << port);
    return TransportAdapter::FAIL;
    ;
  }

  // Bind to the server address
  acceptor_.bind(endpoint, ec);
  if (ec) {
    std::string str_err = "ErrorBind: " + ec.message();
    LOG4CXX_ERROR(logger_, str_err << " host/port: " << address << "/" << port);
    return TransportAdapter::FAIL;
  }

  // Start listening for connections
  acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    std::string str_err = "ErrorListen: " + ec.message();
    LOG4CXX_ERROR(logger_, str_err << " host/port: " << address << "/" << port);
    return TransportAdapter::FAIL;
  }

  if (false == Run()) {
    return TransportAdapter::FAIL;
  }

  return TransportAdapter::OK;
}

bool WebSocketListener::Run() {
  LOG4CXX_AUTO_TRACE(logger_);
  if (WaitForConnection()) {
    boost::asio::post(io_pool_, [&]() { ioc_.run(); });
    return true;
  }
  LOG4CXX_ERROR(logger_, "Connection is shutdown or acceptor isn't open");
  return false;
}

bool WebSocketListener::WaitForConnection() {
  if (!shutdown_ && acceptor_.is_open()) {
    acceptor_.async_accept(
        socket_,
        std::bind(
            &WebSocketListener::StartSession, this, std::placeholders::_1));
    return true;
  }
  return false;
}

void WebSocketListener::StartSession(boost::system::error_code ec) {
  LOG4CXX_AUTO_TRACE(logger_);
  if (ec) {
    std::string str_err = "ErrorAccept: " + ec.message();
    LOG4CXX_ERROR(logger_, str_err);
    return;
  }

  if (shutdown_) {
    return;
  }

  const ApplicationHandle app_handle = socket_.native_handle();

  tcp::endpoint endpoint = socket_.remote_endpoint();
  const auto address = endpoint.address().to_string();
  const auto port = std::to_string(endpoint.port());
  const auto device_uid =
      address + ":" + port + ":" + std::to_string(app_handle);

  auto websocket_device =
      std::make_shared<WebSocketDevice>(address, port, device_uid);

  DeviceSptr device = controller_->AddDevice(websocket_device);

  LOG4CXX_INFO(logger_, "Connected client: " << device->name());

  auto connection = std::make_shared<WebSocketConnection<WebSocketSession<> > >(
      device->unique_device_id(), app_handle, std::move(socket_), controller_);

  controller_->ConnectionCreated(
      connection, device->unique_device_id(), app_handle);

  controller_->ConnectDone(device->unique_device_id(), app_handle);

  connection->Run();

  mConnectionListLock.Acquire();
  mConnectionList.push_back(connection);
  mConnectionListLock.Release();

  WaitForConnection();
}

void WebSocketListener::Shutdown() {
  LOG4CXX_AUTO_TRACE(logger_);
  if (false == shutdown_.exchange(true)) {
    socket_.close();
    boost::system::error_code ec;
    acceptor_.close(ec);
    io_pool_.stop();
    io_pool_.join();
  }
}

}  // namespace transport_adapter
}  // namespace transport_manager
