// Copyright (c) 2014-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "EvalEngine.hpp"
#include "EvalTracer.hpp"
#include "EvalEngineImpl.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include <QSignalMapper>
#include <QThread>
#include <QTimer>
#include <cassert>

static const int MONITOR_INTERVAL_MS = 1000;
static const int THREAD_JOIN_MAX_MS = 10000;

EvalEngine::EvalEngine(QObject *parent):
    QObject(parent),
    _tracer(new EvalTracer),
    _flaggedLockUp(false),
    _logger(Poco::Logger::get("PothosFlow.EvalEngine")),
    _thread(new QThread(this)),
    _monitorTimer(new QTimer(this)),
    _impl(new EvalEngineImpl(*_tracer)),
    _blockEvalMapper(new QSignalMapper(this)),
    _affinityDock(AffinityZonesDock::global())
{
    assert(_affinityDock != nullptr);
    connect(_affinityDock, &AffinityZonesDock::zonesChanged, this, &EvalEngine::handleAffinityZonesChanged);
    connect(_blockEvalMapper, SIGNAL(mapped(QObject *)), this, SLOT(submitBlock(QObject *)));

    _impl->moveToThread(_thread);
    _thread->start();

    //manual call so initial zone info gets loaded into the evaluator
    this->handleAffinityZonesChanged();

    connect(_monitorTimer, &QTimer::timeout, this, &EvalEngine::handleMonitorTimeout);
    connect(_impl, &EvalEngineImpl::monitorHeartBeat, this, &EvalEngine::handleEvalThreadHeartBeat);
    connect(_impl, SIGNAL(deactivateDesign(void)), parent, SLOT(handleEvalEngineDeactivate(void)));
}

EvalEngine::~EvalEngine(void)
{
    _impl->invokeMethod("submitCleanup", Qt::BlockingQueuedConnection);
    delete _impl;
    _thread->quit();
    if (not _thread->wait(THREAD_JOIN_MAX_MS))
    {
        _logger.fatal("Detected lock-up when shutting down evaluation thread:\n%s",
            _tracer->trace().toStdString());
        _thread->wait();
    }
    delete _tracer;
}

static BlockInfo blockToBlockInfo(GraphBlock *block)
{
    BlockInfo blockInfo;
    blockInfo.block = block;
    blockInfo.isGraphWidget = block->isGraphWidget();
    blockInfo.id = block->getId();
    blockInfo.uid = block->uid();
    blockInfo.enabled = block->isEnabled();
    blockInfo.zone = block->getAffinityZone();
    blockInfo.desc = block->getBlockDesc();
    const auto editor = block->draw()->getGraphEditor();
    blockInfo.constantNames = editor->listGlobals();
    for (const auto &name : blockInfo.constantNames)
    {
        blockInfo.constants[name] = editor->getGlobalExpression(name);;
    }
    for (const auto &propKey : block->getProperties())
    {
        blockInfo.properties[propKey] = block->getPropertyValue(propKey);
        blockInfo.paramDescs[propKey] = block->getParamDesc(propKey);
    }
    return blockInfo;
}

void EvalEngine::submitTopology(const GraphObjectList &graphObjects)
{
    //create list of block eval information
    BlockInfos blockInfos;
    for (auto obj : graphObjects)
    {
        auto block = qobject_cast<GraphBlock *>(obj);
        if (block == nullptr) continue;
        _blockEvalMapper->setMapping(block, block);
        connect(block, SIGNAL(triggerEvalEvent(void)), _blockEvalMapper, SLOT(map(void)));
        blockInfos[block->uid()] = blockToBlockInfo(block);
    }

    //create a list of connection eval information
    const auto connInfos = TopologyEval::getConnectionInfo(graphObjects);

    //submit the information to the eval thread object
    _impl->invokeMethod("submitTopology", Qt::QueuedConnection, Q_ARG(BlockInfos, blockInfos), Q_ARG(ConnectionInfos, connInfos));
}

void EvalEngine::submitReeval(const GraphObjectList &graphObjects)
{
    std::vector<size_t> uids;
    for (auto obj : graphObjects) uids.push_back(obj->uid());
    _impl->invokeMethod("submitReeval", Qt::QueuedConnection, Q_ARG(std::vector<size_t>, uids));
}

void EvalEngine::submitActivateTopology(const bool active)
{
    _impl->invokeMethod("submitActivateTopology", Qt::QueuedConnection, Q_ARG(bool, active));
}

void EvalEngine::submitBlock(QObject *obj)
{
    auto block = qobject_cast<GraphBlock *>(obj);
    assert(block != nullptr);
    _impl->invokeMethod("submitBlock", Qt::QueuedConnection, Q_ARG(BlockInfo, blockToBlockInfo(block)));
}

QByteArray EvalEngine::getTopologyDotMarkup(const QByteArray &config)
{
    QByteArray result;
    _impl->invokeMethod("getTopologyDotMarkup", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QByteArray, result), Q_ARG(QByteArray, config));
    return result;
}

QByteArray EvalEngine::getTopologyJSONDump(const QByteArray &config)
{
    QByteArray result;
    _impl->invokeMethod("getTopologyJSONDump", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QByteArray, result), Q_ARG(QByteArray, config));
    return result;
}

QByteArray EvalEngine::getTopologyJSONStats(void)
{
    QByteArray result;
    _impl->invokeMethod("getTopologyJSONStats", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QByteArray, result));
    return result;
}

void EvalEngine::handleAffinityZonesChanged(void)
{
    ZoneInfos zoneInfos;
    for (const auto &zoneName : _affinityDock->zones())
    {
        zoneInfos[zoneName] = _affinityDock->zoneToConfig(zoneName);
    }
    _impl->invokeMethod("submitZoneInfo", Qt::QueuedConnection, Q_ARG(ZoneInfos, zoneInfos));
}

void EvalEngine::handleEvalThreadHeartBeat(void)
{
    if (_flaggedLockUp)
    {
        _flaggedLockUp = false;
        _logger.notice("Evaluation thread has recovered. Perhaps a call is taking too long.");
    }

    _lastHeartBeat = std::chrono::system_clock::now();

    //make sure monitor is started when we see the heartbeat
    //this starts the monitor on initialization and after failure
    if (not _monitorTimer->isActive()) _monitorTimer->start(MONITOR_INTERVAL_MS);
}

void EvalEngine::handleMonitorTimeout(void)
{
    if ((std::chrono::system_clock::now() - _lastHeartBeat) > std::chrono::seconds(10))
    {
        _flaggedLockUp = true;
        _monitorTimer->stop(); //stop so the error messages will not continue
        _logger.fatal("Detected evaluation thread lock-up. The evaluator will not function:\n%s",
            _tracer->trace().toStdString());
    }
}
