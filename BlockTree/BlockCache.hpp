// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QJsonArray>
#include <map>

class HostExplorerDock;
class QReadWriteLock;

class BlockCache : public QObject
{
    Q_OBJECT
public:

    //! Get access to the global block cache
    static BlockCache *global(void);

    BlockCache(QObject *parent, HostExplorerDock *hostExplorer);

    ~BlockCache(void);

    //! Get a block description given the block registry path
    QJsonObject getBlockDescFromPath(const QString &path);

signals:
    void blockDescUpdate(const QJsonArray &);
    void blockDescReady(void);

public slots:
    void clear(void);
    void update(void);

private slots:
    void handleWatcherFinished(void);

    void handleWatcherDone(const int which);

private:
    HostExplorerDock *_hostExplorerDock;
    QStringList _allRemoteNodeUris;
    QFutureWatcher<QJsonArray> *_watcher;

    //storage structures
    QReadWriteLock *_mapMutex;
    std::map<QString, QJsonArray> _uriToBlockDescs;
    std::map<QString, QJsonObject> _pathToBlockDesc;
};
