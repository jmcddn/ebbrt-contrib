//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef APPS_ZK_BAREMETAL_SRC_ZOOKEEPER_H_
#define APPS_ZK_BAREMETAL_SRC_ZOOKEEPER_H_

#include <cstdio>
#include <ebbrt/Debug.h>
#include <ebbrt/Future.h>
#include <ebbrt/SharedEbb.h>
#include <ebbrt/SharedIOBufRef.h>
#include <ebbrt/SpinLock.h>
#include <ebbrt/Timer.h>
#include <string>
#include <zookeeper.h>

namespace ebbrt {

namespace {

constexpr int ZkConnectionTimeoutMs = 30000;
constexpr int ZkIoEventTimer = 1000;
}

typedef struct Stat ZkStat;
/* struct ZkStat:
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
*/

class ZooKeeper : public ebbrt::Timer::Hook {
public:
  struct Znode {
    int err;
    std::string value;
    ZkStat stat;
  };

  struct ZnodeChildren {
    int err;
    std::vector<std::string> values;
    ZkStat stat;
  };

  /* Watcher is the specified callback for one or many 'watcher' events
   */
  class Watcher {
  public:
    virtual ~Watcher() {}
    virtual void OnConnected(){};
    virtual void OnConnecting(){};
    virtual void OnSessionExpired(){};
    virtual void OnCreated(const char *path){};
    virtual void OnDeleted(const char *path){};
    virtual void OnChanged(const char *path){};
    virtual void OnChildChanged(const char *path){};
    virtual void OnNotWatching(const char *path){};
    virtual void OnAuthFailed(){};
    virtual void OnAssociating(){};
    virtual void WatchHandler(int type, int state, const char *path) {
      if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_EXPIRED_SESSION_STATE) {
          OnSessionExpired();
        } else if (state == ZOO_CONNECTED_STATE) {
          OnConnected();
        } else if (state == ZOO_CONNECTING_STATE) {
          OnConnecting();
        } else if (state == ZOO_ASSOCIATING_STATE) {
          OnAssociating();
        } else if (state == ZOO_AUTH_FAILED_STATE) {
          OnAuthFailed();
        } else {
          ebbrt::kabort("unsupported session event");
        }
      } else if (type == ZOO_CREATED_EVENT) {
        OnCreated(path);
      } else if (type == ZOO_DELETED_EVENT) {
        OnDeleted(path);
      } else if (type == ZOO_CHANGED_EVENT) {
        OnChanged(path);
      } else if (type == ZOO_CHILD_EVENT) {
        OnChildChanged(path);
      } else if (type == ZOO_NOTWATCHING_EVENT) {
        OnNotWatching(path);
      } else {
        ebbrt::kabort("unsupported event type");
      }
    }
  };

  static EbbRef<ZooKeeper> Create(EbbId id, const std::string &server_hosts,
                                  Watcher *connection_watcher = nullptr,
                                  int timeout_ms = ZkConnectionTimeoutMs,
                                  int timer_ms = ZkIoEventTimer) {
    auto rep =
        new ZooKeeper(server_hosts, connection_watcher, timeout_ms, timer_ms);
    local_id_map->Insert(std::make_pair(id, std::move(rep)));
    return EbbRef<ZooKeeper>(id);
  }

  static ZooKeeper &HandleFault(EbbId id) {
    LocalIdMap::ConstAccessor accessor;
    auto found = local_id_map->Find(accessor, id);
    if (!found)
      throw std::runtime_error("Failed to find root for ZooKeeper Ebb");

    auto rep = boost::any_cast<ZooKeeper *>(accessor->second);
    EbbRef<ZooKeeper>::CacheRef(id, *rep);
    return *rep;
  }

  void Fire() override;
  ~ZooKeeper();
  // disable copy
  ZooKeeper(const ZooKeeper &) = delete;
  ZooKeeper &operator=(const ZooKeeper &) = delete;
  void CLI(char *line);
  /*
   * ZooKeeper::New
   *
   * - flags:   ZOO_EPHEMERAL - node will automatically get removed if the
   *                          client session goes away.
   *            ZOO_SEQUENCE  - a unique monotonically increasing sequence
   *                          number is appended to the path name.
   */
  Future<Znode> New(const std::string &path,
                    const std::string &value = std::string(), int flags = 0);
  Future<Znode> Exists(const std::string &path, Watcher *watch = nullptr);
  Future<Znode> Get(const std::string &path, Watcher *watch = nullptr);
  Future<Znode> Delete(const std::string &path, int version = -1);
  Future<Znode> Set(const std::string &path, const std::string &value,
                    int version = -1);
  Future<ZnodeChildren> GetChildren(const std::string &path,
                                    Watcher *watch = nullptr);
  static void PrintZnode(Znode *zkr);
  static void PrintZnodeChildren(ZnodeChildren *zkcr);

  static void my_data_completion(int rc, const char *value, int value_len,
                                 const ZkStat *stat, const void *data);
  static void my_silent_data_completion(int rc, const char *value,
                                        int value_len, const ZkStat *stat,
                                        const void *data);
  static void my_stat_completion(int rc, const ZkStat *stat, const void *data);
  static void my_string_completion(int rc, const char *name, const void *data);
  static void my_string_completion_free_data(int rc, const char *name,
                                             const void *data);
  static void my_strings_completion(int rc, const struct String_vector *strings,
                                    const void *data);
  static void my_strings_stat_completion(int rc,
                                         const struct String_vector *strings,
                                         const ZkStat *stat, const void *data);
  static void my_void_completion(int rc, const void *data);
  static void my_silent_stat_completion(int rc, const ZkStat *stat,
                                        const void *data);

private:
  ZooKeeper(const std::string &server_hosts,
            ebbrt::ZooKeeper::Watcher *connection_watcher, int timeout_ms,
            int timer_ms);
  zhandle_t *zk_ = nullptr;
  SpinLock lock_;
  int verbose = 0; // clean this
  Watcher *connection_watcher_ = nullptr;
  static void process_watch_event(zhandle_t *, int type, int state,
                                  const char *path, void *ctx);
  static int startsWith(const char *line, const char *prefix);
  static void print_stat(ZkStat *stat);
};
}

#endif
