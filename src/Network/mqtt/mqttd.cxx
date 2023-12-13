// mqttd.cxx -- a mqtt daemon subsystem based on Mongoose mqtt
//
// Written by xusy@zhftc.com, started April 2023.
//
// Copyright (C) 2023  xusy@zhftc.com
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

#include "mqttd.hxx"
#include "PropertyChangeObserver.hxx"
#include <Main/fg_props.hxx>

#include <mongoose.h>

#include <string>
#include <vector>
#include <map>

#ifdef MG_VERSION
#define MONGOOSE_VERSION MG_VERSION
#undef poll
#endif

using std::string;
using std::vector;
using std::map;

namespace flightgear {
namespace mqtt {

const char * PROPERTY_ROOT = "/sim/mqtt";

class MongooseMQTTConnection {
public:
    struct mg_mgr mgr;
    string addr;
    string sub_topic;
    string pub_topic;
    uint8_t pub_qos;    // Outbound QoS, default to 1
    uint8_t sub_qos;    // Inbound QoS, default to 0
    int timeout_ms;
    struct mg_connection* conn;

    map<string, struct mg_str> updateList;


    MongooseMQTTConnection() : addr(""),
                               sub_topic("mg/+/test"),
                               pub_topic("mg/clnt/test"),
                               sub_qos(0),
                               pub_qos(1),
                               timeout_ms(3000),
                               conn(nullptr)
    {
        mg_mgr_init(&mgr);
    }

    MongooseMQTTConnection(string url) : addr(url),
                                         sub_topic("mg/+/test"),
                                         pub_topic("mg/clnt/test"),
                                         sub_qos(0),
                                         pub_qos(1),
                                         timeout_ms(3000),
                                         conn(nullptr)
    {
        mg_mgr_init(&mgr);
    }

    ~MongooseMQTTConnection() {
        mg_mgr_free(&mgr);
    }

    int establishConnection(string& url, int timeout_ms);
    int subscribeUpdate(SGPropertyNode_ptr node, struct mg_str data);
    int publishUpdate(SGPropertyNode_ptr node);

private:
    static void staticRequestHandler(struct mg_connection*, int event, void* ev_data, void* fn_data);
    // Timer function - recreate client connection if it is closed
    static void staticReconnectTimer(void* arg);

    typedef  union {
        bool vBOOL;
        int vINT;
        long vLONG;
        float vFLOAT;
        double vDOUBLE;
    } u_val_t;
};

int MongooseMQTTConnection::establishConnection(string& url, int timeout)
{
    addr = url;
    timeout_ms = timeout;

    mg_timer_add(&mgr, timeout_ms,
                 MG_TIMER_REPEAT | MG_TIMER_RUN_NOW,
                 staticReconnectTimer, this);

    return 1;
}

int MongooseMQTTConnection::subscribeUpdate(SGPropertyNode_ptr node, struct mg_str data)
{
    u_val_t* val = (u_val_t*)data.ptr;
    string path = node->getPath(true);

    switch (node->getType()) {
    case simgear::props::BOOL: {
        bool ba = val->vBOOL;
        SG_LOG(SG_NETWORK, SG_INFO, "RECEIVED BOOL val:" << path << " -> " << ba);
        // To make sure LSB is used
        return node->setBoolValue(0xff & (val->vBOOL));
    }
    case simgear::props::INT:
        return node->setIntValue(val->vINT);

    case simgear::props::LONG:
        return node->setLongValue(val->vLONG);
        
    case simgear::props::FLOAT:
        return node->setFloatValue(val->vFLOAT);
 
    case simgear::props::DOUBLE: 
        return node->setDoubleValue(val->vDOUBLE);
 
    default:
        return node->setStringValue(data.ptr);
 
    }
 
}

int MongooseMQTTConnection::publishUpdate(SGPropertyNode_ptr node)
{
    static struct mg_str data;
    u_val_t val;
    string strV;

    switch (node->getType()) {
    case simgear::props::BOOL:
        data.ptr = (char *)&val;
        data.len = sizeof(bool);
        val.vBOOL = node->getBoolValue();
        break;
    case simgear::props::INT:
        data.ptr = (char*)&val;
        data.len = sizeof(int);
        val.vINT = node->getIntValue();
        break;
    case simgear::props::LONG:
        data.ptr = (char*)&val;
        data.len = sizeof(long);
        val.vLONG = node->getLongValue();
        break;
    case simgear::props::FLOAT:
        data.ptr = (char*)&val;
        data.len = sizeof(float);
        val.vFLOAT = node->getFloatValue();
        break;
    case simgear::props::DOUBLE: {
        data.ptr = (char*)&val;
        data.len = sizeof(double);
        val.vDOUBLE = node->getDoubleValue();
        break;
    }
    default:
        strV = node->getStringValue();
        data.ptr = strV.c_str();
        data.len = strV.size();
        break;
    }

    if (conn != NULL) {
        string topic = node->getPath(true);

        struct mg_mqtt_opts pub_opts = {0};
        pub_opts.topic = mg_str(topic.c_str());
        pub_opts.message = data;
        pub_opts.qos = pub_qos;
        pub_opts.retain = false;
        mg_mqtt_pub(conn, &pub_opts);

        //SG_LOG(SG_NETWORK, SG_INFO, "MQTT connection PUBLISHED " << data.ptr << " to " << topic);

        return data.len;
    } else {
        return 0;
    }
}


void MongooseMQTTConnection::staticRequestHandler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    MongooseMQTTConnection* p_conn = (MongooseMQTTConnection*)fn_data;
    const char* s_url = p_conn->addr.c_str();
    const char* s_sub_topic = p_conn->sub_topic.c_str();
    const char* s_pub_topic = p_conn->pub_topic.c_str();
    int s_qos = p_conn->sub_qos;
    int p_qos = p_conn->pub_qos;
    struct mg_connection **s_conn = &(p_conn->conn);

  if (ev == MG_EV_OPEN) {
    SG_LOG(SG_NETWORK, SG_INFO, "MQTT(TCP) connection CREATED!");
    // mg_mqtt_login() would send MQTT CONNECT request by mongoose.c itself.
  } else if (ev == MG_EV_ERROR) {
    // On error, log error message
    MG_ERROR(("%p %s", c->fd, (char *) ev_data));
  } else if (ev == MG_EV_CONNECT) {
    // If target URL is SSL/TLS, command client connection to use TLS
    if (mg_url_is_ssl(s_url)) {
      struct mg_tls_opts opts = {0};
      opts.ca = mg_str("ca.pem");
      opts.name = mg_url_host(s_url);
      mg_tls_init(c, &opts);
    }
  } else if (ev == MG_EV_MQTT_OPEN) {
    // MQTT connect is successful
    struct mg_str subt = mg_str(s_sub_topic);
    struct mg_str pubt = mg_str(s_pub_topic), data = mg_str("hello");
    SG_LOG(SG_NETWORK, SG_INFO, "MQTT connection CONNECTED to " << s_url);

    struct mg_mqtt_opts sub_opts = {0};
    sub_opts.topic = subt;
    sub_opts.qos = s_qos;
    mg_mqtt_sub(c, &sub_opts);
    SG_LOG(SG_NETWORK, SG_INFO, "MQTT connection SUBSCRIBING to " << subt.ptr);
    // TODO: Note that Mongoose does not handle retries for QoS 1 and 2. 
    // That has to be handled by the application in the event handler, if needed.

    struct mg_mqtt_opts pub_opts = {0};
    pub_opts.topic = mg_str(s_pub_topic);
    pub_opts.message = mg_str("hello");
    pub_opts.qos = p_qos;
    pub_opts.retain = false;
    mg_mqtt_pub(c, &pub_opts);

    SG_LOG(SG_NETWORK, SG_INFO, "MQTT connection PUBLISHED " <<data.ptr << " to " << pubt.ptr);
  } else if (ev == MG_EV_MQTT_MSG) {
    // When we get a MQTT message, append to updateList
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    // SG_LOG(SG_NETWORK, SG_INFO, "MQTT connection RECEIVED " << mm->data.ptr << " from " << mm->topic.ptr);

    p_conn->updateList.insert_or_assign(string(mm->topic.ptr, mm->topic.len), mg_strdup(mm->data));

    // to close the connection
    //c->is_closing = 1;  
  } else if (ev == MG_EV_CLOSE) {
    SG_LOG(SG_NETWORK, SG_INFO, "MQTT connection CLOSED.");
    *s_conn = NULL;  // Mark that we're closed
  }

}

// Timer function - recreate client connection if it is closed
void MongooseMQTTConnection::staticReconnectTimer(void* arg)
{
    MongooseMQTTConnection* p_conn = (MongooseMQTTConnection*)arg;

    struct mg_mgr* mgr = &(p_conn->mgr);
    const char* s_url = p_conn->addr.c_str();
    const char* s_sub_topic = p_conn->sub_topic.c_str();
    const char* s_pub_topic = p_conn->pub_topic.c_str();
    int s_qos = p_conn->pub_qos;
    struct mg_connection **s_conn = &(p_conn->conn);

    if (*s_conn == NULL) {
        struct mg_mqtt_opts opts = {0};
        opts.clean = true;      // clean session
        opts.qos = s_qos;
        opts.topic = mg_str(s_pub_topic);
        opts.version = 4;
        opts.message = mg_str("goodbye");

        *s_conn = mg_mqtt_connect(mgr, s_url, &opts, staticRequestHandler, arg);
        /* This function does not connect to a broker; 
           it allocates the required resources and starts the TCP connection process. 
           Once that connection is established, an MG_EV_CONNECT event is sent to the connection event handler, 
           then the MQTT connection process is started (by means of mg_mqtt_login()).
           mg_mqtt_login() is usually called by mg_mqtt_connect(), 
           you will only need to call it when you manually start the MQTT connect process, 
           e.g: when using MQTT over WebSocket. 
         */
    }
}


/**
 * A FGMqttd implementation based on mongoose mqttd
 *
 * Mongoose API is documented here: mqtt://cesanta.com/docs/API.shtml
 */
class MongooseMqttd : public FGMqttd
{
public:
    /**
     * Construct a MongooseMqttd object from options in a PropertyNode
     */
    MongooseMqttd(SGPropertyNode_ptr);

    /**
     * Cleanup et.al.
     */
    ~MongooseMqttd();

    // Subsystem API.
    void bind() override;            // Currently a noop
    void init() override;            // Reads the configuration PropertyNode, installs URIHandlers and configures mongoose
    void unbind() override;          // shutdown of mongoose, clear connections, unregister URIHandlers
    void update(double dt) override; // poll connections, check for changed properties

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "mongoose-mqttd"; }

private:

    MongooseMQTTConnection _conn;

    SGPropertyNode_ptr _configNode;
    
    PropertyChangeObserver _propertyChangeObserver;

    class WatchedNodesList : public std::vector<SGPropertyNode_ptr>
    {
    public:
        int addWatchedNode(const std::string& node, PropertyChangeObserver* propertyChangeObserver);
        int removeWatchedNode(const std::string& node, PropertyChangeObserver* propertyChangeObserver);
    };

    WatchedNodesList _watchedNodes;
};

MongooseMqttd::MongooseMqttd(SGPropertyNode_ptr configNode)
    : _configNode(configNode)
{
}

MongooseMqttd::~MongooseMqttd()
{
    //mg_mgr_free(&_conn.mgr);
}

void MongooseMqttd::init()
{
  SGPropertyNode_ptr n;

  // save the pointer to our MongooseMqttd server wrapper
  _conn.mgr.userdata = this;

  n = _configNode->getNode("options");
  if (n.valid()) {

    const string fgRoot = fgGetString("/sim/fg-root");
    string addr = n->getStringValue("url", "mqtt://broker.hivemq.com:1883");
    string topic = n->getStringValue("watched-list", "/network/mqtt");
    int timeout = n->getIntValue("retry-timeout-ms", 30000);

    SG_LOG(SG_NETWORK, SG_INFO, "starting mqtt connection with these options: ");
    SG_LOG(SG_NETWORK, SG_INFO, "  > addr: '" << addr << "'");
    SG_LOG(SG_NETWORK, SG_INFO, "  > interested-topic: '" << topic << "'");
    SG_LOG(SG_NETWORK, SG_INFO, "  > retry-timeout: '" << timeout << "'");
    SG_LOG(SG_NETWORK, SG_INFO, "end of mqtt options.");
    
    _conn.sub_topic = topic + "/#";
    _propertyChangeObserver.clear();
    _watchedNodes.addWatchedNode(topic, &_propertyChangeObserver);

    if (_conn.establishConnection(addr, timeout)) 
    {
        _configNode->setBoolValue("running", true);
    }
  }
}

void MongooseMqttd::bind()
{
}

void MongooseMqttd::unbind()
{
  _configNode->setBoolValue("running",false);
  //mg_mgr_free(&_conn.mgr);
  _propertyChangeObserver.clear();
  _watchedNodes.clear();
}

void MongooseMqttd::update(double dt)
{
    string path, data;

    _propertyChangeObserver.check();

    for (WatchedNodesList::iterator it = _watchedNodes.begin(); it != _watchedNodes.end(); ++it) {
        SGPropertyNode_ptr node = *it;

        if (_propertyChangeObserver.isChangedValue(node)) {
            path = node->getPath(true);
            data = node->getStringValue();
            
            SG_LOG(SG_NETWORK, SG_INFO, "mqttd: new Local Value for " << path << ": " << data);
            _conn.publishUpdate(node);
        }
    }

    mg_mgr_poll(&_conn.mgr, 0);

    for (WatchedNodesList::iterator it = _watchedNodes.begin(); it != _watchedNodes.end(); ++it) {
        SGPropertyNode_ptr node = *it;
        // Skip to update the node we've just published
        if (!_propertyChangeObserver.isChangedValue(node)) 
        {
            path = node->getPath(true);

            auto newData = _conn.updateList.find(path);

            if (newData != _conn.updateList.end()) {
                //node->setStringValue(newData->second);
                _conn.subscribeUpdate(node, newData->second);
                free((void*)newData->second.ptr);
            }
        }
    }
    _conn.updateList.clear();

    _propertyChangeObserver.check();   // To flush updates by remote,
    _propertyChangeObserver.uncheck(); // So we don't resend the updates on next iteration.
}

FGMqttd * FGMqttd::createInstance(SGPropertyNode_ptr configNode)
{
// only create a server if a port has been configured
  if (!configNode.valid()) return NULL;
  string port = configNode->getStringValue("options/listening-port", "");
  if (port.empty()) return NULL;
  return new MongooseMqttd(configNode);
}

int MongooseMqttd::WatchedNodesList::addWatchedNode(const std::string& node, PropertyChangeObserver* propertyChangeObserver)
{
    for (iterator it = begin(); it != end(); ++it) {
        if (node == (*it)->getPath(true)) {
            SG_LOG(SG_NETWORK, SG_WARN, "mqttd: addWatchedNode (" << node << ") ignored (duplicate)");
            return 0; // duplicated
        }
    }
    SGPropertyNode *ls = fgGetNode(node, true);
    int nCh = ls->nChildren();
    if (ls->hasValue()) {
        SGPropertyNode_ptr n = propertyChangeObserver->addObservation(node);
        if (n.valid()) push_back(n);
        string strAct = "";
        auto act = ls->getNode("_attr_/active");
        if (act)
            strAct = act->getStringValue();

        SG_LOG(SG_NETWORK, SG_INFO, "mqttd: addWatchedNode (" << node << ") success. sigACT=" << strAct);

        return 1;
    } else if (nCh > 0) {
        for (int i = 0; i < nCh; i++) {
            addWatchedNode(ls->getChild(i)->getPath(), propertyChangeObserver);
        }
        return nCh;
    }
}

int MongooseMqttd::WatchedNodesList::removeWatchedNode(const std::string& node, PropertyChangeObserver* propertyChangeObserver)
{
    for (iterator it = begin(); it != end(); ++it) {
        if (node == (*it)->getPath(true)) {
            this->erase(it);
            SG_LOG(SG_NETWORK, SG_INFO, "mqttd: removeWatchedNode (" << node << ") success");
            
            return 1;
        }
    }
    SG_LOG(SG_NETWORK, SG_WARN, "mqttd: removeWatchedNode (" << node << ") ignored (not found)");

    return 0;
}

} // namespace mqtt
} // namespace flightgear
