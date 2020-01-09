#include "transport_manager/websocket/websocket_listener.h"
#include "transport_manager/transport_adapter/transport_adapter_controller.h"
#include "transport_manager/websocket/websocket_device.h"

namespace transport_manager {
namespace transport_adapter {
CREATE_LOGGERPTR_GLOBAL(logger_, "WebSocketListener")

WebSocketListener::WebSocketListener(TransportAdapterController* controller,
                                     const TransportManagerSettings& settings,
                                     const int num_threads)
    : controller_(controller)
    , ioc_(num_threads)
    , ctx_(ssl::context::sslv23)
    , acceptor_(ioc_)
    , socket_(ioc_)
    , secure_acceptor_(secure_ioc_)
    , secure_socket_(secure_ioc_)
    , io_pool_(num_threads)
    , secure_io_pool_(num_threads)
    , shutdown_(false)
    , settings_(settings) {}

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

  auto const address =
      boost::asio::ip::make_address(settings_.websocket_server_address());
  tcp::endpoint endpoint = {address, settings_.websocket_server_port()};
  tcp::endpoint secure_endpoint = {address,
                                   settings_.websocket_secured_server_port()};

  auto init_acceptor = [&address](tcp::acceptor& acceptor,
                                  const tcp::endpoint& endpoint) {
    boost::system::error_code ec;
    // Open the acceptor
    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
      auto str_err = "ErrorOpen: " + ec.message();
      LOG4CXX_ERROR(logger_,
                    str_err << " host/port: " << endpoint.address().to_string()
                            << "/" << endpoint.port());
      return TransportAdapter::FAIL;
    }

    acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
      std::string str_err = "ErrorSetOption: " + ec.message();
      LOG4CXX_ERROR(logger_,
                    str_err << " host/port: " << endpoint.address().to_string()
                            << "/" << endpoint.port());
      return TransportAdapter::FAIL;
      ;
    }

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if (ec) {
      std::string str_err = "ErrorBind: " + ec.message();
      LOG4CXX_ERROR(logger_,
                    str_err << " host/port: " << endpoint.address().to_string()
                            << "/" << endpoint.port());
      return TransportAdapter::FAIL;
    }

    // Start listening for connections
    acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
      std::string str_err = "ErrorListen: " + ec.message();
      LOG4CXX_ERROR(logger_,
                    str_err << " host/port: " << endpoint.address().to_string()
                            << "/" << endpoint.port());
      return TransportAdapter::FAIL;
    }

    return TransportAdapter::OK;
  };

  if (TransportAdapter::OK != init_acceptor(acceptor_, endpoint) ||
      TransportAdapter::OK !=
          init_acceptor(secure_acceptor_, secure_endpoint)) {
    return TransportAdapter::FAIL;
  }

  if (false == Run()) {
    return TransportAdapter::FAIL;
  }

  return TransportAdapter::OK;
}

bool WebSocketListener::Run() {
  LOG4CXX_AUTO_TRACE(logger_);

  bool is_connection_open = WaitForConnection();
  if (is_connection_open) {
    boost::asio::post(io_pool_, [&]() { ioc_.run(); });
  } else {
    LOG4CXX_ERROR(logger_, "Connection is shutdown or acceptor isn't open");
  }

  bool is_secure_connection_open = WaitForSecureConnection();
  if (is_secure_connection_open) {
    boost::asio::post(secure_io_pool_, [&]() { secure_ioc_.run(); });
  } else {
    LOG4CXX_ERROR(logger_,
                  "Secure connection is shutdown or acceptor isn't open");
  }

  return is_connection_open && is_secure_connection_open;
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

bool WebSocketListener::WaitForSecureConnection() {
  LOG4CXX_AUTO_TRACE(logger_);
  if (!shutdown_ && secure_acceptor_.is_open()) {
    secure_acceptor_.async_accept(
        secure_socket_,
        std::bind(&WebSocketListener::StartSecureSession,
                  this,
                  std::placeholders::_1));
    return true;
  }
  return false;
}

void WebSocketListener::StartSecureSession(boost::system::error_code ec) {
  LOG4CXX_AUTO_TRACE(logger_);

  if (ec) {
    std::string str_err = "ErrorSecureAccept: " + ec.message();
    LOG4CXX_ERROR(logger_, str_err);
    return;
  }

  if (shutdown_) {
    return;
  }

  const ApplicationHandle app_handle = secure_socket_.native_handle();

  tcp::endpoint endpoint = secure_socket_.remote_endpoint();
  const auto address = endpoint.address().to_string();
  const auto port = std::to_string(endpoint.port());
  const auto device_uid =
      address + ":" + port + ":" + std::to_string(app_handle);

  auto websocket_device =
      std::make_shared<WebSocketDevice>(address, port, device_uid);

  DeviceSptr device = controller_->AddDevice(websocket_device);

  LOG4CXX_INFO(logger_, "Connected client: " << device->name());

  auto connection =
      std::make_shared<WebSocketConnection<WebSocketSecureSession<> > >(
          device->unique_device_id(),
          app_handle,
          std::move(secure_socket_),
          ctx_,
          controller_);

  controller_->ConnectionCreated(
      connection, device->unique_device_id(), app_handle);

  controller_->ConnectDone(device->unique_device_id(), app_handle);

  connection->Run();

  mConnectionListLock.Acquire();
  mConnectionList.push_back(connection);
  mConnectionListLock.Release();

  WaitForSecureConnection();
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
    auto shutdown = [](tcp::socket& socket,
                       tcp::acceptor& acceptor,
                       boost::asio::thread_pool& thread_pool) {
      socket.close();
      boost::system::error_code ec;
      acceptor.close(ec);

      if (ec) {
        LOG4CXX_ERROR(logger_, "Acceptor closed with error: " << ec);
      }

      thread_pool.stop();
      thread_pool.join();
    };
    shutdown(socket_, acceptor_, io_pool_);
    shutdown(secure_socket_, secure_acceptor_, secure_io_pool_);
  }
}

}  // namespace transport_adapter
}  // namespace transport_manager
