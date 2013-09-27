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
// Copyright (C) 2011-2013 by GateHouse A/S
// All Rights Reserved.
// http://www.gatehouse.dk
// mailto:gh@gatehouse.dk
//====================================================================
#ifndef _providerclient_h
#define _providerclient_h

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "applutil.h"

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

class ProviderClient;

/*
class ProviderClientSocket
{
public:

	ProviderClientSocket(ProviderClient &_host, boost::asio::ip::tcp::socket *_socket);

	void handle_local_read(const boost::system::error_code& error,size_t bytes_transferred);

	boost::asio::ip::tcp::socket &socket();

	friend bool operator==( const ProviderClientSocket&, const ProviderClientSocket&);

protected:

	boost::asio::ip::tcp::socket *m_socket;

	ProviderClient &m_host;
};
*/

class ProviderClient
{
public:

	ProviderClient( bool _active, 
		const std::vector<ProxyEndpoint> &_local_endpoints, const std::vector<ProxyEndpoint> &_proxy_endpoints, 
		//const int _max_connections, 
		PluginHandler &_plugin );

	void start();
	void stop();

	void threadproc();
	void interrupt();

	ssl_socket &remote_socket();
/*
	void handle_local_read( ProviderClientSocket &_hostsocket, const boost::system::error_code& error,size_t bytes_transferred);
	void handle_remote_read(const boost::system::error_code& error,size_t bytes_transferred);

	void handle_local_write(boost::asio::ip::tcp::socket *_socket, const boost::system::error_code& error);
	void handle_remote_write(const boost::system::error_code& error);
*/
//	void remove_socket( boost::asio::ip::tcp::socket &_socket );

	std::string remote_hostname();
	int remote_port();

	bool m_active;

	std::vector<ProxyEndpoint> m_proxy_endpoints; // The list of remote proxies to connect to in a round robin fashion.
	std::vector<ProxyEndpoint> m_local_ep; // The list of local providers to connect to in a round robin fashion.
	int m_proxy_index;
	//int m_local_port;
	//int m_max_connections;

	bool is_local_connected();
	bool is_remote_connected(int index);
	//void handle_accept( boost::asio::ip::tcp::socket *_socket, const boost::system::error_code& error );
	void cleanup();

	// Should these move to ProxyEndpoint ?
	data_flow m_count_in, m_count_out;

	int m_id;

	void dolog( const std::string &_line );
	const std::string dolog();


	std::string get_password() const;

	boost::posix_time::ptime m_activate_stamp;

	bool remote_hostname( int index, std::string &result );
	std::vector<std::string> local_hostnames();


protected:

	// It is difficult to handle the mutexes, so keep this one protected and use local getter function to retrieve information.
	stdt::mutex m_mutex;

	//std::vector<std::shared_ptr<ProviderClientSocket> > m_local_sockets; // Elements are inserted here, once they have been accepted.

	// These are used for RAII handling. They do not own anything and should not be assigned by new.
	//boost::asio::io_service *mp_io_service;
	//boost::asio::io_service m_io_service;

	//boost::asio::ip::tcp::acceptor *mp_acceptor;
	ssl_socket *mp_remote_socket;
	//boost::asio::ip::tcp::socket *mp_local_socket;

	mylib::thread m_thread;
	PluginHandler &m_plugin;
	int m_write_count;

	bool m_local_connected, m_remote_connected;

	enum { max_length = 1024 };
	char m_local_data[max_length];
	char m_remote_data[max_length];

	std::string m_log;
};


#endif