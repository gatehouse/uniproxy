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
#include "baseclient.h"
#include <boost/bind.hpp>
#include "proxy_global.h"

static int static_local_id = 0;

BaseClient::BaseClient(bool _active, int _local_port, const std::vector<ProxyEndpoint> &_proxy_endpoints, const int _max_connections, PluginHandler &_plugin)
:	m_proxy_index(0),
	m_active(_active),
	mp_remote_socket( nullptr ),
	m_count_in(true), m_count_out(true),
	m_local_port(_local_port),
	m_max_connections(_max_connections),
	m_thread_activate([]{}),
	m_thread( [this]{ this->interrupt(); } ),
	m_plugin(_plugin)
{
	this->m_proxy_endpoints = _proxy_endpoints;
	this->m_activate_stamp = boost::get_system_time();
	this->m_id = ++static_local_id;
}


ssl_socket &BaseClient::remote_socket()
{
	ASSERTE(this->mp_remote_socket, uniproxy::error::socket_invalid, "");
	return *this->mp_remote_socket;
}


void BaseClient::dolog( const std::string &_line )
{
	this->m_log = _line;
	log().add( " hostname: " + this->remote_hostname() + " port: " + mylib::to_string(this->remote_port()) + ": " + _line );
}


const std::string BaseClient::dolog()
{
	return this->m_log;
}


int BaseClient::remote_port() const
{
	return this->m_proxy_endpoints[this->m_proxy_index].m_port;
}


std::string BaseClient::remote_hostname() const
{
	return this->m_proxy_endpoints[this->m_proxy_index].m_hostname;
}


bool BaseClient::is_remote_connected(int index) const
{
	stdt::lock_guard<stdt::mutex> l(this->m_mutex);
	return this->mp_remote_socket != nullptr && this->mp_remote_socket->lowest_layer().is_open() && //this->m_remote_connected && 
		is_connected(this->mp_remote_socket->lowest_layer())
		&& (this->m_proxy_index == index || index == -1);
}


std::string BaseClient::get_password() const
{
	return "1234";
}


void BaseClient::ssl_prepare(boost::asio::ssl::context &ssl_context) const
{
	ssl_context.set_password_callback(boost::bind(&BaseClient::get_password,this));
	ssl_context.set_verify_mode(boost::asio::ssl::context::verify_peer|boost::asio::ssl::context::verify_fail_if_no_peer_cert);
	ssl_context.load_verify_file(my_certs_name);
	ssl_context.use_certificate_chain_file(my_public_cert_name);
	ssl_context.use_private_key_file(my_private_key_name, boost::asio::ssl::context::pem);
}


void BaseClient::threadproc_activate(int index)
{
	try
	{
		if ( boost::get_system_time() < this->m_activate_stamp )
		{
			DOUT("Activate: " << index << " starting");
			boost::asio::io_service io_service;
			boost::asio::ssl::context ssl_context( io_service, boost::asio::ssl::context::sslv23);
			this->ssl_prepare(ssl_context);
			boost::asio::io_service::work session_work(io_service);
			ssl_socket remote_socket(io_service, ssl_context);
			
			std::string epz = this->m_proxy_endpoints[index].m_hostname + ":" + mylib::to_string(this->m_proxy_endpoints[index].m_port);
			this->dolog("Activate Performing remote connection to: " + epz );
			boost::asio::sockect_connect( remote_socket.lowest_layer(), io_service, this->m_proxy_endpoints[index].m_hostname, this->m_proxy_endpoints[index].m_port );
			this->m_proxy_index = index;

			ProxyEndpoint &ep = this->m_proxy_endpoints[index];
			std::error_code err;
			this->dolog("Activate Attempting to perform certificate exchange with " + ep.m_name );
			this->m_activate_stamp = boost::get_system_time(); // Reset to current time to avoid double attempts.
			if (	!global.SetupCertificates( remote_socket.next_layer(), ep.m_name, false, err ) &&
					!global.SetupCertificates( remote_socket.next_layer(), ep.m_name, true, err ) )
			{
				this->dolog("Succeeded in exchanging certificates" );
			}
			else
			{
				this->dolog("Failed to exchange certificate: " + err.message() );
			}
		}			
	}
	catch(std::exception &exc)
	{
		DERR("Threadproc exception: " << exc.what());
	}
}


void BaseClient::stop_activate()
{
	this->m_thread_activate.stop();
}


void BaseClient::start_activate(int _index)
{
	this->m_thread_activate.start([this,_index](){this->threadproc_activate(_index);});	
}


std::string BaseClient::local_portname() const
{
	return mylib::to_string(this->m_local_port);
}


