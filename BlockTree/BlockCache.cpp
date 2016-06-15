// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "BlockTree/BlockCache.hpp"
#include "HostExplorer/HostExplorerDock.hpp"
#include "MainWindow/MainSplash.hpp"
#include <Pothos/Remote.hpp>
#include <Pothos/Proxy.hpp>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/Logger.h>
#include <iostream>
#include <map>

/***********************************************************************
 * Query JSON docs from node
 **********************************************************************/
static Poco::JSON::Array::Ptr queryBlockDescs(const QString &uri)
{
    try
    {
        auto client = Pothos::RemoteClient(uri.toStdString());
        auto env = client.makeEnvironment("managed");
        return env->findProxy("Pothos/Util/DocUtils").call<Poco::JSON::Array::Ptr>("dumpJson");
    }
    catch (const Pothos::Exception &ex)
    {
        poco_warning_f2(Poco::Logger::get("PothosGui.BlockCache"), "Failed to query JSON Docs from %s - %s", uri.toStdString(), ex.displayText());
    }

    return Poco::JSON::Array::Ptr(); //empty JSON array
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
    _watcher(new QFutureWatcher<Poco::JSON::Array::Ptr>(this))
{
    globalBlockCache = this;
    assert(_hostExplorerDock != nullptr);
    connect(_watcher, SIGNAL(resultReadyAt(int)), this, SLOT(handleWatcherDone(int)));
    connect(_watcher, SIGNAL(finished(void)), this, SLOT(handleWatcherFinished(void)));
    connect(_hostExplorerDock, SIGNAL(hostUriListChanged(void)), this, SLOT(update(void)));
}

Poco::JSON::Object::Ptr BlockCache::getBlockDescFromPath(const std::string &path)
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
            return DocUtils.call<Poco::JSON::Object::Ptr>("dumpJsonAt", path);
        }
        catch (const Pothos::Exception &)
        {
            //pass
        }
    }

    return Poco::JSON::Object::Ptr();
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
    std::map<QString, Poco::JSON::Array::Ptr> newMap;
    for (const auto &uri : _allRemoteNodeUris) newMap[uri] = _uriToBlockDescs[uri];
    _uriToBlockDescs = newMap;

    //map paths to block descs
    {
        Poco::RWLock::ScopedWriteLock lock(_mapMutex);
        _pathToBlockDesc.clear();
        for (const auto &pair : _uriToBlockDescs)
        {
            if (not pair.second) continue;
            for (const auto &blockDescObj : *pair.second)
            {
                const auto blockDesc = blockDescObj.extract<Poco::JSON::Object::Ptr>();
                const auto path = blockDesc->get("path").extract<std::string>();
                _pathToBlockDesc[path] = blockDesc;
            }
        }
    }

    //make a master block desc list
    auto superSetBlockDescs = new Poco::JSON::Array();
    {
        Poco::RWLock::ScopedReadLock lock(_mapMutex);
        for (const auto &pair : _pathToBlockDesc)
        {
            superSetBlockDescs->add(pair.second);
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
