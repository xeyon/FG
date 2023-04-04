// httpd.cxx -- a http daemon subsystem based on Mongoose http
//
// Written by Torsten Dreyer, started April 2014.
//
// Copyright (C) 2014  Torsten Dreyer
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "config.h"

#include "httpd.hxx"
#include "HTTPRequest.hxx"
#include "PropertyChangeWebsocket.hxx"
#include "MirrorPropertyTreeWebsocket.hxx"
#include "ScreenshotUriHandler.hxx"
#include "PropertyUriHandler.hxx"
#include "JsonUriHandler.hxx"
#include "FlightHistoryUriHandler.hxx"
#include "PkgUriHandler.hxx"
#include "RunUriHandler.hxx"
#include "NavdbUriHandler.hxx"
#include "PropertyChangeObserver.hxx"
#include <Main/fg_props.hxx>

#include <mongoose.h>
#include <cJSON.h>

#include <string>
#include <vector>

#ifdef MG_VERSION
#define MONGOOSE_VERSION MG_VERSION
#undef poll
enum mg_result { MG_FALSE=0,
                 MG_TRUE,
                 MG_MORE };
#endif

using std::string;
using std::vector;

namespace flightgear {
namespace http {

const char * PROPERTY_ROOT = "/sim/http";


// this structure is used to save corresponding messages in mg_connection.data[]
struct Mg_Message_Array {
    struct mg_http_message* http_msg;
    struct mg_ws_message* ws_msg;
};

/**
 * A Helper class for URI Handlers
 *
 * This class stores a list of URI Handlers and provides a lookup
 * method for find the handler by it's URI prefix
 */
class URIHandlerMap: public vector<SGSharedPtr<URIHandler> > {
public:
  /**
   * Find a URI Handler for a given URI
   *
   * Look for the first handler with a uri matching the beginning
   * of the given uri parameter.
   *
   * @param uri The uri to find the handler for
   * @return a SGSharedPtr of the URIHandler or an invalid SGSharedPtr if not found
   */
  SGSharedPtr<URIHandler> findHandler(const std::string & uri)
  {
    for (iterator it = begin(); it != end(); ++it) {
      SGSharedPtr<URIHandler> handler = *it;
      // check if the request-uri starts with the registered uri-string
      if (0 == uri.find(handler->getUri())) return handler;
    }
    return SGSharedPtr<URIHandler>();
  }
};

/**
 * A Helper class to create a HTTPRequest from a mongoose connection struct
 */
class MongooseHTTPRequest: public HTTPRequest {
private:
  /**
   * Creates a std::string from a char pointer and an optionally given length
   * If the pointer is NULL or the length is zero, return an empty string
   * If no length is given, create a std::string from a c-string (up to the /0 terminator)
   * If length is given, use as many chars as given in length (can exceed the /0 terminator)
   *
   * @param cp Points to the source of the string
   * @param len The number of chars to copy to the new string (optional)
   * @return a std::string containing a copy of the source
   */
  static inline string NotNull(const char * cp, size_t len = string::npos)
  {
    if ( NULL == cp || 0 == len) return string("");
    if (string::npos == len) return string(cp);
    return string(cp, len);
  }

public:
  /**
   * Constructs a HTTPRequest from a mongoose connection struct
   * Copies all fields into STL compatible local elements, performs urlDecode etc.
   *
   * @param connection the mongoose connection struct with the source data
   */
  MongooseHTTPRequest(struct mg_connection * connection)
  {
      struct Mg_Message_Array* msg = (struct Mg_Message_Array*)(connection->data);
      struct mg_http_message* req_msg = msg->http_msg;
      struct mg_ws_message* ws_msg = msg->ws_msg;

      Method = NotNull(req_msg->method.ptr, req_msg->method.len);
      Uri = urlDecode(NotNull(req_msg->uri.ptr, req_msg->uri.len));
      HttpVersion = NotNull(req_msg->proto.ptr, req_msg->proto.len); // HTTP/1.1
      QueryString = NotNull(req_msg->query.ptr, req_msg->query.len);

      //remoteAddress = NotNull(connection->loc.ip);   // we aren't processing the IP here
      remotePort = connection->rem.port;
      //localAddress = NotNull(connection->local_ip);
      localPort = connection->loc.port;

      if (connection->is_websocket && ws_msg)
      {
          Content = NotNull(ws_msg->data.ptr, ws_msg->data.len);

      } else {
          using namespace simgear::strutils;
          string_list pairs = split(string(QueryString), "&");
          for (string_list::iterator it = pairs.begin(); it != pairs.end(); ++it) {
              string_list nvp = split(*it, "=");
              if (nvp.size() != 2) continue;
              RequestVariables.insert(make_pair(urlDecode(nvp[0]), urlDecode(nvp[1])));
          }

          for (int i = 0; i < MG_MAX_HTTP_HEADERS; i++)
              if (req_msg->headers[i].name.ptr && req_msg->headers[i].value.ptr)
                  HeaderVariables[string(req_msg->headers[i].name.ptr, req_msg->headers[i].name.len)]
                  = string(req_msg->headers[i].value.ptr, req_msg->headers[i].value.len);

          Content = NotNull(req_msg->body.ptr, req_msg->body.len);
      }
  }

  /**
   * Decodes a URL encoded string
   * replaces '+' by ' '
   * replaces %nn hexdigits
   *
   * @param s The source string do decode
   * @return The decoded String
   */
  static string urlDecode(const string & s)
  {
    string r = "";
    int max = s.length();
    int a, b;
    for (int i = 0; i < max; i++) {
      if (s[i] == '+') {
        r += ' ';
      } else if (s[i] == '%' && i + 2 < max && isxdigit(s[i + 1]) && isxdigit(s[i + 2])) {
        i++;
        a = isdigit(s[i]) ? s[i] - '0' : toupper(s[i]) - 'A' + 10;
        i++;
        b = isdigit(s[i]) ? s[i] - '0' : toupper(s[i]) - 'A' + 10;
        r += (char) (a * 16 + b);
      } else {
        r += s[i];
      }
    }
    return r;
  }

};

/**
 * A FGHttpd implementation based on mongoose httpd
 *
 * Mongoose API is documented here: http://cesanta.com/docs/API.shtml
 */
class MongooseHttpd : public FGHttpd
{
public:
    /**
     * Construct a MongooseHttpd object from options in a PropertyNode
     */
    MongooseHttpd(SGPropertyNode_ptr);

    /**
     * Cleanup et.al.
     */
    ~MongooseHttpd();

    // Subsystem API.
    void bind() override;            // Currently a noop
    void init() override;            // Reads the configuration PropertyNode, installs URIHandlers and configures mongoose
    void unbind() override;          // shutdown of mongoose, clear connections, unregister URIHandlers
    void update(double dt) override; // poll connections, check for changed properties

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "mongoose-httpd"; }

    /**
     * Returns a URIHandler for the given uri
     *
     * @see URIHandlerMap::findHandler( const std::string & uri )
     */
    SGSharedPtr<URIHandler> findHandler(const std::string & uri) {
        return _uriHandler.findHandler(uri);
    }

    Websocket * newWebsocket(const string & uri);

private:
    int poll(struct mg_connection * connection);
    int request(struct mg_connection * connection);
    int onConnect(struct mg_connection * connection);
    void close(struct mg_connection * connection);

    static void staticRequestHandler(struct mg_connection *, int event, void *ev_data, void *fn_data);

    //struct mg_server *_server;
    struct {
        struct mg_mgr mgr;
        const char* addr;
        string root_dir;
        string mime_types;
        int idle_timeout_ms;
    } _server;

    struct mg_http_serve_opts _opts;

    SGPropertyNode_ptr _configNode;

    typedef int (MongooseHttpd::*handler_t)(struct mg_connection *);
    URIHandlerMap _uriHandler;

    PropertyChangeObserver _propertyChangeObserver;
};

class MongooseConnection: public Connection {
public:
  MongooseConnection(MongooseHttpd * httpd)
      : _httpd(httpd)
  {
  }

  virtual ~MongooseConnection();

  virtual void close(struct mg_connection * connection) = 0;
  virtual int poll(struct mg_connection * connection) = 0;
  virtual int request(struct mg_connection * connection) = 0;
  virtual int onConnect(struct mg_connection * connection) {return 0;}
  virtual void write(const char * data, size_t len)
  {
    if (_connection) mg_send(_connection, data, len);
  }

  static MongooseConnection * getConnection(MongooseHttpd * httpd, struct mg_connection * connection);

protected:
  void setConnection(struct mg_connection * connection)
  {
    _connection = connection;
  }
  MongooseHttpd * _httpd;
  struct ::mg_connection * _connection;

};

MongooseConnection::~MongooseConnection()
{
}

class RegularConnection: public MongooseConnection {
public:
  RegularConnection(MongooseHttpd * httpd)
      : MongooseConnection(httpd)
  {
  }
  virtual ~RegularConnection()
  {
  }

  virtual void close(struct mg_connection * connection);
  virtual int poll(struct mg_connection * connection);
  virtual int request(struct mg_connection * connection);

private:
  SGSharedPtr<URIHandler> _handler;
};

class WebsocketConnection: public MongooseConnection {
public:
  WebsocketConnection(MongooseHttpd * httpd)
      : MongooseConnection(httpd), _websocket(NULL)
  {
  }
  virtual ~WebsocketConnection()
  {
      delete _websocket;
  }
  virtual void close(struct mg_connection * connection);
  virtual int poll(struct mg_connection * connection);
  virtual int request(struct mg_connection * connection);
  virtual int onConnect(struct mg_connection * connection);

private:
  class MongooseWebsocketWriter: public WebsocketWriter {
  public:
    MongooseWebsocketWriter(struct mg_connection * connection)
        : _connection(connection)
    {
    }

    virtual int writeToWebsocket(int opcode, const char * data, size_t len)
    {
        return mg_ws_send(_connection, data, len, opcode);
          // mg_websocket_write(_connection, opcode, data, len);
    }
  private:
    struct mg_connection * _connection;
  };
  Websocket * _websocket;
};

// a pointer to MongooseConnection is stored in the associated mg_connection->fn_data
MongooseConnection * MongooseConnection::getConnection(MongooseHttpd * httpd, struct ::mg_connection * connection)
{
    MongooseConnection* c = static_cast<MongooseConnection*>(connection->fn_data);
    if (c) return c;

    if (connection->is_websocket) 
        c = new WebsocketConnection(httpd);
    else c = new RegularConnection(httpd);

    connection->fn_data = c;
    return c;
}

// Called on MG_EV_HTTP_MSG
int RegularConnection::request(struct mg_connection* connection)
{
  setConnection(connection);
  struct Mg_Message_Array* msg = (struct Mg_Message_Array*)(connection->data);
  MongooseHTTPRequest request(connection);
  SG_LOG(SG_NETWORK, SG_INFO, "RegularConnection::request for " << request.Uri);

  // find a handler for the uri and remember it for possible polls on this connection
  _handler = _httpd->findHandler(request.Uri);
  if (!_handler.valid()) {
      // uri not registered - check for websocket uri
      if ((request.Uri.find("/PropertyListener") == 0)||
          (request.Uri.find("/PropertyTreeMirror/") == 0)){

          SG_LOG(SG_NETWORK, SG_INFO, "Upgrade to WebSocket for: " << request.Uri);
          mg_ws_upgrade(connection, msg->http_msg, NULL);
          return MG_TRUE;
      } else {
          return MG_FALSE;
      }
  }

  // We handle this URI, prepare the response
  HTTPResponse response;
  response.Header["Server"] = "FlightGear/" FLIGHTGEAR_VERSION " Mongoose/" MONGOOSE_VERSION;
  response.Header["Connection"] = "keep-alive";
  response.Header["Cache-Control"] = "no-cache";
  {
    char buf[64];
    time_t now = time(NULL);
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
    response.Header["Date"] = buf;
  }

  // hand the request over to the handler, returns true if request is finished, 
  // false the handler wants to get polled again (calling handlePoll() next time)
  bool done = _handler->handleRequest(request, response, this);
  // fill in the response header
  string header;
  for (HTTPResponse::Header_t::const_iterator it = response.Header.begin(); it != response.Header.end(); ++it) {
      const string name = it->first;
      const string value = it->second;
      if (name.empty() || value.empty()) continue;
      header += (name + ": " + value + "\r\n");
  }

  if (done) {
      mg_http_reply(connection, response.StatusCode, header.c_str(), response.Content.c_str());
  } else {
      if (response.StatusCode == 200) {
          // Status OK but response.Content too long, try chunk encoding
          mg_printf(connection, "HTTP/1.1 200 OK\r\n%sTransfer-Encoding: chunked\r\n\r\n", header.c_str());
          mg_http_write_chunk(connection, response.Content.c_str(), response.Content.size());
          mg_http_printf_chunk(connection, "");
      } else {
          // just send the error code
          mg_http_reply(connection, response.StatusCode, header.c_str(), "");
      }
  }
  return done ? MG_TRUE : MG_MORE;
}

int RegularConnection::poll(struct mg_connection * connection)
{
  setConnection(connection);
  if (!_handler.valid()) return MG_FALSE;
  // only return MG_TRUE if we handle this request
  return _handler->poll(this) ? MG_TRUE : MG_MORE;
}

void RegularConnection::close(struct mg_connection * connection)
{
  setConnection(nullptr);
  // nothing to close
}

void WebsocketConnection::close(struct mg_connection * connection)
{
  setConnection(nullptr);
  if ( NULL != _websocket) _websocket->close();
  delete _websocket;
  _websocket = NULL;
}

int WebsocketConnection::poll(struct mg_connection * connection)
{
  setConnection(connection);
  // we get polled before the first request came in but we know 
  // nothing about how to handle that before we know the URI.
  // so simply ignore that poll
  if ( NULL != _websocket) {
    MongooseWebsocketWriter writer(connection);
    _websocket->poll(writer);
  }
  return MG_MORE;
}

// called on MG_EV_WS_OPEN
int WebsocketConnection::onConnect(struct mg_connection * connection)
{
  setConnection(connection);
  struct Mg_Message_Array* msg = (struct Mg_Message_Array*)(connection->data);
  MongooseHTTPRequest request(connection);
  SG_LOG(SG_NETWORK, SG_INFO, "WebsocketConnection::connect for " << request.Uri);
  if ( NULL == _websocket) _websocket = _httpd->newWebsocket(request.Uri);
  if ( NULL == _websocket) {
    SG_LOG(SG_NETWORK, SG_WARN, "httpd: unhandled websocket uri: " << request.Uri);
    return 0;
  }

  return 0;
}

// called on MG_EV_WS_MSG
int WebsocketConnection::request(struct mg_connection* connection)
{
  setConnection(connection);
  struct Mg_Message_Array* msg = (struct Mg_Message_Array*)(connection->data);

  if ((msg->ws_msg->flags & 0x0f) >= 0x8) {
    // control opcode (close/ping/pong)
    return MG_MORE;
  }

  MongooseHTTPRequest request(connection);
  SG_LOG(SG_NETWORK, SG_DEBUG, "WebsocketConnection::request for " << request.Uri);

  if ( NULL == _websocket) {
    SG_LOG(SG_NETWORK, SG_ALERT, "httpd: unhandled websocket uri: " << request.Uri);
    return MG_TRUE; // close connection - good bye
  }

  MongooseWebsocketWriter writer(connection);
  _websocket->handleRequest(request, writer);
  return MG_MORE;
}

MongooseHttpd::MongooseHttpd(SGPropertyNode_ptr configNode)
    : _configNode(configNode)
{
    _server.addr = "http://0.0.0.0:8080";
    _server.root_dir = string(".");
}

MongooseHttpd::~MongooseHttpd()
{
    //mg_destroy_server(&_server);
    mg_mgr_free(&_server.mgr);
}

void MongooseHttpd::init()
{
  SGPropertyNode_ptr n = _configNode->getNode("uri-handler");
  if (n.valid()) {
    string uri;

    if (!(uri = n->getStringValue("screenshot")).empty()) {
      SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding screenshot uri handler at " << uri);
      _uriHandler.push_back(new flightgear::http::ScreenshotUriHandler(uri));
    }

    if (!(uri = n->getStringValue("property")).empty()) {
      SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding property uri handler at " << uri);
      _uriHandler.push_back(new flightgear::http::PropertyUriHandler(uri));
    }

    if (!(uri = n->getStringValue("json")).empty()) {
      SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding json uri handler at " << uri);
      _uriHandler.push_back(new flightgear::http::JsonUriHandler(uri));
    }

    if (!(uri = n->getStringValue("pkg")).empty()) {
      SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding pkg uri handler at " << uri);
      _uriHandler.push_back(new flightgear::http::PkgUriHandler(uri));
    }

    if (!(uri = n->getStringValue("flighthistory")).empty()) {
      SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding flighthistory uri handler at " << uri);
      _uriHandler.push_back(new flightgear::http::FlightHistoryUriHandler(uri));
    }

    if (!(uri = n->getStringValue("run")).empty()) {
      SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding run uri handler at " << uri);
      _uriHandler.push_back(new flightgear::http::RunUriHandler(uri));
    }

    if (!(uri = n->getStringValue("navdb")).empty()) {
      SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding navdb uri handler at " << uri);
      _uriHandler.push_back(new flightgear::http::NavdbUriHandler(uri));
    }
  }

  mg_mgr_init(&_server.mgr);
  // save the pointer to our MongooseHttpd server wrapper
  _server.mgr.userdata = this;

  n = _configNode->getNode("options");
  if (n.valid()) {

    const string fgRoot = fgGetString("/sim/fg-root");
    string docRoot = n->getStringValue("document-root", fgRoot.c_str());
    if (docRoot[0] != '/') docRoot.insert(0, "/").insert(0, fgRoot);

    _server.addr = (string("http://0.0.0.0:") + n->getStringValue("listening-port", "8080")).c_str();
    mg_http_listen(&_server.mgr, _server.addr, MongooseHttpd::staticRequestHandler, NULL);
    {
      // build url rewrites relative to fg-root
      string rewrites = n->getStringValue("url-rewrites", "");
      string_list rwl = simgear::strutils::split(rewrites, ",");
      rwl.push_back(string("/aircraft-dir/=") + fgGetString("/sim/aircraft-dir") + "/" );
      rwl.push_back(string("/fg-home/=") + fgGetString("/sim/fg-home") + "/" );
      rwl.push_back(string("/fg-root/=") + fgGetString("/sim/fg-root") + "/" );
      rewrites.clear();
      for (string_list::iterator it = rwl.begin(); it != rwl.end(); ++it) {
        string_list rw_entries = simgear::strutils::split(*it, "=");
        if (rw_entries.size() != 2) {
          SG_LOG(SG_NETWORK, SG_WARN, "invalid entry '" << *it << "' in url-rewrites ignored.");
          continue;
        }
        string & lhs = rw_entries[0];
        string & rhs = rw_entries[1];
        if (!rewrites.empty()) rewrites.append(1, ',');
        rewrites.append(lhs).append(1, '=');
        SGPath targetPath(rhs);
        if (targetPath.isAbsolute() ) {
            rewrites.append(rhs);
        } else {
            // don't use targetPath here because SGPath strips trailing '/'
            rewrites.append(fgRoot).append(1, '/').append(rhs);
        }
      }
      _server.root_dir = docRoot + "," + rewrites;
    }
    //mg_set_option(_server, "enable_directory_listing", n->getStringValue("enable-directory-listing", "yes").c_str());
    //mg_set_option(_server, "idle_timeout_ms", n->getStringValue("idle-timeout-ms", "30000").c_str());
    _server.idle_timeout_ms = n->getIntValue("idle-timeout-ms", 30000);
    //mg_set_option(_server, "index_files", n->getStringValue("index-files", "index.html").c_str());
    //mg_set_option(_server, "extra_mime_types", n->getStringValue("extra-mime-types", "").c_str());
    _server.mime_types = n->getStringValue("extra-mime-types", "");
    //mg_set_option(_server, "access_log_file", n->getStringValue("access-log-file", "").c_str());
    memset(&_opts, 0, sizeof(struct mg_http_serve_opts));
    _opts.root_dir = _server.root_dir.c_str();
    _opts.mime_types = _server.mime_types.c_str();

    SG_LOG(SG_NETWORK, SG_INFO, "starting mongoose with these options: ");
    SG_LOG(SG_NETWORK, SG_INFO, "  > addr: '" << _server.addr << "'");
    SG_LOG(SG_NETWORK, SG_INFO, "  > root_dir: '" << _server.root_dir << "'");
    SG_LOG(SG_NETWORK, SG_INFO, "end of mongoose options.");
  }

  _configNode->setBoolValue("running",true);

}

void MongooseHttpd::bind()
{
}

void MongooseHttpd::unbind()
{
  _configNode->setBoolValue("running",false);
  //mg_destroy_server(&_server);
  mg_mgr_free(&_server.mgr);
  _uriHandler.clear();
  _propertyChangeObserver.clear();
}

void MongooseHttpd::update(double dt)
{
  _propertyChangeObserver.check();
  mg_mgr_poll(&_server.mgr, 0);
  _propertyChangeObserver.uncheck();
}

int MongooseHttpd::poll(struct mg_connection * connection)
{
  if ( NULL == connection->fn_data) return MG_FALSE; // connection not yet set up - ignore poll

  return MongooseConnection::getConnection(this, connection)->poll(connection);
}

int MongooseHttpd::request(struct mg_connection * connection)
{
    auto c = MongooseConnection::getConnection(this, connection);
    if (c == nullptr) 
        return MG_FALSE;

    if ((c->request(connection)) == MG_FALSE) {
        // no dynamic handler, serve static pages
        struct Mg_Message_Array* msg = (struct Mg_Message_Array*)(connection->data);
        mg_http_serve_dir(connection, msg->http_msg, &(_opts));
    }
    return MG_TRUE;
}

int MongooseHttpd::onConnect(struct ::mg_connection * connection)
{
  return MongooseConnection::getConnection(this, connection)->onConnect(connection);
}

void MongooseHttpd::close(struct ::mg_connection * connection)
{
  MongooseConnection * c = MongooseConnection::getConnection(this, connection);
  c->close(connection);

  // delete c -- we'll try to reuse RegularConnection
}
Websocket * MongooseHttpd::newWebsocket(const string & uri)
{
  if (uri.find("/PropertyListener") == 0) {
    SG_LOG(SG_NETWORK, SG_INFO, "new PropertyChangeWebsocket for: " << uri);
    return new PropertyChangeWebsocket(&_propertyChangeObserver);
  } else if (uri.find("/PropertyTreeMirror/") == 0) {
    const auto path = uri.substr(20);
    SG_LOG(SG_NETWORK, SG_INFO, "new MirrorPropertyTreeWebsocket for: " << path);
    return new MirrorPropertyTreeWebsocket(path);
  }
  return NULL;
}

void MongooseHttpd::staticRequestHandler(struct ::mg_connection* connection, int event, void* ev_data, void* fn_data)
{
    // the mg_mgr struct is storing the pointer on init();
    MongooseHttpd* httpd = static_cast<MongooseHttpd*>(connection->mgr->userdata);

    // the Mg_Message_Array is saved in connection->data[]
    struct Mg_Message_Array* msg = (struct Mg_Message_Array*)(connection->data);

    switch (event) {
        case MG_EV_POLL:
            // on each ITERATION, update websocket
            httpd->poll(connection);
            return;

        case MG_EV_HTTP_MSG: 
            // on each HTTP_MSG, generate response from the request
            msg->http_msg = (struct mg_http_message*)ev_data;
            httpd->request(connection);
            return;

        case MG_EV_OPEN:
            // on each EV_OPEN, a new connection would be created
            memset(connection->data, 0, sizeof(connection->data));
            connection->fn_data = NULL;
            httpd->onConnect(connection);
            return;

        case MG_EV_CLOSE: 
            // on each EV_CLOSE, close the connection
            httpd->close(connection);
            connection->fn_data = (void *)NULL;
            MG_INFO(("HTTP SERVER closed connetion on Port: %d", connection->loc.port));
            return;

        case MG_EV_ERROR: 
            // on each EV_ERROR, print the message but continue
            MG_INFO(("HTTP SERVER error: %s", (char*)ev_data));  // we don't handle errors - let mongoose do the work
            return;

        case MG_EV_WS_OPEN: 
            // on each WS_OPEN, close regular connection and establish new websocket connection
            httpd->close(connection);
            connection->fn_data = NULL;
            msg->http_msg = (struct mg_http_message*)ev_data;
            /*if (!(connection->is_websocket)) {
                MG_INFO(("Switching to websocket on Port: %d", connection->loc.port));
                connection->is_websocket = 1;
            }*/
            httpd->onConnect(connection);
            return;

        case MG_EV_WS_MSG:
            // on each WS_MSG, client message comes
            msg->ws_msg = (struct mg_ws_message*)ev_data;
            httpd->request(connection);
            return;

        default:
            return; //MG_FALSE; // keep compiler happy..
  }
}

FGHttpd * FGHttpd::createInstance(SGPropertyNode_ptr configNode)
{
// only create a server if a port has been configured
  if (!configNode.valid()) return NULL;
  string port = configNode->getStringValue("options/listening-port", "");
  if (port.empty()) return NULL;
  return new MongooseHttpd(configNode);
}


// Register the subsystem.
#if 0
SGSubsystemMgr::Registrant<MongooseHttpd> registrantMongooseHttpd;
#endif

} // namespace http
} // namespace flightgear
