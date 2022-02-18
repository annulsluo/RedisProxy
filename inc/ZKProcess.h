
#pragma once
#include "CommInc.h"
#include "util/myboost_log.h"

#include "zookeeper/zookeeper.h"

#define ZK_DEFAULT_RECVTIMEOUT 3000
#define CONNECT_WAIT_TIME 1000
#define ZK_RPATH_LEN 4096

using std::string;
using std::vector;

namespace aiad {

/*
 struct Stat {
 int64_t czxid;
 int64_t mzxid;
 int64_t ctime;
 int64_t mtime;
 int32_t version;
 int32_t cversion;
 int32_t aversion;
 int64_t ephemeralOwner;
 int32_t dataLength;
 int32_t numChildren;
 int64_t pzxid;
 };
 */

const char *ErrorStr(int code);
const char *EventStr(int event);
const char *StateStr(int state);
std::string GetLastErrorMsg(int err);
std::string ParentPath( const std::string &path );

// Zookeeper watch path data callback function
typedef void (*DataWatcherCallback)(const std::string &path,
                                    const std::string &value, void *ctx,
                                    const struct Stat *stat);

// Zookeeper watch path children callback function
typedef void (*ChildrenWatcherCallback)(const std::string &path,
                                        const std::vector<std::string> &value,
                                        void *ctx, const struct Stat *stat);

// user callback function, call when zookeeper session status change
typedef void (*UserWatcherCallback)(int type, int state, const char *path,
                                    void *data);

// Zookeeper return code class
class ZKRet {
  friend class ZKProcess;

 public:
  ZKRet(int ret) { _code = ret; }

  // Zookeeper process success
  bool Ok(void) const { return ZOK == _code; }

  // Zookeeper path exists
  bool NodeExists(void) const { return ZNODEEXISTS == _code; }

  // Zookeeper path not exists
  bool NodeNotExists(void) const { return ZNONODE == _code; }

  // Check process result
  operator bool() { return Ok(); }

  int getCode() { return _code; }

 protected:
  explicit ZKRet(void) { _code = ZOK; }

 private:
  int _code;
};

class ZKProcess {
 public:
  ZKProcess();

  ~ZKProcess();

  // zkConnect to zookeeper
  // return true not mean had connected to zookeeper,
  // just success send connect request to zookeeper
  // call isConnected check whether connected to zookeeper
  bool zkConnect(const string &hosts,
                 const uint32_t recv_time = ZK_DEFAULT_RECVTIMEOUT, 
				 const string sRootPath = "",
                 FILE *log_stream = stderr,
                 UserWatcherCallback user_func = NULL, void *user_ctx = NULL);
  bool doConnect();

  // close connection to zookeeper
  void disConnect(void);

  // Create path in zookeeper
  // parm: recursive, if true, create recursive path in zookeeper
  ZKRet createNode(const string &path, const string &value, bool recursive);
  ZKRet deleteNode(const string &path, int version);

  // Create ZOO_EPHEMERAL and ZOO_SEQUENCE path
  ZKRet createEphAndSeqNode(const string &path, const string &value,
                            string &rpath);
  ZKRet createEph(const string &path, const string &value, string &rpath);

  // Get path value
  bool getData(const string &path, string &value, struct Stat *stat = NULL);

  // Set path value
  bool setData(const string &path, const string &value,
               struct Stat *stat = NULL);

  // Check path exists
  bool exists(const string &path, struct Stat *stat = NULL);

  // Get all childrens path
  bool getChildren(const string &path, std::vector<string> &children,
                   struct Stat *stat = NULL);

  // watch path value, if change, will callback
  bool watchData(const string &path, DataWatcherCallback cb, void *cb_ctx);

  bool delWatchData(const string &path);

  // wathc children, if change, will callback
  bool watchChildren(const string &path, ChildrenWatcherCallback cb,
                     void *cb_ctx);

  // Open zookeeper debug log
  void setDebugLogLevel(bool open = true);

  // synchronous data
  bool syncPath(const string &path);

  // Set zookeeper log level
  inline void setDefaultLogLevel(ZooLogLevel level) {
    this->_default_log_level = level;
  }

  // Get zookeeper hosts
  inline string getHosts(void) { return _hosts; }

  // Check whether had connected to zookeeper
  bool isConnected(void);

  // Reconnect to zookeeper
  bool restart(void);

  // if zookeeper state change will call user func
  inline void setUserFuncAndCtx(UserWatcherCallback user_func, void *user_ctx) {
    this->_user_func = user_func;
    this->_user_ctx = user_ctx;
  }

  // get zookeeper session expired timeout time
  inline int getRecvTimeout(void) {
    return _connected ? zoo_recv_timeout(_zk_handle) : -1;
  }

 private:
  // Call the _user_func function
  void callUserCallback(int type, int state, const char *path) const {
    if (NULL != _user_func) _user_func(type, state, path, _user_ctx);
  }

  // watch zookeeper event
  static void defaultWatcher(zhandle_t *handle, int type, int state,
                             const char *path, void *watcherCtx);

  // use in watch data
  static void dataCompletion(int rc, const char *value, int valuelen,
                             const struct Stat *stat, const void *data);

  // use in watch children
  static void childrenCompletion(int rc, const struct String_vector *strings,
                                 const struct Stat *stat, const void *data);

  // use in sync data
  static void syncCompletion(int rc, const char *value, const void *data);

  // when recv connect response, set had connected zookeeper
  inline void setConnected(bool connect) { this->_connected = connect; }

  // Create node
  ZKRet createNode2(const string &path, const string &value, int flag,
                    char *rpath, int rpathlen, bool recursive);

  // Set session client id
  void setClientId(const clientid_t *id);

  // Check exit
  bool isExit(void) { return _exit_flag; }

  // Abstract watch of DataWatch and ChildrenWatch
  class Watcher {
   public:
    Watcher(ZKProcess *zk, const string &path);
    virtual ~Watcher() {}

    // Set async watch function
    virtual void Get(void) const = 0;

    // Get watch path
    const string &getPath(void) const { return _path; }

    // Get ZKProcess handle
    ZKProcess *getZk(void) const { return _zk; }

   protected:
    ZKProcess *_zk;

    // watch path
    string _path;
  };

  // Watch path data
  class DataWatcher : public Watcher {
   public:
    typedef DataWatcherCallback Callback;

   public:
    DataWatcher(ZKProcess *zk, const string &path, const Callback cb,
                void *cb_ctx);

    // Set async watch get data callback
    virtual void Get(void) const;

    // when data change, callback
    void doCallback(const string &data, const struct Stat *stat) const 
	{
      _cb(_path, data, _cb_ctx, stat);
    }

   private:
    Callback _cb;
    void *_cb_ctx;
  };

  // Watch path children
  class ChildrenWatcher : public Watcher {
   public:
    typedef ChildrenWatcherCallback Callback;

   public:
    ChildrenWatcher(ZKProcess *zk, const string &path, const Callback cb,
                    void *cb_ctx);

    // Set async watch children callback
    virtual void Get(void) const;

    // when children dir change, callback
    void doCallback(const vector<string> &data, const struct Stat *stat) const {
      _cb(_path, data, _cb_ctx, stat);
    }

   private:
    Callback _cb;
    void *_cb_ctx;
  };

  // Store all data watchs and children watchs
  class WatcherPool {
    typedef std::map<string, Watcher *> WatcherMap;

   public:
    explicit WatcherPool() {}
    ~WatcherPool() {
      for (WatcherMap::iterator it = _watcher_map.begin();
           it != _watcher_map.end(); ++it) {
        delete it->second;
      }
      _watcher_map.clear();
    }

    // Create DataWatch or ChildrenWatch
    template <class T>
    Watcher *createWatcher(ZKProcess *zk, const string &path,
                           typename T::Callback cb, void *cb_ctx) {
      string name = typeid(T).name() + path;
      WatcherMap::iterator it = _watcher_map.find(name);

      if (_watcher_map.end() == it) {
        Watcher *w = new T(zk, path, cb, cb_ctx);
        if (NULL == w) {
          MYBOOST_LOG_ERROR( "New " << typeid(T).name() << ", ERROR: " << errno
                    << ", " << GetLastErrorMsg(errno) );
          return NULL;
        }

        _watcher_map[name] = w;
        return w;
      } else {
        return it->second;
      }
    }

    // erase DataWatch or ChildrenWatch
    template <class T>
    Watcher *eraseWatcher(ZKProcess *zk, const string &path) {
      string name = typeid(T).name() + path;
      WatcherMap::iterator it = _watcher_map.find(name);

      if (_watcher_map.end() == it) {
        return NULL;
      } else {
        Watcher *w = it->second;
        _watcher_map.erase(name);
        return w;
      }
    }

    // Get DataWatch or ChildrenWatch
    template <class T>
    Watcher *getWatcher(const string &path) {
      string name = typeid(T).name() + path;

      WatcherMap::iterator it = _watcher_map.find(name);
      if (_watcher_map.end() == it) {
        return NULL;
      } else {
        return it->second;
      }
    }

    // Call all watchers
    void getAll(void) const {
      for (WatcherMap::const_iterator it = _watcher_map.begin();
           it != _watcher_map.end(); ++it) {
        it->second->Get();
      }
    }

   private:
    WatcherMap _watcher_map;
  };

  // zookeeper host list
  string _hosts;

  // true if had connected to zookeeper
  bool _connected;

  // set by configure file
  uint32_t _recv_timeout;

  std::string _sroot_path;

  // zookeeper log file
  FILE *_log_stream;

  // zookeeper handle
  zhandle_t *_zk_handle;

  // Zookeeper log level
  ZooLogLevel _default_log_level;

  // Zookeeper all watchers
  WatcherPool _watcher_pool;

  // session client id
  clientid_t _client_id;

  // Zookeeper status change callback function
  UserWatcherCallback _user_func;

  // Zookeeper status change callback function param
  void *_user_ctx;

  std::mutex _oMutex;
  ;

  // Exit flag, if true, not reconnect zookeeper
  volatile bool _exit_flag;
};

}  // namespace aiad
