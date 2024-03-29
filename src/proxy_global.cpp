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
// Copyright (C) 2011-2022 by GateHouse A/S
// All Rights Reserved.
// http://www.gatehouse.dk
// mailto:gh@gatehouse.dk
//====================================================================

#include "proxy_global.h"
#include "cppcms_util.h"
#include <cppcms/view.h>
#include "httpclient.h"
#include <boost/filesystem.hpp>

// From release.cpp
extern const char * version;

PluginHandler standard_plugin("");
proxy_global global;


proxy_global::proxy_global()
{
   this->m_ip4_mask = "0.0.0.0";
   this->m_debug = false;
}


//-----------------------------------


void proxy_global::lock()
{
   DOUT("Now starting all connections");
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   for (auto& p : this->remotehosts)
   {
      DOUT("Starting remotehost at: " << p->port());
      p->start();
   }
   for (auto& p : this->localclients)
   {
      if (p->is_active())
      {
         DOUT("Starting localhost at: " << p->local_portname());
         p->start();
      }
   }
   this->m_activate_host.start(this->m_activate_port); // NB!! Hardcoded here...
}


void proxy_global::unlock()
{
   DOUT(__FUNCTION__ << ":" << __LINE__);
   this->stopall();
   DOUT(__FUNCTION__ << ":" << __LINE__);
}


void proxy_global::stopall()
{
   this->m_activate_host.stop(false);
   {
      std::lock_guard<std::mutex> l(this->m_mutex_list);
      DOUT("sockethandler.StopAll()");
      for (auto& p : this->remotehosts)
      {
         DOUT("remotehost.Stop() b");
         RemoteProxyHost* p2 = p.get();
         p2->stop();
      }
      for (auto& lc : this->localclients)
      {
         DOUT("localhost.Stop()");
         lc->stop();
      }
      this->remotehosts.clear();
      this->localclients.clear();
   }
   this->m_activate_host.stop(true);
}


bool proxy_global::host_activate(const std::string& param)
{
   DOUT(__FUNCTION__ <<  ": " << param);
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   for (auto& host : this->remotehosts)
   {
      for(auto& ep : host->m_remote_ep)
      {
         if (ep.m_name == param)
         {
            this->m_activate_host.add(param);
            return true;
         }
      }
   }
   return false;
}


bool proxy_global::client_activate(const std::string& param, const std::string& sid)
{
   bool result = false;
   DOUT(__FUNCTION__ << ": " << param << " ID: " << sid);
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   for (auto& client : this->localclients)
   {
      int id;
      if (client->id() == mylib::from_string(sid, id))
      {
         if (client->activate(param))
         {
            return true;
         }
      }
   }
   return result;
}


void proxy_global::client_set_active(const std::string& param, int id, bool active)
{
   DOUT(__FUNCTION__ << ": " << param << " ID: " << id << " Checked: " << active);
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   for (auto& client : this->localclients)
   {
      if (client->set_active(param, id, active))
      {
         return;
      }
   }
}


int proxy_global::host_test(const std::string& param)
{
   DOUT(__FUNCTION__ <<  ": " << param);
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   for (auto& host : this->remotehosts)
   {
      for (auto& ep : host->m_remote_ep)
      {
         if (ep.m_name == param)
         {
            this->m_mutex_list.unlock();
            DOUT("Host test ep: " << ep.save());
            int code = host->test_local_connection(param);
            DOUT("Host test completed result: " << code);
            return code;
         }
      }
   }
   return 422;
}


void proxy_global::host_set_active(const std::string& param, int id, bool active)
{
   DOUT(__FUNCTION__ << ": " << param << " Checked: " << active);
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   for (auto& host : this->remotehosts)
   {
      if (host->id() == id)
      {
         if (active)
         {
            host->m_active = true;
            host->start();
         }
         else
         {
            host->m_active = false;
            host->stop(); // NB!! Is this a problem since it may be blocking?
         }
         DOUT("Updated active state for host: " << id << " new value: " << active);
      }
   }
}


bool proxy_global::client_certificate_exists(const std::string& certname) const
{
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   for (auto& client : this->localclients) // Look through all the client connections. If one is found then we reload.
   {
      if (client->certificate_exists(certname))
      {
         return true;
      }
   }
   return false;
}


bool proxy_global::host_certificate_exists(const std::string& certname) const
{
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   for (auto& host : this->remotehosts)
   {
      for (const auto& remote : host->m_remote_ep)
      {
         if (remote.m_name == certname)
         {
            return true;
         }
      }
   }
   return false;
}


bool is_provider(const BaseClient &client)
{
   return dynamic_cast<const ProviderClient*>(&client) != nullptr;
}


bool has_array(const cppcms::json::value &obj, const std::string &name)
{
   cppcms::json::value res = obj.find( name );
   return res.type() == cppcms::json::is_array;
}


bool proxy_global::is_same( const BaseClient &client, cppcms::json::value &obj1, bool &param_changed, bool &remotes_changed ) const
{
   //NB!! This function does not currently work correctly if changing a port on the remote part.
   param_changed = true;
   remotes_changed = true;
   return false;
/*
   bool result = false;
   param_changed = false;
   remotes_changed = false;
   mylib::port_type client_port = 0;
   cppcms::utils::check_port( obj1, "port", client_port );
   if ( client.port() != client_port ) return false;
   if ( client_port > 0 || (client_port == 0 && is_provider(client)))
   {
      remotes_changed = true;
      cppcms::json::value res = obj1.find( "remotes" );
      if (res.type() == cppcms::json::is_array)
      {
         int i1 = res.array().size();
         if ( i1 == client.m_proxy_endpoints.size())
         {
            remotes_changed = false;
            for (int index = 0; index < client.m_proxy_endpoints.size(); index++)
            {           
               RemoteEndpoint ep;
               if (!ep.load(res[index]) || !(ep == client.m_proxy_endpoints[index]) )
               {
                  remotes_changed = true;
               }
            }
         }
      }
      // param_changes, we should set.
      if (! (client.m_active == cppcms::utils::check_bool(obj1,"active",true,false)))
      {
         param_changed = true;
      }
      result = true;
   }
   DOUT("Compare clients: " << client << " <=> " << obj1 << " result: " << result << " param: " << (int)param_changed << " remotes_changed: " << (int)remotes_changed);
   return result;
*/
}


bool proxy_global::is_same(const RemoteProxyHost &host, cppcms::json::value &obj, bool &param_changed, bool &locals_changed, std::vector<RemoteEndpoint> &rem_added, std::vector<RemoteEndpoint> &rem_removed) const
{
   bool result = false;
   param_changed = false;
   locals_changed = false;
   mylib::port_type host_port = 0;
   cppcms::utils::check_port( obj, "port", host_port );
   if ( host.port() != host_port ) return false;
   if ( host_port > 0)
   {
      cppcms::json::value res = obj.find( "remotes" );
      ASSERTD(res.type() == cppcms::json::is_array, "Invalid host without remotes in new configuration");
      for (auto rem : res.array())
      {
         RemoteEndpoint ep;
         ASSERTD(ep.load(rem),"Failed to load remote item in new configuration");
         if (std::find_if(host.m_remote_ep.begin(), host.m_remote_ep.end(),[&](const RemoteEndpoint &_ep){ return ep == _ep;} ) == host.m_remote_ep.end())
         {
            rem_added.push_back(ep);
         }
      }
      for (auto h : host.m_remote_ep)
      {
         bool hit = false;
         for (auto rem : res.array())
         {
            RemoteEndpoint ep;
            ASSERTD(ep.load(rem),"Failed to load remote item in new configuration");
            if (h == ep) hit = true;
         }
         if (!hit)
         {
            rem_removed.push_back(h);
         }
      }
      // Check for local connection changes.
      locals_changed = true;
      res = obj.find( "locals" );
      if (res.type() == cppcms::json::is_array)
      {
         int i1 = res.array().size();
         if (i1 == host.m_local_ep.size())
         {
            locals_changed = false;
            for (int index = 0; index < host.m_local_ep.size(); index++)
            {
               LocalEndpoint ep;
               if (!ep.load(res[index]) || !(ep == host.m_local_ep[index]) )
               {
                  locals_changed = true;
               }
            }
         }
      }

      // param_changes, we should set.
      if (! (host.m_plugin.m_type == cppcms::utils::check_string(obj,"type","",false)))
      {
         param_changed = true;
      }
      result = true;
   }
   DOUT("Compare: " << host << " <=> " << obj << " result: " << (int)result << " param: " << (int)param_changed << " remote: " << (int)locals_changed)
   return result;
}


// obj represents the new setup. So anything running that doesn't match the new object must be stopped and removed.
void proxy_global::unpopulate_json( cppcms::json::value obj )
{
   std::lock_guard<std::mutex> l(this->m_mutex_list);

   for ( auto iter = this->localclients.begin(); iter != this->localclients.end(); )
   {
      std::vector<RemoteEndpoint> rep;
      std::vector<LocalEndpoint> lec;
      bool keep = false;
      if ( obj["clients"].type() == cppcms::json::is_array )
      {
         cppcms::json::array clients = obj["clients"].array();
         for ( auto &client : clients )
         {
            bool param = false;
            bool connections = false;
            if (this->is_same(*(*iter),client,param,connections))
            {
               keep = !(param || connections);
            }
         }
      }
      if (keep)
      {
         DOUT("keeping client on port: " << (*iter)->port());
         iter++;
      }
      else
      {
         DOUT("Stopping and removing client on port: " << (*iter)->port());
         (*iter)->stop();
         iter = this->localclients.erase(iter);
      }
   }
   // Check all remotehosts. 
   // - Check if each is still active
   // - For those active check and remove any cancelled remotes
   for (auto eiter = this->remotehosts.begin(); eiter != this->remotehosts.end();)
   {
      auto &ehost = (*(*eiter)); // Existing host
      bool keep = false;
      cppcms::json::value kobj;
      std::vector<RemoteEndpoint> added, removed;
      if ( obj["hosts"].type() == cppcms::json::is_array ) // We cannot assert here, because we may have removed all hosts in the new config.
      {
         cppcms::json::array newhosts = obj["hosts"].array();
         for ( auto &newhost : newhosts )
         {
            bool param = false;
            bool local_changes = false;
            if (this->is_same(ehost,newhost,param,local_changes, added, removed))
            {
               kobj = newhost;
               keep = !(param || local_changes);
            }
         }
      }
      if (keep)
      {
         DOUT("keeping host on port: " << ehost.port() << " Checking and removing any cancelled remote connections, count: " << removed.size());
         ehost.remove_any(removed);

         eiter++;
      }
      else
      {
         DOUT("Stopping and removing host on port: " << ehost.port());
         ehost.stop();
         eiter = this->remotehosts.erase(eiter);
      }
      DOUT("remotehost.Stop() a");
   }
}


// The function will check for each of the main entries. If a main entry exists, then it is will overwrite, i.e. destroy any existing information.
// NB!! This should not be called while threads are active.
void proxy_global::populate_json( cppcms::json::value &obj, int _json_acl )
{
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   this->reset();
   if ( (_json_acl & clients) > 0 && obj["clients"].type() == cppcms::json::is_array )
   {
      DOUT(__FUNCTION__ << " populating clients");

      cppcms::json::array ar = obj["clients"].array();
      for ( auto iter1 = ar.begin(); iter1 != ar.end(); iter1++ )
      {
         auto &item1 = *iter1;
         bool active = cppcms::utils::check_bool( item1, "active", true, false );
         bool provider = cppcms::utils::check_bool(item1, "provider", false, false);
         mylib::port_type client_port = cppcms::utils::check_int(item1, "port", 0, !provider);
         mylib::port_type activate_port = cppcms::utils::check_int(item1, "activate.port", 25500, false);
         auto iter_cur = std::find_if(this->localclients.begin(), this->localclients.end(), [&](const baseclient_ptr& client){ return client_port == client->port(); });
         if (iter_cur != this->localclients.end())
         {
            DOUT("Found existing running client on port: " << client_port);
            continue;
         }

         std::string shelp;
         boost::posix_time::time_duration read_timeout = boost::posix_time::minutes(5);
         if (cppcms::utils::check_string( item1, "read_timeout", shelp ) && !shelp.empty())
         {
            mylib::from_string(shelp, read_timeout);
            DOUT("Read timeout from configuration: " << read_timeout);
         }
         bool auto_reconnect = false;
         if (cppcms::utils::check_bool(item1, "auto_reconnect", auto_reconnect))
         {
            DOUT("Auto reconnect: " << auto_reconnect);
         }
         int max_connections = cppcms::utils::check_int( item1, "max_connections", 1, false );
         std::vector<RemoteEndpoint> proxy_endpoints;
         std::vector<LocalEndpoint> provider_endpoints;
         if ( item1["remotes"].type() == cppcms::json::is_array )
         {
            auto ar2 = item1["remotes"].array();
            for ( auto iter2 = ar2.begin(); iter2 != ar2.end(); iter2++ )
            {
               auto &item2 = *iter2;
               RemoteEndpoint ep;
               if (ep.load(item2))
               {
                  proxy_endpoints.push_back( ep );
               }
            }
         }
         if (provider)
         {
            if ( item1["locals"].type() == cppcms::json::is_array ) // Only for provider
            {
               auto ar2 = item1["locals"].array();
               for ( auto iter2 = ar2.begin(); iter2 != ar2.end(); iter2++ )
               {
                  auto &item2 = *iter2;
                  LocalEndpoint ep;
                  if (ep.load(item2))
                  {
                     provider_endpoints.push_back( ep );
                  }
               }
            }
            if (provider_endpoints.size() > 0 )
            {
               baseclient_ptr local_ptr = std::make_shared<ProviderClient>(active, activate_port, provider_endpoints, proxy_endpoints, standard_plugin, item1);
               this->localclients.push_back( local_ptr );
            }
            else
            {
               // NB!! Here should go an error if active
               DERR("Provider configuration invalid");
            }
         }
         else if ( active && proxy_endpoints.size() > 0 )
         {
            // NB!! Search for the correct plugin version
            baseclient_ptr local_ptr(new LocalHost(active, client_port, activate_port, proxy_endpoints, max_connections, standard_plugin, read_timeout, auto_reconnect));
            this->localclients.push_back( local_ptr );
         } // NB!! What else if one of them is empty
      }
   }
   if ( (_json_acl & hosts) > 0 && obj["hosts"].type() == cppcms::json::is_array )
   {
      cppcms::json::array ar1 = obj["hosts"].array();
      for ( auto iter1 = ar1.begin(); iter1 != ar1.end(); iter1++ )
      {
         bool found = false;
         auto &item1 = *iter1;
         for (auto &host : this->remotehosts) // This should be std::find_if
         {
            bool param_changed = false;
            bool locals_changed = false;
            std::vector<RemoteEndpoint> rem_added, rem_removed;
            if (this->is_same(*host, item1, param_changed, locals_changed, rem_added, rem_removed))
            {
               ASSERTD(!(param_changed || locals_changed), "Host should have been deleted: " + mylib::to_string(host->port()));
               DOUT("Found: " << host->port() << " for: " << item1 << " values: " << param_changed << " " << rem_added.size() << " " << rem_removed.size() << " " << locals_changed);
               found = true;
               host->add_remotes(rem_added);
               host->remove_remotes(rem_removed);
            }
         }
         if (!found)
         {
            //DOUT("Didn't find: " << item1.port());
            // We must have a port number and at least one local address and one remote address
            mylib::port_type host_port = cppcms::utils::check_int( item1, "port", 0, true );
            std::vector<LocalEndpoint> local_endpoints;
            if ( item1["locals"].type() == cppcms::json::is_array )
            {
               auto ar2 = item1["locals"].array();
               for ( auto iter2 = ar2.begin(); iter2 != ar2.end(); iter2++ )
               {
                  auto &item2 = *iter2;
                  LocalEndpoint addr;
                  if (addr.load(item2)) local_endpoints.push_back( addr );
               }
            }
            std::vector<RemoteEndpoint> remote_endpoints;
            if ( item1["remotes"].type() == cppcms::json::is_array )
            {
               auto ar2 = item1["remotes"].array();
               for ( auto iter2 = ar2.begin(); iter2 != ar2.end(); iter2++ )
               {
                  auto &item2 = *iter2;
                  RemoteEndpoint ep;
                  if (ep.load(item2)) remote_endpoints.push_back( ep );
               }
            }
            // Check size > 0
            PluginHandler *plugin = &standard_plugin;
            std::string plugin_type = cppcms::utils::check_string( item1, "type", "", false );
            if ( plugin_type.length() > 0 )
            {
               for ( auto iter3 = PluginHandler::plugins().begin(); iter3 != PluginHandler::plugins().end(); iter3++ )
               {
                  if ( (*iter3)->m_type == plugin_type )
                  {
                     plugin = (*iter3);
                  }
               }
               // NB!! What if we don't find the right one ? now we simply default to the empty one.
            }
            if ( remote_endpoints.size() > 0 && local_endpoints.size() > 0 )
            {
               remotehost_ptr remote_ptr = std::make_shared<RemoteProxyHost>( host_port, remote_endpoints, local_endpoints, *plugin );
               this->remotehosts.push_back( remote_ptr );
            } // NB!! What else if one of them is empty
         }
      }
   }
   if ( (_json_acl & web) > 0 && obj["web"].type() == cppcms::json::is_object )
   {
      cppcms::json::value &web_obj( obj["web"] );
      cppcms::utils::check_port( web_obj, "port", this->m_web_port );
   }
   if ( (_json_acl & config) > 0 && obj["config"].type() == cppcms::json::is_object )
   {
      int i;
      std::string shelp;
      cppcms::json::value &config_obj( obj["config"] );
      cppcms::utils::check_string( config_obj, "name", this->m_name );
      cppcms::utils::check_bool( config_obj, "debug", this->m_debug );
      cppcms::utils::check_string( config_obj, "log_path", global.m_log_path );
      cppcms::json::value proxies = config_obj.find( "uniproxies" );
      if (cppcms::utils::check_int( config_obj, "activate.timeout", i ))
      {
         this->m_activate_timeout = boost::posix_time::seconds(i);
         DOUT("Activate timeout: " << this->m_activate_timeout);
      }
      else if (cppcms::utils::check_string( config_obj, "activate.timeout", shelp))
      {
         mylib::from_string(shelp, this->m_activate_timeout);
         DOUT("Activate timeout: " << this->m_activate_timeout);
      }
      cppcms::utils::check_port( config_obj, "activate.port", this->m_activate_port );
      cppcms::utils::check_int(config_obj, "min_tls_protocol", this->min_tls_protocol);
      cppcms::utils::check_bool(config_obj, "accept_short_certificates", this->accept_short_certs);
      DOUT("min_tls_protocol: " << this->min_tls_protocol << " accept_short_certificates: " << this->accept_short_certs);
      ASSERTE(this->min_tls_protocol >= 12 && this->min_tls_protocol <= 13, uniproxy::error::parse_file_failed, OSS("Invalid TLS protocol, must be 12 r 13 is: " << this->min_tls_protocol));
// NB!! Check if new uniproxies
      if ( proxies.type() == cppcms::json::is_array )
      {
         for (auto &item : proxies.array())
         {
            LocalEndpoint ep;
            ep.m_port = 8085;
            if (ep.load(item)) // NB!! Should be assert??
            {
               if (std::find_if(this->uniproxies.begin(), this->uniproxies.end(), [&](const LocalEndpoint &_ep){ return _ep == ep; }) == this->uniproxies.end())
               {
                  this->uniproxies.push_back(ep);
               }
            }
         }
         std::ostringstream oss;
         std::copy(this->uniproxies.begin(), this->uniproxies.end(),std::ostream_iterator<LocalEndpoint>(oss, "\r\n"));
         DOUT("Loaded proxies: " << oss.str());
      }
   }
}


bool proxy_global::is_new_configuration(cppcms::json::value &newobj) const
{
   if (this->m_name.empty())
      return true;
   if (newobj["config"].type() != cppcms::json::is_object) // Should possibly reject this update ?
      return true;
   if (cppcms::utils::check_string(newobj,"config.name","",false) != this->m_name)
      return true;
   cppcms::json::value proxies = newobj.find( "config.uniproxies" );
   int new_uniproxy_count = 0;
   std::ostringstream oss;
   std::copy(this->uniproxies.begin(), this->uniproxies.end(), std::ostream_iterator<LocalEndpoint>(oss, "\r\n"));
   DOUT("Uniproxies: " << oss.str());
   if ( proxies.type() == cppcms::json::is_array )
   {
      for (auto &item : proxies.array())
      {
         new_uniproxy_count++;
         LocalEndpoint ep;
         if (!ep.load(item))
            return true;
         if (std::find_if(this->uniproxies.begin(), this->uniproxies.end(), [&](const LocalEndpoint &_ep)
         {
            DOUT("Compare: " << ep << " == " << _ep << " count: " << new_uniproxy_count);
            return _ep == ep;
         }) == this->uniproxies.end())
            return true;
      }
   }
   else if (!this->uniproxies.empty())
   {
      return this->uniproxies.size() != new_uniproxy_count;    
   }
   return false;
}


std::string proxy_global::save_json_status( bool readable )
{
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   cppcms::json::value glob;

   // ---- THE CLIENT PART ----
   for ( int index1 = 0; index1 < this->localclients.size(); index1++ )
   {
      cppcms::json::value obj = this->localclients[index1]->save_json_status();
      glob["clients"][index1] = obj;
   }

   // ---- THE HOST PART ----
   for ( int index = 0; index < this->remotehosts.size(); index++ )
   {
      cppcms::json::value obj = this->remotehosts[index]->save_json_status();
      glob["hosts"][index] = obj;
   }
   cppcms::json::object config_obj;
   config_obj["name"] = this->m_name;
   glob["global"] = config_obj;
   glob["version"] = version;

   std::ostringstream os;
   glob.save( os, readable );
   return os.str();
}


bool proxy_global::execute_openssl()
{
   std::string params;
#ifdef _WIN32
   params = " -config openssl.cfg ";
#endif
   int res = process::execute_process("openssl",
                                      " req " + params + "-x509 -nodes -days 100000 -subj /C=DK/ST=Denmark/L=GateHouse/CN=" +
                                      this->m_name + " -newkey rsa:3072 -keyout my_private_key.pem -out my_public_cert.pem ",
                                      [&](const std::string &_out)
                                      {
                                         DOUT(_out);
                                      },
                                      [&](const std::string &_err)
                                      {
                                         DERR(_err);
                                      });
   DOUT("Execute openssl result: " << res);
   return res == 0;
}


//
bool proxy_global::load_configuration()
{
   DOUT("Loading configuration from " << config_filename);
   static int openssl_count = 0;
   std::string certificate_common_name;
   try
   {
      {
         int line = 0;
         cppcms::json::value my_object;
         std::ifstream ifs( config_filename );
         ASSERTE(my_object.load( ifs, false, &line ), uniproxy::error::parse_file_failed, config_filename + " on line: " + mylib::to_string(line));
         this->populate_json(my_object,proxy_global::config|proxy_global::web);
      }
      ASSERTE(!this->m_name.empty(),uniproxy::error::certificate_name_unknown, config_filename + " should contain \"config\" : { \"name\" : \"own_certificate_name\" }");

      // NB!! For these we should not overwrite any existing files. If the files exist, but there is an error in them we need to get user interaction ??
      bool load_private = false;
      bool load_public = false;
      {
         std::ifstream ifs( my_private_key_name );
         if ( ifs.good() )
         {
            boost::system::error_code ec1,ec2;
            boost::asio::ssl::context ctx(boost::asio::ssl::context::tls);
            set_ssl_context(ctx);

            ctx.use_private_key_file(my_private_key_name,boost::asio::ssl::context_base::file_format::pem,ec1);
            ctx.use_certificate_file(my_public_cert_name,boost::asio::ssl::context_base::file_format::pem,ec2);
            load_private = !ec1 && !ec2;
            DOUT("Private handling: " << load_private << " 1:" << ec1 << " " << (ec1) << " " << !ec1 << " 2:" << ec2 << " " << (ec2) << " " << !ec2);
            if (!load_private)
            {
               log().add("Found private file which does NOT contain a valid private key. Delete file: " + my_private_key_name + " and try again");
            }
         }
      }
      {
         std::vector<certificate_type> certs;
         load_public = load_certificates_file( my_public_cert_name, certs ) && certs.size() > 0;
         if ( load_public )
         {
            certificate_common_name = get_common_name(certs[0]);
            DOUT("Found own private certificate with name: \"" << certificate_common_name + "\"");
            if ( this->m_name == certificate_common_name )
            {
               DOUT("Certificate do match own name");
            }
            else
            {
               log().add("Own name \"" + this->m_name + "\" does not match own certificate \"" + certificate_common_name + "\"");
            }
         }
      }
      if ( ! (load_private && load_public) )
      {
         if ( this->m_name.length() == 0 )
         {
            throw std::runtime_error("certificate files not found and cannot be generated because the name is not specified in the configuration json file");
         }
         if ( this->execute_openssl() )
         {
            // NB!! We should now be ready to try once again. Can we end up in an endless loop here ? Guard anyway.
            if ( ++openssl_count < 3 )
            {
               log().add("Succesfully executed the initial OpenSSL command");
               throw mylib::reload_exception();
            }
            log().add("Unkown fatal error, possibly endless loop!");
         }
         else
         {
            log().add("Failed to execute initial OpenSSL command. Is OpenSSL properly installed and available in the path");
         }
      }
      {  // Sort of a safe create if not exist
         std::ifstream ifs( my_certs_name );
         if ( !ifs.good() )
         {
            std::ofstream ofs( my_certs_name );
         }
      }

      // 
      if ( this->m_name.length() > 0 && this->m_name == certificate_common_name )
      {
         int line;
         this->m_new_setup.null(); // Reset to ensure we don't have undefined behaviour.
         std::ifstream ifs( config_filename );
         if (!this->m_new_setup.load( ifs, false, &line ) )
         {
            log().add("Failed to load and parse configuration file: " + config_filename + " on line: " + mylib::to_string(line));
         }
         this->populate_json(this->m_new_setup,proxy_global::all);
      }
      this->load_certificate_names( my_certs_name );
   }
   catch( std::exception &exc )
   {
      DOUT(__FUNCTION__ << " exception: " << exc.what());
      return false;
   }
   return true;
}


// NB!! Currently for debugging only.
std::string proxy_global::save_json_config( bool readable )
{
   std::lock_guard<std::mutex> l(this->m_mutex_list);
   cppcms::json::value glob;
   // The client part
   for ( int index = 0; index < this->localclients.size(); index++ )
   {
      cppcms::json::value obj = this->localclients[index]->save_json_config();
      glob["clients"][index] = obj;
   }
   // The host part
   for ( int index = 0; index < this->remotehosts.size(); index++ )
   {
      auto obj = this->remotehosts[index]->save_json_config();
      glob["hosts"][index] = obj;
   }

   cppcms::json::object web_obj;
   web_obj["port"] = this->m_web_port;
   web_obj["ip4"] = this->m_ip4_mask;
   glob["web"] = web_obj;

   cppcms::json::object config_obj;
   config_obj["debug"] = this->m_debug;
   config_obj["name"] = this->m_name;
   glob["global"] = config_obj;

   std::ostringstream os;
   glob.save( os, readable );
   return os.str();
}


//-----------------------------------


session_data &proxy_global::get_session_data( cppcms::session_interface &_session )
{
   std::lock_guard<std::mutex> l(this->m_session_data_mutex);
   int id;
   if ( _session.is_set( "id" ) )
   {
      id = mylib::from_string(_session.get( "id" ),id);
   }
   else
   {
      id = rand();
      _session.set( "id", mylib::to_string(id) );
   }

   for ( int index = 0; index < this->m_sessions.size(); index++ )
   {
      if ( this->m_sessions[index]->m_id == id )
      {
         this->m_sessions[index]->update_timestamp();
         return *this->m_sessions[index];
      }
   }
   DOUT("Adding new session for id: " << id);
   auto p = std::make_shared<session_data>( id );
   this->m_sessions.push_back( p );
   return *p;
}


void proxy_global::clean_session()
{
   std::lock_guard<std::mutex> l(this->m_session_data_mutex);

}


bool proxy_global::load_certificate_names( const std::string & _filename )
{
   std::lock_guard<std::mutex> lock(this->m_mutex_certificates);
   std::string names;
   bool result = false;
   std::vector< certificate_type > certs;
   m_cert_names.clear();
   if ( load_certificates_file( _filename, certs ) )
   {
      for ( auto iter = certs.begin(); iter != certs.end(); iter++ )
      {
         m_cert_names.push_back( get_common_name( *iter ) );
         names += "|" + get_common_name( *iter );
      }
      result = true;
   }
   DOUT("Load certificates from file: " << names );
   return result;
}


std::string proxy_global::SetupCertificatesServer(boost::asio::ip::tcp::socket& _remote,
                                                  boost::asio::io_service& _io_service,
                                                  const std::vector<std::string>& _certnames)
{
   try
   {
      DOUT(__FUNCTION__ << " certificate names: " << _certnames);
      ASSERTE( _certnames.size() > 0, uniproxy::error::connection_name_unknown, "" );

      const int buffer_size = 4000;
      char buffer[buffer_size];
      memset(buffer, 0, buffer_size);
      read_with_timeout(_remote, _io_service, boost::asio::buffer(buffer, buffer_size),
                        boost::posix_time::seconds(5));
      int length = strlen(buffer);

      ASSERTE(length > 0 && length < buffer_size, uniproxy::error::certificate_invalid, "received"); // NB!! Check the overflow situation.....
      DOUT(info(_remote) << "SSL Possible Certificate received (length " << length << "): " << buffer);
      std::vector<certificate_type> remote_certs, local_certs;

      ASSERTE(load_certificates_string( buffer, remote_certs ) && remote_certs.size() == 1, uniproxy::error::certificate_invalid, "received");
      std::string remote_name = get_common_name( remote_certs[0] );
      DOUT(info(_remote) << "Received certificate name: " << remote_name << " for connection: " << _certnames);
      
      if (std::find(_certnames.begin(), _certnames.end(), remote_name) == _certnames.end())
      {
         DOUT(info(_remote) << "Certificate exchange attempt by: " << remote_name << " did not match any expected: " << _certnames);
         return std::string();
      }

      ASSERTE( load_certificates_file( my_certs_name, local_certs ), uniproxy::error::certificate_not_found,
               std::string( " in file ") + my_certs_name + " for connection: " );

      for ( auto iter = local_certs.begin(); iter != local_certs.end(); )
      {
         if ( remote_name == get_common_name( *iter ) )
         {
            DOUT(info(_remote) << "Removing old existing certificate name for replacement: " << remote_name);
            iter = local_certs.erase( iter );
         }
         else
         {
            iter++;
         }
      }
      local_certs.push_back( remote_certs[0] );
      save_certificates_file( my_certs_name, local_certs );
      this->load_certificate_names( my_certs_name );
      return remote_name;
   }
   catch (std::exception& exc)
   {
      DOUT(info(_remote) << __FUNCTION__ << " exception: " << exc.what());
      log().add(exc.what());
   }
   return std::string();
}


bool proxy_global::SetupCertificatesClient(boost::asio::ip::tcp::socket& _remote, const std::string& _connection_name)
{
   try
   {
      DOUT(info(_remote) << __FUNCTION__ << " connection name: " << _connection_name);
      std::this_thread::sleep_for(std::chrono::seconds(1));
      const int buffer_size = 4000;
      char buffer[buffer_size];
      memset(buffer, 0, buffer_size);
      ASSERTE( _connection_name.length() > 0, uniproxy::error::connection_name_unknown, "" );

      std::string cert = readfile( my_public_cert_name );
      ASSERTE( cert.length() > 0, uniproxy::error::certificate_not_found, std::string( " certificate not found in file: ") + my_certs_name );
      int count = _remote.write_some( boost::asio::buffer( cert, cert.length() ) );
      DOUT(info(_remote) << "Wrote certificate: " << count << " of " << cert.length() << " bytes ");
      DOUT("Sent: " << cert);
      return true;
   }
   catch( std::exception &exc )
   {
      DOUT(info(_remote) << __FUNCTION__ << " exception: " << exc.what());
      log().add(exc.what());
   }
   return false;
}


bool proxy_global::certificate_available( const std::string &_cert_name)
{
   std::lock_guard<std::mutex> lock(this->m_mutex_certificates);
   return std::find(this->m_cert_names.begin(), this->m_cert_names.end(), _cert_name ) != this->m_cert_names.end();
}

static std::string get_password()
{
   return "1234";
}

void proxy_global::set_ssl_context(boost::asio::ssl::context& ctx)
{
   switch (this->min_tls_protocol)
   {
   case 12:
      ctx.set_options(boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::no_sslv3
            | boost::asio::ssl::context::no_tlsv1
            | boost::asio::ssl::context::no_tlsv1_1
            | boost::asio::ssl::context::single_dh_use
            );
      break;
   default:
      ctx.set_options(boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::no_sslv3
            | boost::asio::ssl::context::no_tlsv1
            | boost::asio::ssl::context::no_tlsv1_1
            | boost::asio::ssl::context::no_tlsv1_2
            | boost::asio::ssl::context::single_dh_use
            );
      break;
   }
   ctx.set_password_callback(boost::bind(&get_password));
   ctx.set_verify_callback([this](bool preverified, boost::asio::ssl::verify_context& ctx)
   {
      if (!preverified)
      {
         X509_STORE_CTX *cts = ctx.native_handle();
         int error = X509_STORE_CTX_get_error(cts);
         if (error == X509_V_ERR_EE_KEY_TOO_SMALL)
         {
            if (this->accept_short_certs)
            {
               DOUT("Certificate too short but otherwise valid");
               preverified = true;
            }
            else
            {
               DOUT("Certificate too short but otherwise valid, consider setting: config.accept_short_certs");
            }
         }
      }
      return preverified; 
   });
   ctx.set_verify_mode(boost::asio::ssl::context::verify_peer|boost::asio::ssl::context::verify_fail_if_no_peer_cert);
   load_verify_file(ctx, my_certs_name);
   ctx.use_certificate_chain_file(my_public_cert_name);
   ctx.use_private_key_file(my_private_key_name, boost::asio::ssl::context::pem);
}


//-------------------------------------


client_certificate_exchange::client_certificate_exchange()
   : mylib::thread([](){ })
{
}

void client_certificate_exchange::lock()
{
}


void client_certificate_exchange::unlock()
{
   this->mylib::thread::stop();
}


void client_certificate_exchange::start( const std::vector<LocalEndpoint> &eps )
{
   this->mylib::thread::start([this, eps](){ this->thread_proc(eps); });
}


void client_certificate_exchange::thread_proc( const std::vector<LocalEndpoint> eps )
{
   DOUT("Certificates thread started with params: " << eps.size());
   this->sleep(5000);
   for (auto ep : eps)
   {
      DOUT(": " << ep.m_hostname << ":" << ep.m_port);
   }
   while (this->check_run())
   {
      if (global.m_activate_host.size())
      {
         this->sleep(20000);
      }
      else
      {
         this->sleep(60000);
      }
      for (auto &ep : eps)
      {
         try
         {
            std::string host = ep.m_hostname + ":" + mylib::to_string(ep.m_port);
            std::string output;
            //DOUT("Get certificate from: " << host);
            bool result = httpclient::sync(host,"/json/command/certificate/get/",output);
            if (result)
            {
               std::vector<certificate_type> own,remotes;
               ASSERTE(load_certificates_string(output, remotes), uniproxy::error::parse_file_failed, "Failed to load certificates from remote host");
               if (remotes.empty())
               {
                  continue;
               }
               load_certificates_file(my_certs_name,own);
               int ownsize = own.size();
               for (auto rem : remotes)
               {
                  bool ok = false;
                  for (auto cert : own)
                  {
                     if (get_common_name(cert) == get_common_name(rem))
                     {
                        ok = true;
                        // NB!! Check if cert are otherwise equal.
                        if (!equal(cert,rem))
                        {
                           DERR("Certificate with name: " << get_common_name(cert) << " has changed contents");
                        }
                     }
                  }
                  if (!ok)
                  {
                     DOUT("Adding cert: " << get_common_name(rem));
                     own.push_back(rem);
                     global.m_activate_host.remove(get_common_name(rem));
                  }
               }
               if (ownsize != own.size())
               {
                  save_certificates_file(my_certs_name,own);
                  global.load_certificate_names( my_certs_name );
               }
            }
            output = "";
            result = httpclient::sync(host,"/json/command/certificate/public/",output);
            if (result)
            {
               std::vector<certificate_type> own_cert, rem_cert;
               ASSERTE(load_certificates_string(output, rem_cert) && rem_cert.size() == 1, uniproxy::error::parse_file_failed, "Failed to load public certificate from remote host: " + output);
               ASSERTE(load_certificates_file(my_public_cert_name, own_cert) && own_cert.size() == 1, uniproxy::error::parse_file_failed, "Failed to load own public certificate");
               ASSERTE(equal(own_cert.front(), rem_cert.front()), uniproxy::error::parse_file_failed, "Own public Certificate are NOT equal between self and " + host);
            }
         }
         catch (std::exception& exc)
         {
            log().add("Failed to exchange certificates with " + ep.m_hostname + ":" + mylib::to_string(ep.m_port) + " => " + exc.what());
         }
      }
   }
}


//----------------------------------------------


activate_host::activate_host()
   : mylib::thread([this]{this->interrupt();})
{
   
}


void activate_host::start(int _port)
{
   if (!this->mylib::thread::is_running())
   {
      DOUT("Starting activation host on port: " << _port);
      this->mylib::thread::start([this, _port]{this->threadproc(_port);});
   }
   else
   {
      int port = -1;
      std::lock_guard<std::mutex> lock(this->m_mutex_activate);
      if (this->mp_acceptor != nullptr)
      {
         boost::system::error_code ec;
         boost::asio::ip::tcp::endpoint ep = this->mp_acceptor->local_endpoint(ec);
         port = ep.port();
      }
      DOUT("Activation host already running on port: " << port << " new port: " << _port);
   }
}


void activate_host::interrupt()
{
   DOUT(__FUNCTION__);
   if (int sock = get_socket(this->mp_acceptor, this->m_mutex_activate); sock != 0)
   {
      // Do we need a cancel??
      shutdown(sock, boost::asio::socket_base::shutdown_both);
   }
   if (int sock = get_socket(this->mp_socket, this->m_mutex_activate); sock != 0)
   {
      shutdown(sock, boost::asio::socket_base::shutdown_both);
   }
}


void activate_host::add(std::string _certname)
{
   std::lock_guard<std::mutex> l(this->m_mutex_activate);

   this->cleanup();

   if (std::find_if(this->m_activate.begin(), this->m_activate.end(), [&](const activate_t&ac){ return ac.m_activate_name == _certname;}) == this->m_activate.end())
   {
      activate_t ac;
      ac.m_activate_stamp = boost::get_system_time() + boost::posix_time::seconds(global.m_activate_timeout);
      ac.m_activate_name = _certname;
      this->m_activate.push_back(ac);
   }
}


void activate_host::threadproc(int _port)
{
   while(this->check_run())
   {
      while (this->size() == 0)
      {
         this->sleep(10000);
      }
      try
      {
         boost::asio::io_service io_service;
         boost::asio::ip::tcp::acceptor acceptor( io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), _port) );
         boost::asio::ip::tcp::socket socket(io_service);
         mylib::protect_pointer<boost::asio::ip::tcp::socket> socket_lock(this->mp_socket, socket, this->m_mutex_activate);
         mylib::protect_pointer<boost::asio::ip::tcp::acceptor> acceptor_lock(this->mp_acceptor, acceptor, this->m_mutex_activate);
         DOUT(__FUNCTION__ << " acceptor ready");

         acceptor.accept(socket);
         DOUT(__FUNCTION__ << " new certificate client connected");
         std::vector<std::string> activate_names;
         {
            std::lock_guard<std::mutex> lock(this->m_mutex_activate);
            for (auto &elm : this->m_activate)
            {
               if (elm.m_activate_stamp > boost::get_system_time())
               {
                  activate_names.push_back(elm.m_activate_name);
               }
            }
         }
         io_service.reset();
         std::string certname = global.SetupCertificatesServer(socket, io_service, activate_names);
         if (!certname.empty() && global.SetupCertificatesClient(socket, certname))
         {
            DOUT("Succeeded in exchanging certificates for " << certname);
            std::lock_guard<std::mutex> lock(this->m_mutex_activate);
            for (auto iter = this->m_activate.begin(); iter != this->m_activate.end(); iter++)
            {
               if (iter->m_activate_name == certname)
               {
                  DOUT("Found and removing from activation list: " << certname);
                  this->m_activate.erase(iter);
                  break;
               }
            }
         }
         else
         {
            DOUT("Failed to exchange certificates for " << certname);
         }
      }
      catch(std::exception &exc)
      {
         DOUT("Exception in certificate client: " << exc.what());
      }
   }
}


bool activate_host::remove(const std::string &certname)
{
   std::lock_guard<std::mutex> lock(this->m_mutex_activate);
   this->cleanup();
   for (auto iter = this->m_activate.begin(); iter != this->m_activate.end(); iter++)
   {
      if (iter->m_activate_name == certname)
      {
         DOUT("Found and removing from activation list: " << certname);
         this->m_activate.erase(iter);
         return true;
      }
   }
   return false;
}


bool activate_host::is_in_list(const std::string &_certname, boost::posix_time::ptime &_timeout)
{
   std::lock_guard<std::mutex> lock(this->m_mutex_activate);
   this->cleanup();
   auto iter = std::find_if(this->m_activate.begin(), this->m_activate.end(), [&](const activate_t&ac){ return ac.m_activate_name == _certname;});
   if (iter != this->m_activate.end())
   {
      _timeout = iter->m_activate_stamp;
      return true;
   }
   return false;
}


// Only called locally, i.e. no mutex
void activate_host::cleanup()
{
   for (auto iter = this->m_activate.begin(); iter != this->m_activate.end(); )
   {
      if (iter->m_activate_stamp <= boost::get_system_time())
      {
         iter = this->m_activate.erase(iter);
      }
      else
      {
         iter++;
      }
   }
}

void load_verify_file(boost::asio::ssl::context& ctx, const std::string& filename)
{
   if (boost::filesystem::exists(filename) && boost::filesystem::file_size(filename) > 0)
   {
      ctx.load_verify_file(filename);
   }
}
