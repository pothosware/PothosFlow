// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "EnvironmentEval.hpp"
#include "TopologyEval.hpp"
#include "BlockEval.hpp"
#include <QString>
#include <QJsonObject>
#include <QAtomicInt>
#include <memory>
#include <utility>
#include <map>
#include <set>

class EnvironmentEval;
class ThreadPoolEval;
class TopologyEval;
class BlockEval;
class GraphBlock;
class EvalTracer;
class QTimer;
class EvalEngineGuiBlockDeleter;

typedef std::map<size_t, BlockInfo> BlockInfos;
typedef std::map<QString, QJsonObject> ZoneInfos;

/*!
 * The EvalEngineImpl hold eval state and performs the actual work
 */
class EvalEngineImpl : public QObject
{
    Q_OBJECT
public:

    EvalEngineImpl(EvalTracer &tracer);

    ~EvalEngineImpl(void);

    /*!
     * Invoke a queued event on the evaluator thread:
     * Wrap sending the event count into the queue to detect if more messages
     * are en-queued and the evaluator should wait until the next message.
     */
    template <typename ...Args>
    void invokeMethod(const char *methodName, const Qt::ConnectionType type, Args && ...args)
    {
        _invokeCount++;
        QMetaObject::invokeMethod(this, "updateCurrentInvokeCount", Qt::QueuedConnection, Q_ARG(int, _invokeCount));
        QMetaObject::invokeMethod(this, methodName, type, std::forward<Args>(args)...);
    }

signals:

    //! Emitted by the monitor timer to signal that this thread is not-blocked
    void monitorHeartBeat(void);

    //! A failure occured, this is a notification to deactivate
    void deactivateDesign(void);

public slots:

    //! Submit trigger for de/activation of the topology
    void submitActivateTopology(const bool enable);

    //! Submit a single block info for individual re-eval
    void submitBlock(const BlockInfo &info);

    //! Submit most up to date topology information
    void submitTopology(const BlockInfos &blockInfos, const ConnectionInfos &connections);

    //! Submit a list if UIDs to re-evaluate
    void submitReeval(const std::vector<size_t> &uids);

    //! Submit most up to date zone information
    void submitZoneInfo(const ZoneInfos &info);

    //! query the dot markup for the active topology
    QByteArray getTopologyDotMarkup(const QByteArray &config);

    //! query the JSON dump for the active topology
    QByteArray getTopologyJSONDump(const QByteArray &config);

    //! query the JSON stats for the active topology
    QByteArray getTopologyJSONStats(void);

    //! Cleanup and shutdown prior to destruction
    void submitCleanup(void);

private slots:
    void handleMonitorTimeout(void);

    void updateCurrentInvokeCount(const int count)
    {
        _lastRxInvokeCount = count;
    }

private:
    void evaluate(void);
    bool _requireEval;

    //queued event tracking
    int _lastRxInvokeCount{0};
    QAtomicInt _invokeCount;

    EvalTracer &_tracer;
    QTimer *_monitorTimer;

    //most recent info
    BlockInfos _blockInfo;
    ConnectionInfos _connectionInfo;
    ZoneInfos _zoneInfo;

    //current state of the evaluator
    std::map<HostProcPair, std::shared_ptr<EnvironmentEval>> _environmentEvals;
    std::map<QString, std::shared_ptr<ThreadPoolEval>> _threadPoolEvals;
    std::map<size_t, std::shared_ptr<BlockEval>> _blockEvals;
    std::shared_ptr<TopologyEval> _topologyEval;

    void handleOrphanedGuiBlocks(void);
    std::set<std::shared_ptr<void>> _guiBlocks;
    std::shared_ptr<EvalEngineGuiBlockDeleter> _guiBlockDeleter;
};
