#ifndef _HiredisppAsync_H_
#define _HiredisppAsync_H_
#include <memory>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

#define HIREDISPP_USE_LIBEVENT  1

//#include <boost/function.hpp>
//#include <boost/shared_ptr.hpp>

namespace hiredispp
{
    enum redisAsyncState
    {
        redisAsyncStateDisconnected,
        redisAsyncStateConnecting,
        redisAsyncStateConnected
    };

    class RedisConnectionAsync
    {
    public:
        RedisConnectionAsync()
            : _ac(NULL), _host("127.0.0.1"), _port(6379), _reconnect(true),_redisState(redisAsyncStateDisconnected)
        {}
        RedisConnectionAsync(const std::string& host, int port)
            : _ac(NULL), _host(host), _port(port), _reconnect(true),_redisState(redisAsyncStateDisconnected)
        {}
        void setHost(const std::string& host, int port)
        {
            _host = host;
            _port = port;
        }
        void setLoopBase(void *loopbase)
        {
            _loopbase = loopbase;
        }

        redisAsyncState getState() {return _redisState;}

        template<typename HandlerC, typename HandlerD, typename HandlerR>
        void connect(HandlerC handlerC, HandlerD handlerD, HandlerR handlerR)
        {
            if (asyncConnect() == 0) {
                _redisState = redisAsyncStateConnecting;
                _onConnected = createOnHandler(handlerC);
                _onDisconnected = createOnHandler(handlerD);
                _onReply = createOnExeHandler(handlerR);
            }
        }
        
        void disconnect()
        {
            if (_ac) {
                redisAsyncDisconnect(_ac);
                _ac=NULL;
                if (_onConnected) delete _onConnected;
                if (_onDisconnected) delete _onDisconnected;
                if (_onReply)   delete _onReply;

                _onConnected = nullptr;
                _onDisconnected = nullptr;
                _onReply = nullptr;
            }
            _redisState = redisAsyncStateDisconnected;
        }

        // Origin function
        template<typename CharT, typename ExecHandler>
        void execAsyncCommand(const RedisCommandBase<CharT> & cmd, ExecHandler handler)
        {
            if (_ac==NULL || _ac->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING))
                throw RedisException("Can't execute a command, disconnecting or freeing");

            const int sz=cmd.size();
            const char* argv[sz];
            size_t argvlen[sz];
            
            for (size_t i = 0; i < sz; ++i) {
                argv[i] = cmd[i].data();
                argvlen[i] = cmd[i].size();
            }
            
            Handler<ExecHandler> *hand=new Handler<ExecHandler>(handler);
            int result = 
                ::redisAsyncCommandArgv(_ac, Handler<ExecHandler>::callback, hand,
                                        sz, argv, argvlen);

            if (result == REDIS_ERR) {
                delete hand;
                throw RedisException("Can't execute a command, REDIS ERROR");
            }
        }

        // Use null callback function
        int execAsyncCommand(const char *format, ...)
        {
            if (_ac==NULL || _ac->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING)) {
                return -1;
                // throw RedisException("Can't execute a command, disconnecting or freeing");
            }
            va_list ap;
            va_start(ap,format);
            int result = redisvAsyncCommand(_ac, nullptr, nullptr, format, ap);
            va_end(ap);
            if (result == REDIS_ERR) {
                // throw RedisException("Can't execute a command, REDIS ERROR");
            }
            return result;
        }

        // Use null callback function
        int execvAsyncCommand(const char *format, va_list ap)
        {
            if (_ac==NULL || _ac->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING)) {
                return -1;
                // throw RedisException("Can't execute a command, disconnecting or freeing");
            }
            int result = redisvAsyncCommand(_ac, nullptr, nullptr, format, ap);
            if (result == REDIS_ERR) {
                // throw RedisException("Can't execute a command, REDIS ERROR");
            }
            return result;
        }

        // Functor will be delete
        template<typename ExecHandler>
        int execAsyncCommand(ExecHandler handler, const char *format, ...)
        {
            if (_ac==NULL || _ac->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING)) {
                return -1;
                // throw RedisException("Can't execute a command, disconnecting or freeing");
            }

            Handler<ExecHandler> *hand=new Handler<ExecHandler>(handler);

            va_list ap;
            va_start(ap,format);
            int result = redisvAsyncCommand(_ac, Handler<ExecHandler>::callback, hand, format, ap);
            va_end(ap);
            if (result == REDIS_ERR) {
                delete hand;
                throw RedisException("Can't execute a command, REDIS ERROR");
            }
            return result;
        }

        // Functor will be delete
        template<typename ExecHandler>
        int execvAsyncCommand(ExecHandler handler, const char *format, va_list ap)
        {
            if (_ac==NULL || _ac->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING)) {
                return -1;
                // throw RedisException("Can't execute a command, disconnecting or freeing");
            }

            Handler<ExecHandler> *hand=new Handler<ExecHandler>(handler);

            int result = redisvAsyncCommand(_ac, Handler<ExecHandler>::callback, hand, format, ap);
            if (result == REDIS_ERR) {
                delete hand;
                throw RedisException("Can't execute a command, REDIS ERROR");
            }
            return result;
        }

        // Using default functor
        int execAsyncCommandDefault(void* privdata, const char *format, ...)
        {
            if (_ac==NULL || _ac->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING)) {
                return -1;
                // throw RedisException("Can't execute a command, disconnecting or freeing");
            }
            va_list ap;
            va_start(ap,format);
            int result = ::redisvAsyncCommand(_ac, replied, privdata, format, ap);
            va_end(ap);
            if (result == REDIS_ERR) {
                //throw RedisException("Can't execute a command, REDIS ERROR");
            }
            return result;
        }

        // Using default functor
        int execvAsyncCommandDefault(void* privdata, const char *format, va_list ap)
        {
            if (_ac==NULL || _ac->c.flags & (REDIS_DISCONNECTING | REDIS_FREEING)) {
                return -1;
                // throw RedisException("Can't execute a command, disconnecting or freeing");
            }
            int result = ::redisvAsyncCommand(_ac, replied, privdata, format, ap);
            if (result == REDIS_ERR) {
                //throw RedisException("Can't execute a command, REDIS ERROR");
            }
            return result;
        }

    private:
        typedef RedisConnectionAsync ThisType;

        class BaseOnHandler
        {
        public:
            virtual void operator() (const RedisConnectionAsync& rc, std::shared_ptr<RedisException> &ex)=0;
            virtual ~BaseOnHandler() {}
        };

        class BaseOnExeHandler
        {
        public:
            //virtual void operator() (RedisConnectionAsync& rc, Redis::Element *reply, void* privdata)=0;
            virtual void operator() (RedisConnectionAsync& rc, redisReply *reply, void* privdata)=0;
            virtual ~BaseOnExeHandler() {}
        };

        template<typename Handler>
        class OnHandler : public BaseOnHandler
        {
            Handler _handler;
        public:
            OnHandler(Handler handler) : _handler(handler) {}
            virtual void operator() (const RedisConnectionAsync& rc, std::shared_ptr<RedisException> &ex) {
                _handler(rc, ex);
            }
        };
        
        template<typename ExeHandler>
        class OnExeHandler : public BaseOnExeHandler
        {
            ExeHandler _handler;
        public:
            OnExeHandler(ExeHandler handler) : _handler(handler) {}
            virtual void operator() (RedisConnectionAsync& ac, redisReply *reply, void* privdata) {
                _handler(ac, reply, privdata);
            }
        };

#if 0
        template<typename HandlerT>
        typename std::auto_ptr< BaseOnHandler > createOnHandler(HandlerT handler)
        {
            return std::auto_ptr< BaseOnHandler >(new OnHandler<HandlerT>(handler));
        }
#else
        template<typename HandlerT>
        BaseOnHandler* createOnHandler(HandlerT handler)
        {
            return (new OnHandler<HandlerT>(handler));
        }

        template<typename HandlerT>
        BaseOnExeHandler* createOnExeHandler(HandlerT handler)
        {
            return (new OnExeHandler<HandlerT>(handler));
        }

#endif

        template<typename Callback>
        class Handler // : public enable_shared_from_this<Handler <Callback> >
        {
        public:
            Handler(Callback c) : _c(c) {}

            static void callback(redisAsyncContext *c, void *reply, void *privdata)
            {
                (static_cast< Handler<Callback> * > (privdata)) -> operator() (c,reply);
            }

            void operator() (redisAsyncContext *c, void *reply)
            {
                if (reply) {
                    // not a Redis::Reply, to avoid re-release of reply
                    // Redis::Element replyPtr(static_cast<redisReply*>(reply));
                    // _c(*static_cast<ThisType*>(c->data), &replyPtr);
                    _c(*static_cast<ThisType*>(c->data), static_cast<redisReply*>(reply));
                }
                else {
                    // _c(*static_cast<ThisType*>(c->data), static_cast<Redis::Element*>(NULL));
                    _c(*static_cast<ThisType*>(c->data), static_cast<redisReply*>(NULL));
                }
                delete(this);
            }

        private:
            Callback _c;
        };
        
        void onConnected(int status)
        {
            std::shared_ptr<RedisException> ex;
            if (status!=REDIS_OK) {
                ex.reset(new RedisException((_ac && _ac->errstr) ? _ac->errstr : "REDIS_ERR"));
                redisAsyncDisconnect(_ac);
                asyncClose(); // we started ev_read/write, we must stop it.
                _ac=NULL;
                _redisState = redisAsyncStateDisconnected;
            }
            _onConnected->operator()(*static_cast<ThisType*>(this), ex);
        }
        
        void onDisconnected(int status)
        {
            std::shared_ptr<RedisException> ex;
            if (status!=REDIS_OK) {
                ex.reset(new RedisException((_ac && _ac->errstr) ? _ac->errstr : "REDIS_ERR"));
                _ac=NULL;
            }
            _redisState = redisAsyncStateDisconnected;
            _onDisconnected->operator()(*static_cast<ThisType*>(this), ex);
        }

        void onReply(void* reply, void* privdata)
        {
            // Redis::Element replyPtr(static_cast<redisReply*>(reply));
            // _onReply->operator()(*static_cast<ThisType*>(this), &replyPtr, privdata);
            _onReply->operator()(*static_cast<ThisType*>(this), static_cast<redisReply*>(reply), privdata);
        }

        static void connected(const redisAsyncContext *ac, int status)
        {
            if (ac && ac->data) {
                ((RedisConnectionAsync*)(ac->data))->onConnected(status);
            }
        }
        
        static void disconnected(const redisAsyncContext *ac, int status)
        {
            if (ac && ac->data) {
                ((RedisConnectionAsync*)(ac->data))->onDisconnected(status);
            }
        }

        static void replied(struct redisAsyncContext* ac, void* reply, void* privdata)
        {
            if (ac && ac->data) {
                ((RedisConnectionAsync*)(ac->data))->onReply(reply, privdata);
            }
        }

        std::string        _host;
        uint16_t           _port;
        bool               _reconnect;
        void *_loopbase;
        redisAsyncContext* _ac = nullptr;

        //struct event_base *ev_base;

//        std::auto_ptr<BaseOnHandler>   _onConnected;
//        std::auto_ptr<BaseOnHandler>   _onDisconnected;

        BaseOnHandler*      _onConnected  = nullptr;
        BaseOnHandler*      _onDisconnected  = nullptr;
        BaseOnExeHandler*   _onReply  = nullptr;
        redisAsyncState     _redisState;
        int asyncConnect();
        void asyncClose()
        {
            if (_ac) {
#if HIREDISPP_USE_LIBEVENT
#else
                ev_io_stop(EV_DEFAULT, &((((redisLibevEvents*)(_ac->ev.data)))->rev));
                ev_io_stop(EV_DEFAULT, &((((redisLibevEvents*)(_ac->ev.data)))->wev));
                redisLibevCleanup(_ac->_adapter_data);
#endif
                //redisLibeventCleanup(_ac->ev.data);
                // TODO: should we close it or not???
                // close(_ac->c.fd);
                //_ac = nullptr;
            }
        }
    };

}
#endif
