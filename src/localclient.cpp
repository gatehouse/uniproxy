//====================================================================
//
// Universal Proxy
//
// Core application
//--------------------------------------------------------------------
//
// This version is released as part of the European Union sponsored
// project Mona Lisa work package 4 for the Universal Proxy Application
//
// This version is released under the GNU General Public License with restrictions.
// See the doc/license.txt file.
//
// Copyright (C) 2011-2019 by GateHouse A/S
// All Rights Reserved.
// http://www.gatehouse.dk
// mailto:gh@gatehouse.dk
//====================================================================
#include "localclient.h"

#include <boost/bind.hpp>
#include "proxy_global.h"
#include <random>

using boost::asio::ip::tcp;
using boost::asio::deadline_timer;


int LocalHostSocket::id_gen = 0;

LocalHostSocket::LocalHostSocket(LocalHost &_host, boost::asio::ip::tcp::socket *_socket)
: m_host(_host)
{
   this->id = ++id_gen;
   this->m_socket = _socket;
}


void LocalHostSocket::handle_local_read(const boost::system::error_code& _error,size_t _bytes_transferred)
{
   this->m_host.handle_local_read(*this, _error, _bytes_transferred);
}


boost::asio::ip::tcp::socket &LocalHostSocket::socket()
{
   ASSERTE(this->m_socket,uniproxy::error::socket_invalid,"Fatal Invalid socket");
   return *this->m_socket;
}


LocalHost::LocalHost(bool _active, mylib::port_type _local_port, mylib::port_type _activate_port, const std::vector<RemoteEndpoint> &_proxy_endpoints,
   const int _max_connections, PluginHandler &_plugin, const boost::posix_time::time_duration &_read_timeout, bool auto_reconnect)
   : BaseClient(_active, _local_port, _activate_port, _proxy_endpoints, _max_connections, _plugin),
   m_read_timeout(_read_timeout),
   m_auto_reconnect(auto_reconnect),
   m_thread([this] { this->interrupt(); })
{
   this->m_local_connected = false; 
}


bool LocalHost::is_local_connected() const
{
   return this->local_user_count() > 0;
}


int LocalHost::local_user_count() const
{
   int result = 0;
   for (auto& sock : this->m_local_sockets)
   {
      if (sock->socket().is_open() && this->m_local_connected)
      {
         result++;
      }
   }
   return result;
}


std::vector<std::string> LocalHost::local_hostnames() const
{
   std::vector<std::string> result;
   for (auto& sock : this->m_local_sockets)
   {
      boost::asio::ip::tcp::socket &socket(sock->socket());
      boost::system::error_code ec;
      std::string sz = socket.remote_endpoint(ec).address().to_string(); // This one should not throw exceptions
      result.push_back(sz);
   }
   return result;
}


void LocalHost::start()
{
   if (this->m_thread.is_running())
   {
      DOUT(info() << "LocalHost already running on port: " << this->port());
      return;
   }
   this->m_thread.start( [this]{ this->threadproc(); } );
}


void LocalHost::stop()
{
   this->stop_activate();
   this->m_thread.stop();
}


// This will always happen in local thread context.
void LocalHost::cleanup()
{
   try
   {
      while ( this->m_local_sockets.size() > 0 )
      {
         boost::system::error_code ec;
         TRY_CATCH( this->m_local_sockets.front()->socket().close(ec) );

         std::lock_guard<std::mutex> l(this->m_mutex_base);
         this->m_local_sockets.erase( this->m_local_sockets.begin() ); // Since we use shared_ptr it should autodelete.
      }
      this->mp_acceptor = nullptr;
   }
   catch( std::exception &exc )
   {
      this->dolog(info() + exc.what());
   }
}


// The interrupt function may happen in local or other (main) thread context.
void LocalHost::interrupt()
{
   try
   {
      DOUT(info() << "Enter");
      this->mp_acceptor->cancel();
      if (int sock = get_socket(this->mp_acceptor, this->m_mutex_base); sock != 0)
      {
         DOUT(info() << "Shutdown acceptor");
         shutdown(sock, boost::asio::socket_base::shutdown_both);
      }
      std::vector<int> socks;
      {
         std::lock_guard<std::mutex> l(this->m_mutex_base);
         for (const auto& l : this->m_local_sockets)
         {
            socks.push_back(l->socket().native_handle());
         }
      }
      for (auto s : socks)
      {
         shutdown(s, boost::asio::socket_base::shutdown_both);
      }
      if (int sock = get_socket_lower(this->mp_remote_socket, this->m_mutex_base); sock != 0)
      {
         shutdown(sock, boost::asio::socket_base::shutdown_both);
      }
      DOUT(info() << "Completed");
   }
   catch( std::exception &exc )
   {
      this->dolog(info() + exc.what());
   }
}


void LocalHost::remove_socket( boost::asio::ip::tcp::socket &_socket )
{
   for ( auto iter = this->m_local_sockets.begin(); iter != this->m_local_sockets.end(); iter++ )
   {
      if ( (*iter)->socket() == _socket )
      {
         boost::system::error_code ec;
         TRY_CATCH( _socket.shutdown(boost::asio::socket_base::shutdown_both,ec) );
         TRY_CATCH( _socket.close(ec) );

         std::lock_guard<std::mutex> l(this->m_mutex_base);
         this->m_local_sockets.erase(iter);
         break;
      }
   }
}


void LocalHost::handle_local_read( LocalHostSocket &_hostsocket, const boost::system::error_code& error,size_t bytes_transferred)
{
   if (!error)
   {
      this->m_count_out.add( bytes_transferred );
      Buffer buffer( this->m_local_data, bytes_transferred );
      this->m_last_outgoing_msg = this->m_local_data;
      this->m_last_outgoing_stamp = boost::get_system_time();
      if (global.m_out_data_log_file.is_open())
      {
         this->m_local_data[bytes_transferred] = 0;
         global.m_out_data_log_file << "[" << mylib::to_string(boost::get_system_time()) << "]" << this->m_local_data;
      }
      boost::asio::async_write( this->remote_socket(), boost::asio::buffer( this->m_local_data, bytes_transferred), boost::bind(&LocalHost::handle_remote_write, this, _hostsocket.id, boost::asio::placeholders::error));
   }
   else
   {
      DERR(local_address_port(_hostsocket.socket()) << " Error: " << error << " connections: " << this->m_local_sockets.size());
      this->remove_socket(_hostsocket.socket());
      DOUT(info() << " Last outgoing msg: " << this->m_last_outgoing_stamp << ":" << this->m_last_outgoing_msg << " connections: " << this->m_local_sockets.size());
      if (this->m_local_sockets.empty())
      {
         throw std::runtime_error( "Local connection closed for " + mylib::to_string(this->m_local_port) );
      }
   }
}


void LocalHost::handle_local_write( boost::asio::ip::tcp::socket *_socket, const boost::system::error_code& error)
{
   this->m_write_count--;
   ASSERTE(this->m_pdeadline != nullptr, boost::system::errc::timed_out, "deadline timer out of scope");
   if (!error)
   {
   }
   else
   {
      DOUT(info() << local_address_port(*_socket) << " One of the attached sockets disconnected: " << remote_address_port(*_socket));
      this->remove_socket(*_socket);
   }
   if ( this->m_write_count == 0 && this->m_local_sockets.size() > 0 )
   {
      this->m_pdeadline->expires_from_now(this->m_read_timeout);

      this->remote_socket().async_read_some( boost::asio::buffer( this->m_remote_data, max_length), boost::bind(&LocalHost::handle_remote_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
   }
   if ( this->m_local_sockets.size() == 0 )
   {
      throw std::runtime_error( __FUNCTION__ );       
   }
}


void LocalHost::handle_remote_read(const boost::system::error_code& error,size_t bytes_transferred)
{
   ASSERTE(this->m_pdeadline != nullptr, boost::system::errc::timed_out, "deadline timer out of scope");
   if (!error)
   {
      this->m_count_in.add( bytes_transferred );
      this->m_remote_data[bytes_transferred] = 0;
      this->m_last_incoming_msg = this->m_remote_data;
      this->m_last_incoming_stamp = boost::get_system_time();
      if (global.m_in_data_log_file.is_open())
      {
         global.m_in_data_log_file << "[" << mylib::to_string(boost::get_system_time()) << "]" << this->m_remote_data;
      }
      this->m_write_count = this->m_local_sockets.size();
      this->m_pdeadline->expires_from_now(this->m_read_timeout);
      for ( int index = 0; index < this->m_write_count; index++ )
      {
         boost::asio::ip::tcp::socket *psocket = &this->m_local_sockets[index]->socket();
         boost::asio::async_write( *psocket, boost::asio::buffer( this->m_remote_data, bytes_transferred), boost::bind(&LocalHost::handle_local_write, this, psocket, boost::asio::placeholders::error));
      }
   }
   else
   {
      DERR(info() << "Error: " << error << ": " << error.message() << " bytes transferred: " << bytes_transferred);
      DOUT(info() << "Last incoming msg: " << this->m_last_incoming_stamp << " size:" << this->m_last_incoming_msg.size());
      throw boost::system::system_error( error );
   }
}


void LocalHost::handle_remote_write(int id, const boost::system::error_code& error)
{
   if (!error)
   {
      for (auto& sock : this->m_local_sockets)
      {
         if (sock->id == id)
         {
            sock->socket().async_read_some(boost::asio::buffer(this->m_local_data, max_length),
               boost::bind(&LocalHostSocket::handle_local_read, sock,
               boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
            break;
         }
      }
   }
   else
   {
      DOUT(info() << "Error: " << error << " in " << __FUNCTION__ << ":" <<__LINE__);
      throw boost::system::system_error( error );
   }
}


void LocalHost::handle_accept( boost::asio::ip::tcp::socket *_socket, const boost::system::error_code& error )
{
   int count = this->m_local_sockets.size();
   DOUT(info() << local_address_port(*_socket) << " error?: " << error << " Added extra local socket " << remote_address_port(*_socket) << " now " << this->m_local_sockets.size() << " connections");
   if (!error && _socket)
   {
      if (count < this->m_max_connections)
      {
         auto p = std::make_shared<LocalHostSocket>(*this, _socket);
         this->m_local_sockets.push_back(p);

         // NB!! Currently all will read to the same buffer, so it will crash eventually.
         _socket->async_read_some(boost::asio::buffer(m_local_data, max_length), boost::bind(&LocalHostSocket::handle_local_read, p.get(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
      }
      else
      {
         DOUT(info() << "Already too many connections: " << count << " vs. " << this->m_max_connections);
         boost::system::error_code ec;
         _socket->close(ec);
         delete _socket;
      }
   }
   else
   {
      DOUT(info() << local_address_port(*_socket) << " Local removed, now " << this->m_local_sockets.size() << " connections");
      delete _socket; // NB!! This is not really the right way. Explore passing the shared_ptr as a parameter instead.
   }

   // Unconditionally we start looking for the next socket.
   if ( this->mp_io_service != NULL && this->mp_acceptor != NULL && this->m_local_sockets.size() > 0)
   {
      
      boost::asio::ip::tcp::socket *new_socket = new boost::asio::ip::tcp::socket( *this->mp_io_service);
      this->mp_acceptor->async_accept( *new_socket, boost::bind(&LocalHost::handle_accept, this, new_socket, boost::asio::placeholders::error));
   }
}


// Read from the remote socket did timeout.
// Oddly this is also called whenever a succesfull read was performed.
void LocalHost::check_deadline(const boost::system::error_code& error)
{
   ASSERTE(this->m_pdeadline != nullptr, boost::system::errc::timed_out, "deadline timer out of scope");
   if (error == boost::asio::error::operation_aborted) // This will happen on every read / write that will reset the timer.
   {
      this->m_pdeadline->async_wait(boost::bind(&LocalHost::check_deadline, this, boost::asio::placeholders::error));
      return;
   }
   bool expired = deadline_timer::traits_type::now() >= this->m_pdeadline->expires_at();
   DOUT(":" << this->port() << " Timeout error code: " << error << " expired? " << expired << " now: " << deadline_timer::traits_type::now() << " expire: " << this->m_pdeadline->expires_at());
   if (expired)
   {
      DERR(":" << this->port() << " Timeout read from remote socket io: " << (this->mp_io_service != nullptr));
      boost::system::error_code ec;
      // NB!! this->remote_socket().shutdown(ec); // It has been seen hanging (more than once) in this upper layer shutdown.
      this->remote_socket().lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
      this->remote_socket().lowest_layer().close(ec);

      this->m_pdeadline->expires_at(boost::posix_time::pos_infin);
      if (!this->m_auto_reconnect)
      {
         if (this->mp_io_service != nullptr)
         {
            this->mp_io_service->stop();
         }
         this->interrupt();

         boost::system::error_code ec = make_error_code(boost::system::errc::timed_out);
         throw boost::system::system_error(ec);
      }
   }
}


void LocalHost::handle_handshake(const boost::system::error_code& error)
{
   ASSERTE(this->m_pdeadline != nullptr, boost::system::errc::timed_out, "deadline timer out of scope");
   if (!error)
   {
      this->dolog(info() + "Succesfull SSL handshake to remote host: " + this->remote_hostname() + ":" + mylib::to_string(this->remote_port()));
      {
         this->m_pdeadline->expires_from_now(this->m_read_timeout);
      }
      this->remote_socket().async_read_some(boost::asio::buffer( m_remote_data, max_length), boost::bind(&LocalHost::handle_remote_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
   }
   else
   {
      this->dolog(info() + "Failed SSL handshake to remote host: " + this->remote_hostname() + ":" + mylib::to_string(this->remote_port()) + " error: " + OSS(error));
      throw boost::system::system_error(error);
   }
}


void LocalHost::go_out(boost::asio::io_service &io_service)
{
   try
   {
      io_service.reset();
      boost::asio::deadline_timer deadline(io_service);
      mylib::protect_pointer<boost::asio::deadline_timer> p_deadline( this->m_pdeadline, deadline, this->m_mutex_base );
      boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls);
      global.set_ssl_context(ssl_context);
      ssl_socket rem_socket( io_service, ssl_context );
      mylib::protect_pointer<ssl_socket> p2( this->mp_remote_socket, rem_socket, this->m_mutex_base );

      this->dolog(info() + "Connecting to remote host: " + this->remote_hostname() + ":" + mylib::to_string(this->remote_port()) );
      boost::asio::socket_connect(rem_socket.lowest_layer(), io_service, this->remote_hostname(), this->remote_port() );

      this->dolog(info() + "Connected to remote host: " + this->remote_hostname() + ":" + mylib::to_string(this->remote_port()) + " Attempting SSL handshake" );
      DOUT(info() << "handles: " << rem_socket.next_layer().native_handle() << " / " << rem_socket.lowest_layer().native_handle() );

      boost::asio::socket_set_keepalive_to(rem_socket.lowest_layer(), std::chrono::seconds(20));
      DOUT(info() << "Prepare timeout at: " << this->m_read_timeout)
      deadline.async_wait(boost::bind(&LocalHost::check_deadline, this, boost::asio::placeholders::error));
      deadline.expires_from_now(boost::posix_time::seconds(20));
      boost::system::error_code ec;
#if 1
      rem_socket.async_handshake(boost::asio::ssl::stream_base::client, boost::bind(&LocalHost::handle_handshake, this, _1));
#else
      rem_socket.handshake( boost::asio::ssl::stream_base::client, ec);
      this->dolog("Succesfull SSL handshake to remote host: " + this->remote_hostname() + ":" + mylib::to_string(this->remote_port()) + " ec: " + OSS(ec));
      deadline.expires_from_now(this->m_read_timeout);
      rem_socket.async_read_some(boost::asio::buffer( m_remote_data, max_length), boost::bind(&LocalHost::handle_remote_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
#endif
      // Now let the io_service handle the session.
      DOUT(info() << "async_handshake started, now start ioservice");
      io_service.run(ec);
      DOUT(info() << "ioservice stopped error code: " << ec);
      deadline.cancel();
   }
   catch (std::exception &exc)
   {
      this->dolog(info() + "exception: " + exc.what() + " local count: " + OSS(this->m_local_sockets.size()));
      if (this->m_local_sockets.empty())
      {
         throw;
      }
   }
}


void LocalHost::threadproc()
{
   while (this->m_thread.check_run())
   {
      // On start and after each lost connection we end up here.
      try
      {
         DOUT(info() << __FUNCTION__ << ":" << __LINE__);
         boost::asio::io_service io_service;
         mylib::protect_pointer<boost::asio::io_service> p_io_service( this->mp_io_service, io_service, this->m_mutex_base );

         // We make this as a pointer because there may be more than one.
         boost::asio::ip::tcp::socket *local_socket = new boost::asio::ip::tcp::socket(io_service);

         // NB!! This will only support ipv4
         boost::asio::ip::tcp::acceptor acceptor( io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), this->m_local_port) );

         mylib::protect_pointer<boost::asio::ip::tcp::acceptor> p3( this->mp_acceptor, acceptor, this->m_mutex_base );
         boost::asio::io_service::work session_work(io_service);

         std::shared_ptr<void> ptr( NULL, [this](void*){ DOUT(info() << "local exit loop"); this->interrupt(); this->cleanup();} );
         this->dolog(info() + "Waiting for local connection" );

         // Synchronous wait for connection from local TCP socket. Must be handled by the interrupt function
         acceptor.accept( *local_socket );
         auto p = std::make_shared<LocalHostSocket>(*this, local_socket);
         this->m_local_sockets.push_back( p );  // NB!! redo by inserting line above directly
         this->m_local_connected = true;

         boost::asio::socket_set_keepalive_to( *local_socket, std::chrono::seconds(20) );
         local_socket->async_read_some( boost::asio::buffer( m_local_data, max_length), boost::bind(&LocalHostSocket::handle_local_read, p.get(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred) );

         // NB!! The following line may leak.... but only very slow.
         boost::asio::ip::tcp::socket *new_socket = new boost::asio::ip::tcp::socket(io_service);
         acceptor.async_accept( *new_socket, boost::bind(&LocalHost::handle_accept, this, new_socket, boost::asio::placeholders::error));

         // Unstable. Uncertain about the actual cause. Does not work with either async or sync handshake.
         // io_service.run() terminates immediately, nothing (no handler) is called and no error code is returned.
         // Happens after timeout but appears to work ok if the remote end terminates the connection.
         // NB!!while(this->m_thread.check_run() && !this->m_local_sockets.empty() && this->is_local_connected())
         do
         {
            this->m_proxy_index = 0;
            if (!this->m_proxy_endpoints.empty()) // Pick a new random access value.
            {
               std::random_device rd;  //Will be used to obtain a seed for the random number engine
               std::mt19937 gen(rd());
               std::uniform_int_distribution<> dis(0, this->m_proxy_endpoints.size()-1);
               this->m_proxy_index = dis(gen);
            }
            this->go_out(io_service);
            this->m_thread.sleep(5000);
         }
         while(this->m_auto_reconnect && this->m_thread.check_run() && !this->m_local_sockets.empty() && this->is_local_connected());
      }
      catch( std::exception &exc )
      {
         this->dolog(info() + exc.what());
      }
      this->m_local_connected = false;
      this->m_thread.sleep( 1500 ); //We will need some time to ensure the remote end has settled. May need to be investigated.
   }
}

