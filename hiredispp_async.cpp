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

//#define _EVENT_HAVE_PTHREADS
#include <event2/event.h>

#include <hiredis/hiredis.h>
#include "hiredispp.h"
#include "hiredispp_async.h"
#if HIREDISPP_USE_LIBEVENT
    #include <hiredis/adapters/libevent.h>
#else
    #include <hiredis/adapters/libev.h>
#endif
/*
 I don't like to use exception, so I comment out
*/
namespace hiredispp
{

int RedisConnectionAsync::asyncConnect()
{
    _ac = redisAsyncConnect(_host.c_str(), _port);
    if (_ac == nullptr) return -1;
    _ac->data = (void*)this;

    if (_ac->err) {
        redisAsyncFree(_ac);
        _ac = nullptr;
        return -1;
        // throw RedisException((std::string)"RedisAsyncConnect: "+_ac->errstr);
    }
    if (redisAsyncSetConnectCallback(_ac, &connected)!=REDIS_OK ||
        redisAsyncSetDisconnectCallback(_ac, &disconnected)!=REDIS_OK) {
        redisAsyncFree(_ac);
        _ac = nullptr;
        return -1;
        // throw RedisException("RedisAsyncConnect: Can't register callbacks");
    }

#if HIREDISPP_USE_LIBEVENT
    if (redisLibeventAttach(_ac, (struct event_base *)_loopbase)!=REDIS_OK) {
        redisAsyncFree(_ac);
        _ac = nullptr;
        return -1;
        // throw RedisException("redisLibeventAttach: nothing should be attached when something is already attached");
    }
    // Send PING command, so libevent start working
    redisAsyncCommand(_ac, nullptr, nullptr, "PING");
#else
    // actually start io proccess
    ev_io_start(EV_DEFAULT, &((((redisLibevEvents*)(_ac->ev.data)))->rev));
    ev_io_start(EV_DEFAULT, &((((redisLibevEvents*)(_ac->ev.data)))->wev));
#endif
    return 0;
}


}
