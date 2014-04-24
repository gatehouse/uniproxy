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
#ifndef _proxy_global_h
#define _proxy_global_h

#include "applutil.h"
#include "remoteclient.h"
#include "localclient.h"
#include "providerclient.h"
#include <cppcms/application.h>


typedef std::shared_ptr<BaseClient> baseclient_ptr;
typedef std::shared_ptr<RemoteProxyHost> remotehost_ptr;


class client_certificate_exchange : public mylib::thread
{
public:

	client_certificate_exchange( ) : mylib::thread( [](){} )
	{
	}

	void start( mylib::port_type _port )
	{
		this->mylib::thread::start( [this,_port]( ){this->thread_proc(_port);} );
	}

	void thread_proc( int _port )
	{
		try
		{
			boost::asio::io_service io_service;
			boost::asio::ip::tcp::socket local_socket(io_service);
			boost::asio::ip::tcp::endpoint endpoint( boost::asio::ip::address::from_string( "127.0.0.1" ), _port );
			DOUT( __FUNCTION__ << " connect to " << endpoint );
			local_socket.connect( endpoint );

			const int buffer_size = 100;
			char buffer[buffer_size]; //
			local_socket.read_some( boost::asio::buffer( buffer, buffer_size ) );
		}
		catch(...)
		{
		}
	}

};



class session_data
{
public:

	session_data( int _id );
	void update_timestamp();

	boost::posix_time::ptime m_timestamp;
	int m_id;
	int m_logger_read_index;
};


class proxy_global
{
public:

	proxy_global();
	void lock();
	void unlock();
	void stop( const std::string &_name );
	void stopall(); // Make a hard stop of all.

	typedef enum 
	{
		hosts 	= 0x01,
		clients	= 0x02,
		web		= 0x04,
		config	= 0x08,
		all		= hosts|clients|web|config
	} json_acl;

	// These may throw exceptions.
	void populate_json(cppcms::json::value &obj, int _json_acl);
	void unpopulate_json(cppcms::json::value &obj);

	std::string save_json_status( bool readable );
	std::string save_json_config( bool readable );

	//
	bool load_configuration();

	// lists with associated mutex
	stdt::mutex m_mutex_list;
	std::vector<remotehost_ptr> remotehosts;
	std::vector<baseclient_ptr> localclients;

	mylib::port_type m_port;
	std::string m_ip4_mask;
	bool m_debug;
	cppcms::json::value m_new_setup; // Stop and start services to match this.

	// Own name to be used for generating own certificate.
	std::string m_name;

	// The returned value here can be used for the duration of this transaction without being removed or moved.
	session_data &get_session_data( cppcms::session_interface &_session );
	void clean_session();

	// (common) Names from the certificates loaded from the imported certs.pem file.
	stdt::mutex m_mutex_certificates;
	std::vector<std::string> m_cert_names;
	bool load_certificate_names( const std::string & _filename );
	std::error_code SetupCertificates( boost::asio::ip::tcp::socket &_remote_socket, const std::string &_connection_name, bool _server, std::error_code& ec );

	bool certificate_available( const std::string &_cert_name);
	bool execute_openssl();

	bool is_provider(const BaseClient &client) const;

	bool is_same( const BaseClient &client, cppcms::json::value &obj, bool &param_changes, bool &client_changes ) const;
	bool is_same( const RemoteProxyHost &host, cppcms::json::value &obj, bool &param_changes, bool &client_changes, bool &locals_changed ) const;


protected:

	stdt::mutex m_session_data_mutex;
	std::vector< std::shared_ptr<session_data>> m_sessions;

};

extern proxy_global global;

#endif
