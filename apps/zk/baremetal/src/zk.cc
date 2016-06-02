//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <ebbrt/Debug.h>

#include "Printer.h"
#include "ZooKeeper.h"


class PrinterWatcher : public ebbrt::ZooKeeper::Watcher {
  public:
    void OnConnected() override { printer->Print("watch alert: Session Connected.\n"); }
    void OnConnecting() override { printer->Print("watch alert: Session Conneting.\n"); }
    void OnSessionExpired() override { printer->Print("watch alert: Session Expired.\n"); }
    void OnCreated(const char* path) override { printer->Print("watch alert: Created!\n"); }
    void OnDeleted(const char* path) override { printer->Print("watch alert: Deleted !\n"); }
    void OnChanged(const char* path) override { printer->Print("watch alert: Changed: !\n"); }
    void OnChildChanged(const char* path) override { printer->Print("watch alert: Child Changed.\n"); }
    void OnNotWatching(const char* path) override { printer->Print("watch alert: Not Wathcing.\n"); }
};

auto *mw = new PrinterWatcher();
ebbrt::ZooKeeper *zk = new ebbrt::ZooKeeper("172.17.0.4:2181", mw);

