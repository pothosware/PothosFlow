// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QFutureWatcher>
#include <map>
#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <Poco/RWLock.h>

class HostExplorerDock;

class BlockCache : public QObject
{
    Q_OBJECT
public:

    //! Get access to the global block cache
    static BlockCache *global(void);

    BlockCache(QObject *parent, HostExplorerDock *hostExplorer);

    //! Get a block description given the block registry path
    Poco::JSON::Object::Ptr getBlockDescFromPath(const std::string &path);

signals:
    void blockDescUpdate(const Poco::JSON::Array::Ptr &);
    void blockDescReady(void);

public slots:
    void handleUpdate(void);

private slots:
    void handleWatcherFinished(void);

    void handleWatcherDone(const int which);

private:
    HostExplorerDock *_hostExplorerDock;
    QStringList _allRemoteNodeUris;
    QFutureWatcher<Poco::JSON::Array::Ptr> *_watcher;

    //storage structures
    Poco::RWLock _mapMutex;
    std::map<QString, Poco::JSON::Array::Ptr> _uriToBlockDescs;
    std::map<std::string, Poco::JSON::Object::Ptr> _pathToBlockDesc;
};
