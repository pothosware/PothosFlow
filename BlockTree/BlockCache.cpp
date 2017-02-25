// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "BlockTree/BlockCache.hpp"
#include "HostExplorer/HostExplorerDock.hpp"
#include "MainWindow/MainSplash.hpp"
#include <Pothos/Remote.hpp>
#include <Pothos/Proxy.hpp>
#include <QJsonDocument>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <Poco/Logger.h>
#include <iostream>
#include <map>

/***********************************************************************
 * Query JSON docs from node
 **********************************************************************/
static QJsonArray queryBlockDescs(const QString &uri)
{
    try
    {
        auto client = Pothos::RemoteClient(uri.toStdString());
        auto env = client.makeEnvironment("managed");
        const auto json = env->findProxy("Pothos/Util/DocUtils").call<std::string>("dumpJson");
        QJsonParseError errorParser;
        const auto jsonDoc = QJsonDocument::fromJson(QByteArray(json.data(), json.size()), &errorParser);
        if (jsonDoc.isNull()) throw Pothos::Exception(errorParser.errorString().toStdString());
        return jsonDoc.array();
    }
    catch (const Pothos::Exception &ex)
    {
        poco_warning_f2(Poco::Logger::get("PothosGui.BlockCache"), "Failed to query JSON Docs from %s - %s", uri.toStdString(), ex.displayText());
    }

    return QJsonArray(); //empty JSON array
}

/***********************************************************************
 * Block Cache impl
 **********************************************************************/
static BlockCache *globalBlockCache = nullptr;

BlockCache *BlockCache::global(void)
{
    return globalBlockCache;
}

BlockCache::BlockCache(QObject *parent, HostExplorerDock *hostExplorer):
    QObject(parent),
    _hostExplorerDock(hostExplorer),
    _watcher(new QFutureWatcher<QJsonArray>(this))
{
    globalBlockCache = this;
    assert(_hostExplorerDock != nullptr);
    connect(_watcher, SIGNAL(resultReadyAt(int)), this, SLOT(handleWatcherDone(int)));
    connect(_watcher, SIGNAL(finished(void)), this, SLOT(handleWatcherFinished(void)));
    connect(_hostExplorerDock, SIGNAL(hostUriListChanged(void)), this, SLOT(update(void)));
}

QJsonObject BlockCache::getBlockDescFromPath(const QString &path)
{
    //look in the cache
    {
        Poco::RWLock::ScopedReadLock lock(_mapMutex);
        auto it = _pathToBlockDesc.find(path);
        if (it != _pathToBlockDesc.end()) return it->second;
    }

    //search all of the nodes
    for (const auto &uri : _hostExplorerDock->hostUriList())
    {
        try
        {
            auto client = Pothos::RemoteClient(uri.toStdString());
            auto env = client.makeEnvironment("managed");
            auto DocUtils = env->findProxy("Pothos/Util/DocUtils");
            const auto json = DocUtils.call<std::string>("dumpJsonAt", path.toStdString());
            QJsonParseError errorParser;
            const auto jsonDoc = QJsonDocument::fromJson(QByteArray(json.data(), json.size()), &errorParser);
            if (jsonDoc.isNull()) throw Pothos::Exception(errorParser.errorString().toStdString());
            return jsonDoc.object();
        }
        catch (const Pothos::Exception &)
        {
            //pass
        }
    }

    return QJsonObject();
}

void BlockCache::clear(void)
{
    Poco::RWLock::ScopedWriteLock lock(_mapMutex);
    _pathToBlockDesc.clear();
}

void BlockCache::update(void)
{
    MainSplash::global()->postMessage(tr("Updating block cache..."));

    //cancel the existing future, begin a new one
    //if (_watcher->isRunning()) return;
    _watcher->cancel();
    _watcher->waitForFinished();

    //nodeKeys cannot be a temporary because QtConcurrent will reference them
    _allRemoteNodeUris = _hostExplorerDock->hostUriList();
    _watcher->setFuture(QtConcurrent::mapped(_allRemoteNodeUris, &queryBlockDescs));
}

void BlockCache::handleWatcherFinished(void)
{
    MainSplash::global()->postMessage(tr("Block cache updated."));

    //remove old nodes
    std::map<QString, QJsonArray> newMap;
    for (const auto &uri : _allRemoteNodeUris) newMap[uri] = _uriToBlockDescs[uri];
    _uriToBlockDescs = newMap;

    //map paths to block descs
    {
        Poco::RWLock::ScopedWriteLock lock(_mapMutex);
        _pathToBlockDesc.clear();
        for (const auto &pair : _uriToBlockDescs)
        {
            for (const auto &blockDescVal : pair.second)
            {
                const auto blockDesc = blockDescVal.toObject();
                const auto path = blockDesc["path"].toString();
                _pathToBlockDesc[path] = blockDesc;
            }
        }
    }

    //make a master block desc list
    QJsonArray superSetBlockDescs;
    {
        Poco::RWLock::ScopedReadLock lock(_mapMutex);
        for (const auto &pair : _pathToBlockDesc)
        {
            superSetBlockDescs.push_back(pair.second);
        }
    }

    //let the subscribers know
    emit this->blockDescReady();
    emit this->blockDescUpdate(superSetBlockDescs);
}

void BlockCache::handleWatcherDone(const int which)
{
    _uriToBlockDescs[_allRemoteNodeUris[which]] = _watcher->resultAt(which);
}
