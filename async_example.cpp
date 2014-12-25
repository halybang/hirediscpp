//
// g++ async_example.cpp -o async_example -I.. -lboost_program_options -lhiredis -lev
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <string>
#include <iostream>
#include <sys/types.h>
#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    //#include <err.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <event2/event.h>
#include <hiredis/async.h>
#include "hiredispp.h"
#include "hiredispp_async.h"


using namespace std;
using namespace hiredispp;

class Main
{

public:
    Main() {}

    ~Main()
    {
        if (_connected) {
            _ac.disconnect();
        }
    }

    // This callback function useful for un-interrupt command like: SUBCRIBE or MONITOR
    void onReplyDefault(RedisConnectionAsync& ac, redisReply *reply, void* privdata )
    {
        if (reply == nullptr) return;
        switch(reply->type) {
        case REDIS_REPLY_STRING:
            cout << "Redis onReplyDefault:string:" << reply->str <<endl;
            break;
        case REDIS_REPLY_INTEGER:
            cout << "Redis onReplyDefault:integer:" << reply->integer <<endl;
            break;
        case REDIS_REPLY_ARRAY:
        {
            for (int i = 0; i < reply->elements; i++) {
                redisReply *rep = reply->element[i];
                if (rep->type == REDIS_REPLY_STRING)  {
                    cout << "Redis onReplyDefault:array:string" << i << ":"  << rep->str <<endl;
                } else if (rep->type == REDIS_REPLY_INTEGER) {
                    cout << "Redis onReplyDefault:array:integer" << i << ":"  << rep->integer <<endl;
                }
            }
        }
        break;
        }
    }

    // This callback function useful for use only one response. Do not use for command like: SUBCRIBE or MONITOR, or prog will crash
    void onReply(hiredispp::RedisConnectionAsync &rc, redisReply *reply)
    {
        if (reply == nullptr) return;
        switch(reply->type) {
        case REDIS_REPLY_STRING:
            cout << "Redis onReply:string:" << reply->str <<endl;
            break;
        case REDIS_REPLY_INTEGER:
            cout << "Redis onReply:integer:" << reply->integer <<endl;
            break;
        case REDIS_REPLY_ARRAY:
        {
            for (int i = 0; i < reply->elements; i++) {
                redisReply *rep = reply->element[i];
                if (rep->type == REDIS_REPLY_STRING)  {
                    cout << "Redis onReply:array:" << i << ":string:"  << rep->str <<endl;
                } else if (rep->type == REDIS_REPLY_INTEGER) {
                    cout << "Redis onReply:array:" << i << ":integer:"  << rep->integer <<endl;
                }
            }
        }
        break;
        }
    }

    void onConnected(const RedisConnectionAsync& ac, std::shared_ptr<RedisException> &ex)
    {
        if (ex==NULL) {
            cout << "Redis onConnected: List ALL KEYS" <<endl;
            _ac.execAsyncCommand(std::bind(&Main::onReply, this, std::placeholders::_1, std::placeholders::_2),"KEYS *");
            _ac.execAsyncCommandDefault(nullptr, "SUBSCRIBE MYCHANNEL");
        } else
            cout << "Redis connect failured" <<endl;
    }

    void onDisconnected(const RedisConnectionAsync& ac, std::shared_ptr<RedisException> &ex)
    {
        cout << "Redis onDisconnected: "
             << (ex ? ex->what() : "OK") 
             << endl;
        
        _connected=false;
    }


    string _host = "127.0.0.1";
    int    _port = 6379;
    bool   _connected;
    int    _counter;
    int    _done;

    RedisConnectionAsync _ac;
    static void on_event(int fd, short ev, void *arg)
    {
        Main *srv = (Main *)arg;
        if (!srv) return;
        if (ev & EV_TIMEOUT) {
            srv->onTimeOut();
        }
        if (ev & EV_SIGNAL) {
        }
    }

    void onTimeOut()
    {
        if(_ac.getState() == hiredispp::redisAsyncState::redisAsyncStateDisconnected) {
            cout << "Redis trying connect to " << _host << ":" << _port <<endl;
            _ac.connect(std::bind(&Main::onConnected, this, std::placeholders::_1, std::placeholders::_2),
                              std::bind(&Main::onDisconnected, this, std::placeholders::_1, std::placeholders::_2),
                              std::bind(&Main::onReplyDefault, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
                              );
        }
    }

    void run()
    {
        struct event_base *evbase;
        evbase = event_base_new();
        struct timeval tvi;
        tvi.tv_sec = 2;
        tvi.tv_usec = 0;

        struct event* ev_timer = event_new(evbase, -1, EV_TIMEOUT | EV_PERSIST, on_event, reinterpret_cast<void *>(this));
        if (event_add(ev_timer, &tvi) == -1) {
            return ;
        }
        _ac.setHost(_host, _port);
        _ac.setLoopBase(evbase);

        /* Start the event loop. */
        event_base_dispatch(evbase);
    }
};

namespace std {
    std::ostream& operator<< (std::ostream& out, const std::pair<std::string, std::string> &p) {
        return out << p.first << ":" << p.second;
    }
}

/*
* After run program, open redis-cli and repeat enter command:
* PUBLISH MYCHANNEL DATA_NEED_TO_PUB_TO_MYCHANNEL
* then see result in example console.
*/

int main(int argc, char** argv)
{

#if _WIN32
    WSADATA wsa_data;
    WSAStartup(0x0201, &wsa_data);
#endif

#if HIREDISPP_USE_LIBEVENT
    Main main;
    main.run();
#endif

    return 0;
}
