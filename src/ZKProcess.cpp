#include "ZKProcess.h"
#include <iostream>
#include "util/myboost_log.h"
#include "util/TafOsVar.h"

namespace aiad {

const char *ErrorStr(int code) {
  switch (code) {
    case ZOK:
      return "Everything Is OK";

    case ZSYSTEMERROR:
      return "System Error";

    case ZRUNTIMEINCONSISTENCY:
      return "A Runtime Inconsistency Was Found";

    case ZDATAINCONSISTENCY:
      return "A Data Inconsistency Was Found";

    case ZCONNECTIONLOSS:
      return "zkConnection To The Server Has Been Lost";

    case ZMARSHALLINGERROR:
      return "Error While Marshalling Or Unmarshalling Data";

    case ZUNIMPLEMENTED:
      return "Operation Is Unimplemented";

    case ZOPERATIONTIMEOUT:
      return "Operation Timeout";

    case ZBADARGUMENTS:
      return "Invalid Arguments";

    case ZINVALIDSTATE:
      return "Invalid Zhandle State";

    case ZAPIERROR:
      return "Api Error";

    case ZNONODE:
      return "Node Does Not Exist";

    case ZNOAUTH:
      return "Not Authenticated";

    case ZBADVERSION:
      return "Version Conflict";

    case ZNOCHILDRENFOREPHEMERALS:
      return "Ephemeral Nodes May Not Have Children";

    case ZNODEEXISTS:
      return "The Node Already exists";

    case ZNOTEMPTY:
      return "The Node Has Children";

    case ZSESSIONEXPIRED:
      return "The Session Has Been Expired By The Server";

    case ZINVALIDCALLBACK:
      return "Invalid Callback Specified";

    case ZINVALIDACL:
      return "Invalid ACL Specified";

    case ZAUTHFAILED:
      return "Client Authentication Failed";

    case ZCLOSING:
      return "ZooKeeper Is Closing";

    case ZNOTHING:
      return "(Not Error) No Server Responses To Process";

    case ZSESSIONMOVED:
      return "Session Moved To Another Server, So Operation Is Ignored";

    default:
      return "Unknown Error";
  }
}

const char *EventStr(int event) {
  if (ZOO_CREATED_EVENT == event)
    return "ZOO_CREATED_EVENT";
  else if (ZOO_DELETED_EVENT == event)
    return "ZOO_DELETED_EVENT";
  else if (ZOO_CHANGED_EVENT == event)
    return "ZOO_CHANGED_EVENT";
  else if (ZOO_SESSION_EVENT == event)
    return "ZOO_SESSION_EVENT";
  else if (ZOO_NOTWATCHING_EVENT == event)
    return "ZOO_NOTWATCHING_EVENT";
  else
    return "Unknown Event";
}

const char *StateStr(int state) {
  if (ZOO_EXPIRED_SESSION_STATE == state)
    return "ZOO_EXPIRED_SESSION_STATE";
  else if (ZOO_AUTH_FAILED_STATE == state)
    return "ZOO_AUTH_FAILED_STATE";
  else if (ZOO_CONNECTING_STATE == state)
    return "ZOO_CONNECTING_STATE";
  else if (ZOO_ASSOCIATING_STATE == state)
    return "ZOO_ASSOCIATING_STATE";
  else if (ZOO_CONNECTED_STATE == state)
    return "ZOO_CONNECTED_STATE";
  else
    return "Unknown State";
}

// Return the error message
std::string GetLastErrorMsg(int err) {
  char err_msg[1000] = {0x0};
  char * msg = err_msg;
  int result = 0;

#ifdef _GNU_SOURCE
	msg = strerror_r( err, err_msg, sizeof( err_msg ) );
#else 
	result = strerror_r( err, err_msg, sizeof( err_msg ) );
	if( result != 0 )
	{
		sprintf( err_msg, "Unknown error:%d ", err );
	}
#endif
  return std::string(msg);
}

std::string ParentPath(const std::string& path) {
  if (path.empty()) return "";
  //
  size_t pos = path.rfind('/');
  if (path.length() - 1 == pos) {
    // skip the tail '/'
    pos = path.rfind('/', pos - 1);
  }
  if (std::string::npos == pos) {
    return "/";  //  parent path of "/" is also "/"
  } else {
    return path.substr(0, pos);
  }
}

ZKProcess::ZKProcess() {
  _zk_handle = NULL;
  _log_stream = stderr;
  _connected = false;
  _user_func = NULL;
  _user_ctx = NULL;
  _exit_flag = false;
  setDebugLogLevel(false);
  _recv_timeout = ZK_DEFAULT_RECVTIMEOUT;
  _default_log_level = ZOO_LOG_LEVEL_ERROR;
  memset(&_client_id, 0, sizeof(clientid_t));
}

ZKProcess::~ZKProcess() {
  if (NULL != _zk_handle) {
    zookeeper_close(_zk_handle);
    _zk_handle = NULL;
  }
}

bool ZKProcess::zkConnect(const string &hosts, const uint32_t recv_time,
                          const std::string sRootPath, FILE *log_stream, 
						  UserWatcherCallback user_func,void *user_ctx) {
  if (hosts.empty()) {
    MYBOOST_LOG_ERROR("zkConnect Hosts Is Empty" );
    return false;
  }

  this->_user_func = user_func;
  this->_user_ctx = user_ctx;

  if (NULL != _zk_handle) return true;

  this->_hosts = hosts;
  this->_recv_timeout = recv_time;
  this->_sroot_path = sRootPath;

  if (NULL != _log_stream) this->_log_stream = log_stream;

  return doConnect();
}

bool ZKProcess::doConnect() {
  if (this->_hosts.empty()) {
	  MYBOOST_LOG_ERROR("ZKProcess::doConnect Hosts Is Empty" );
    return false;
  }

  if (NULL != _zk_handle) return true;

  _zk_handle = zookeeper_init(this->_hosts.c_str(), defaultWatcher, _recv_timeout, &_client_id, this, 0);
  if (NULL == _zk_handle) {
	  MYBOOST_LOG_ERROR("ZKProcess::doConnect to Zookeeper handle:" << OS_KV( "host", _hosts ) );
    return false;
  }

  // wait for zookeeper connected callback
  for (int i = 0; i < CONNECT_WAIT_TIME; ++i) {
    sleep(1);
    if (_connected) {
	  MYBOOST_LOG_ERROR("Sleep1 Second Wait For Zookeeper connected." );
      goto end;
    } else {
	  MYBOOST_LOG_ERROR( "Sleep1 Second Wait For Zookeeper connecting..." );
    }
  }

  MYBOOST_LOG_ERROR("ZKProcess::doConnect to Zookeeper: "
            << this->_hosts << ", Failt. Not Response After: "
            << CONNECT_WAIT_TIME << " Second" );

  zookeeper_close(_zk_handle);
  memset(&_client_id, 0, sizeof(clientid_t));
  _zk_handle = NULL;

  return false;

end:
  MYBOOST_LOG_ERROR("ZKProcess::doConnect to Zookeeper: " << this->_hosts << ", OK!!");
  return true;
}

void ZKProcess::disConnect(void) {
  if (NULL == _zk_handle) return;

  _exit_flag = true;

  if (_connected) {
    zookeeper_close(_zk_handle);
    memset(&_client_id, 0, sizeof(clientid_t));
  }

  _zk_handle = NULL;
  _connected = false;
  MYBOOST_LOG_ERROR("Disconnect To Zookeeper: " << _hosts );
  return;
}

ZKRet ZKProcess::createNode(const string &path, const string &value,
                            bool recurisive) {
  if (!isConnected() || NULL == _zk_handle) {
    return ZKRet(ZCLOSING);
  }

  return createNode2(path, value, 0, NULL, 0, recurisive);
}

ZKRet ZKProcess::createEphAndSeqNode(const string &path, const string &value,
                                     string &rpath) {
  if (!isConnected() || NULL == _zk_handle) {
    return ZKRet(ZCLOSING);
  }

  std::lock_guard<std::mutex> oLock(_oMutex);

  // Sync path
  if (!syncPath(path)) {
    MYBOOST_LOG_ERROR("Sync Zookeeper Path: " << path << " Failt" );
    return ZKRet(ZSYSTEMERROR);
  }

  char buffer[ZK_RPATH_LEN] = {0};
  ZKRet zret = createNode2(path, value, ZOO_EPHEMERAL | ZOO_SEQUENCE, buffer,
                           sizeof(buffer), false);
  rpath = buffer;
  return zret;
}

ZKRet ZKProcess::createEph(const string &path, const string &value,
                           string &rpath) {
  std::lock_guard<std::mutex> oLock(_oMutex);

  // Sync path
  if (!syncPath(path)) {
    MYBOOST_LOG_ERROR("createEph:: Sync Zookeeper Path: " << path << " Fail." );
    return ZKRet(ZSYSTEMERROR);
  }

  char buffer[ZK_RPATH_LEN] = {0};
  ZKRet zret =
      createNode2(path, value, ZOO_EPHEMERAL, buffer, sizeof(buffer), false);
  rpath = buffer;
  return zret;
}

// recursive create dir in zookeeper if recursive is true
ZKRet ZKProcess::createNode2(const string &path, const string &value, int flag,
                             char *rpath, int rpathlen, bool recursive) {

  int ret = zoo_create(_zk_handle, path.c_str(), value.c_str(), value.length(),
                       &ZOO_OPEN_ACL_UNSAFE, flag, rpath, rpathlen);

  if (ZNONODE == ret) {
    if (!recursive) {
      return ZKRet(ret);
    }

    string ppath = ParentPath(path);

    if (ppath.empty()) {
      MYBOOST_LOG_ERROR("Recursive createNode Parent Path: " << ppath << ", Is Empty");
      return ZKRet(ret);
    }

    ZKRet zret = createNode2(ppath.c_str(), "", 0, NULL, 0, true);

    if (zret.Ok() || zret.NodeExists()) {
      ret = zoo_create(_zk_handle, path.c_str(), value.c_str(), value.length(),
                       &ZOO_OPEN_ACL_UNSAFE, flag, rpath, rpathlen);
      if (ZOK != ret && ZNODEEXISTS != ret) {
        MYBOOST_LOG_ERROR("Create Node: " << path << ", Failt: " << ErrorStr(ret));
      }
      return ZKRet(ret);
    } else {
      return zret;
    }
  } else if (ZOK != ret && ZNODEEXISTS != ret) {
    MYBOOST_LOG_ERROR("Create Node: " << path << ", Failt: " << ErrorStr(ret));
  }
  return ZKRet(ret);
}

ZKRet ZKProcess::deleteNode(const string &path, int version) {
  if (!isConnected() || NULL == _zk_handle) {
    return ZKRet(ZCLOSING);
  }

  std::lock_guard<std::mutex> oLock(_oMutex);

  struct Stat GetStat;
  memset(&GetStat, 0, sizeof(GetStat));

  if (ZOK != zoo_exists(_zk_handle, path.c_str(), 0, &GetStat)) {
    return ZKRet(ZOK);
  }

  if (version == 0) {
    version = GetStat.version;
  }

  int ret = zoo_delete(_zk_handle, path.c_str(), version);

  return ZKRet(ret);
}

bool ZKProcess::isConnected() {
  if (_zk_handle == NULL) {
    return doConnect();
  }
  struct Stat GetStat;
  memset(&GetStat, 0, sizeof(GetStat));

  if ((zhandle_t *)0x0 == _zk_handle) {
    return false;
  }

  if (ZOK == zoo_exists(_zk_handle, _sroot_path.c_str(), 0, &GetStat)) {
    return true;
  } else {
    //创建节点
    std::string value = "1";
    ZKRet zret = createNode2(_sroot_path, value, 0, NULL, 0, true);
    if (zret != ZOK) {
      MYBOOST_LOG_ERROR("isConnected createNode2: "
                << _sroot_path << " zret " << zret.getCode()
                << ", will doConnect.." );
      //创建不成功，需要重新连接。
      if (NULL != _zk_handle) {
        zookeeper_close(_zk_handle);
        _zk_handle = NULL;
      }
      return doConnect();
    } else {
      return true;
    }
  }
  return false;
}

bool ZKProcess::getData(const string &path, string &value, struct Stat *stat) {
  if (!isConnected() || NULL == _zk_handle) {
    return false;
  }

  std::lock_guard<std::mutex> oLock(_oMutex);

  struct Stat GetStat;
  memset(&GetStat, 0, sizeof(GetStat));
  if (ZOK == zoo_exists(_zk_handle, path.c_str(), 0, &GetStat)) {
    int buffer_size = GetStat.dataLength + 1;
    char *buffer = new char[buffer_size];
    memset(buffer, 0, buffer_size);

    int ret = zoo_get(_zk_handle, path.c_str(), 0, buffer, &buffer_size, stat);
    if (ZOK != ret) {
      MYBOOST_LOG_ERROR("Get Path: " << path << ", Failt: " << ErrorStr(ret) );
      delete[] buffer;
      return false;
    } else {
      value = buffer;
      delete[] buffer;
      return true;
    }
  } else {
    return false;
  }
}

bool ZKProcess::setData(const string &path, const string &value,
                        struct Stat *stat) {
  if (!isConnected() || NULL == _zk_handle) {
    return false;
  }

  int ret = 0;

  // release lock_
  {
    std::lock_guard<std::mutex> oLock(_oMutex);
    ret = zoo_set2(_zk_handle, path.c_str(), value.c_str(), value.length(), -1,
                   stat);
  }

  if (ZOK != ret) {
    // if not exists, create dir
    if (ZNONODE == ret) {
      return createNode(path, value, true);
    } else {
      MYBOOST_LOG_ERROR("Set Path: " << path << ", Value: " << value
                             << ", Failt: " << ErrorStr(ret) );
      return false;
    }
  } else {
    return true;
  }
}

bool ZKProcess::exists(const string &path, struct Stat *stat) {
  if (!isConnected() || NULL == _zk_handle) {
    return false;
  }

  if (NULL == _zk_handle) return true;
  return (ZOK == zoo_exists(_zk_handle, path.c_str(), 0, stat));
}

bool ZKProcess::getChildren(const string &path, std::vector<string> &children,
                            struct Stat *stat) {
  if (!isConnected() || NULL == _zk_handle) {
    return false;
  }

  String_vector sv;

  std::lock_guard<std::mutex> oLock(_oMutex);

  // zoo_get_children2 not check stat is NULL
  int ret = 0;
  if (NULL != stat) {
    ret = zoo_get_children2(_zk_handle, path.c_str(), 0, &sv, stat);
  } else {
    ret = zoo_get_children(_zk_handle, path.c_str(), 0, &sv);
  }
  if (ZOK != ret) {
    MYBOOST_LOG_ERROR("Get Children: " << path << ", Failt: " << ErrorStr(ret)
                               );
    return false;
  } else {
    for (int i = 0; i < sv.count; ++i) {
      children.push_back(sv.data[i]);
    }
    deallocate_String_vector(&sv);
    return true;
  }
}

void ZKProcess::setDebugLogLevel(bool open) {
  ZooLogLevel log_level = _default_log_level;

  if (open) {
    log_level = ZOO_LOG_LEVEL_DEBUG;
  }
  std::lock_guard<std::mutex> oLock(_oMutex);
  zoo_set_debug_level(log_level);
}

bool ZKProcess::syncPath(const string &path) {
  if (!isConnected() || NULL == _zk_handle) {
    return false;
  }

  // TLOGDEBUG("Sync Zoopkeeper Path: " << path );

  int rc = -999;
  int ret =
      zoo_async(_zk_handle, path.c_str(), &ZKProcess::syncCompletion, &rc);

  if (ZOK != ret) {
    MYBOOST_LOG_ERROR("Zoo_async Failt: " << ErrorStr(ret) );
    return false;
  }

  // Wait the timeout for this session
  for (int i = 0; i < getRecvTimeout() / 10; ++i) {
    if (-999 == rc) {
      // TLOGDEBUG("Wait Zoo_async Callback. Sleep 10 ms" );
      usleep(10 * 1000);
    }
  }

  if (-999 == rc) {
    MYBOOST_LOG_ERROR("Wait: " << getRecvTimeout() << " ms Zoo_async Not Callback");
    return false;
  } else {
    return true;
  }
}

void ZKProcess::defaultWatcher(zhandle_t *handle, int type, int state,
                               const char *path, void *watcherCtx) {
  ZKProcess *zk = static_cast<ZKProcess *>(watcherCtx);

  if (ZOO_SESSION_EVENT == type) {
    if (ZOO_CONNECTED_STATE == state) {
      zk->setConnected(true);
      zk->setClientId(zoo_client_id(handle));

      MYBOOST_LOG_INFO( "Connected To Zookeeper: " << OS_KV( "host", zk->getHosts() ) 
                << ", Seesion State: " << OS_KV( "state", StateStr(state) ) );
    } else if (ZOO_EXPIRED_SESSION_STATE == state) {
      zk->disConnect();
      zk->setConnected(false);

      MYBOOST_LOG_ERROR("Zookeeper Session Expired" );

      zk->doConnect();

      // reconnect zookeeper
      // if (!zk->isExit()) zk->restart();
    } else {
      zk->setConnected(false);

      MYBOOST_LOG_ERROR("Not Connect To Zookeeper: "
                << zk->getHosts() << ", Seesion State: " << StateStr(state) );

      // reconnect zookeeper
      if (!zk->isExit()) zk->restart();
    }
  } else if (ZOO_CREATED_EVENT == type) {
    MYBOOST_LOG_ERROR("Create Node: " << path );
  } else if (ZOO_DELETED_EVENT == type) {
    MYBOOST_LOG_ERROR("Delete Node: " << path );
  } else if (ZOO_CHANGED_EVENT == type) {
    MYBOOST_LOG_DEBUG("Changed Node: " << path );
    zk->_watcher_pool.getWatcher<DataWatcher>(path)->Get();
  } else if (ZOO_CHILD_EVENT == type) {
    MYBOOST_LOG_DEBUG("Children Node: " << path );
    zk->_watcher_pool.getWatcher<ChildrenWatcher>(path)->Get();
  } else {
    MYBOOST_LOG_ERROR("Unhandled Zookeeper Event: " << EventStr(type) );
  }

  zk->callUserCallback(type, state, path);
}

void ZKProcess::dataCompletion(int rc, const char *value, int valuelen,
                               const struct Stat *stat, const void *data) {
  const DataWatcher *watcher =
      dynamic_cast<const DataWatcher *>(static_cast<const Watcher *>(data));

  if (ZOK == rc) {
    watcher->doCallback(string(value, valuelen), stat);
  } else {
    MYBOOST_LOG_ERROR("dataCompletion Path: " << watcher->getPath() << ", Failt: " << ErrorStr(rc) );
  }
}

void ZKProcess::childrenCompletion(int rc, const struct String_vector *strings,
                                   const struct Stat *stat, const void *data) {
  const ChildrenWatcher *watcher =
      dynamic_cast<const ChildrenWatcher *>(static_cast<const Watcher *>(data));

  if (ZOK == rc) {
    vector<string> vc;
    for (int i = 0; i < strings->count; ++i) {
      vc.push_back(strings->data[i]);
    }

    watcher->doCallback(vc, stat);
  } else {
    MYBOOST_LOG_ERROR("childrenCompletion Path: " << watcher->getPath() << ", Failt: "
                                          << ErrorStr(rc) );
  }
}

void ZKProcess::syncCompletion(int rc, const char *value, const void *data) {
  if (NULL == data) {
    return;
  }
  int *ret = static_cast<int *>(const_cast<void *>(data));
  *ret = rc;
}

bool ZKProcess::watchData(const string &path, DataWatcherCallback cb,
                          void *cb_ctx) {
  if (!_connected) {
    MYBOOST_LOG_ERROR("Not Connect To Zookeeper" );
    return false;
  }

  if (!exists(path)) {
    MYBOOST_LOG_ERROR("Path: " << path << ", Not exists In Zookeeper" );
    return false;
  }

  Watcher *w = _watcher_pool.createWatcher<DataWatcher>(this, path, cb, cb_ctx);
  w->Get();

  return true;
}

bool ZKProcess::delWatchData(const string &path) {
  if (!_connected) {
    MYBOOST_LOG_ERROR("Not Connect To Zookeeper" );
    return false;
  }

  if (!exists(path)) {
    MYBOOST_LOG_ERROR("Path: " << path << ", Not exists In Zookeeper" );
    return false;
  }

  //Watcher *w = _watcher_pool.eraseWatcher<DataWatcher>(this, path);

  return true;
}

bool ZKProcess::watchChildren(const string &path, ChildrenWatcherCallback cb,
                              void *cb_ctx) {
  if (!_connected) {
    MYBOOST_LOG_ERROR("Not zkConnect To Zookeeper" );
    return false;
  }

  if (!exists(path)) {
    MYBOOST_LOG_ERROR("Watch Children: " << path << ", Not exists" );
    return false;
  }

  Watcher *w =
      _watcher_pool.createWatcher<ChildrenWatcher>(this, path, cb, cb_ctx);
  w->Get();

  return true;
}

bool ZKProcess::restart() {
  MYBOOST_LOG_ERROR("RezkConnect To Zookeeper: " << _hosts );
  if (NULL != _zk_handle) {
    zookeeper_close(_zk_handle);
    _zk_handle = NULL;
  }
  if (!doConnect()) {
    MYBOOST_LOG_ERROR("Reconnect Zookeeper Failt" );
    return false;
  }
  return true;
}

ZKProcess::Watcher::Watcher(ZKProcess *zk, const string &path)
    : _zk(zk), _path(path) {}

ZKProcess::DataWatcher::DataWatcher(ZKProcess *zk, const string &path,
                                    const Callback cb, void *cb_ctx)
    : Watcher(zk, path), _cb(cb), _cb_ctx(cb_ctx) {}

void ZKProcess::DataWatcher::Get() const {
  int ret =
      zoo_awget(_zk->_zk_handle, _path.c_str(), &ZKProcess::defaultWatcher,
                this->getZk(), &ZKProcess::dataCompletion, this);
  if (ZOK != ret) {
    MYBOOST_LOG_ERROR("Zoo_awget Path: " << _path << ", Failt: " << ErrorStr(ret) );
  }
}

ZKProcess::ChildrenWatcher::ChildrenWatcher(ZKProcess *zk, const string &path,
                                            const Callback cb, void *cb_ctx)
    : Watcher(zk, path), _cb(cb), _cb_ctx(cb_ctx) {}

void ZKProcess::ChildrenWatcher::Get() const {
  int ret = zoo_awget_children2(_zk->_zk_handle, _path.c_str(),
                                &ZKProcess::defaultWatcher, this->getZk(),
                                &ZKProcess::childrenCompletion, this);
  if (ZOK != ret) {
    MYBOOST_LOG_ERROR("Zoo_awget_children Path: " << _path << ", Failt: "
                                          << ErrorStr(ret) );
  }
}

void ZKProcess::setClientId(const clientid_t *id) {
  if (NULL != id &&
      (_client_id.client_id == 0 || _client_id.client_id != id->client_id)) {
    memcpy(&_client_id, id, sizeof(clientid_t));
  }
}

}  // namespace aiad
