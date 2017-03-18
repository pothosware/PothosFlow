// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "GraphObjects/GraphObject.hpp"
#include <Pothos/Proxy/Proxy.hpp>
#include <QObject>
#include <QString>
#include <vector>
#include <memory>
#include <map>
#include <Poco/Logger.h>

class BlockEval;

namespace Pothos
{
    class Topology;
}

/*!
 * Information about a connection between src and dst ports.
 * This is everything important we need to know about connections,
 * but extracted so we can access it in a thread-safe manner.
 */
struct ConnectionInfo
{
    ConnectionInfo(void):
        srcBlockUID(0),
        dstBlockUID(0){}
    size_t srcBlockUID, dstBlockUID;
    QString srcPort, dstPort;
};

bool operator==(const ConnectionInfo &lhs, const ConnectionInfo &rhs);

//! typedef for multiple connection informations
typedef std::vector<ConnectionInfo> ConnectionInfos;

//! Calculates set(in0 - in1)
ConnectionInfos diffConnectionInfos(const ConnectionInfos &in0, const ConnectionInfos &in1);

/*!
 * TopologyEval takes up to date connection information
 * and creates topology connections between block objects.
 */
class TopologyEval : public QObject
{
    Q_OBJECT
public:

    TopologyEval(void);

    ~TopologyEval(void);

    //! helper to parse graph objects into a list of thread-safe info
    static ConnectionInfos getConnectionInfo(const GraphObjectList &graphObjects);

    /*!
     * Pass-in up-to-date connection information.
     */
    void acceptConnectionInfo(const ConnectionInfos &);

    /*!
     * Pass-in up-to-date block eval objects.
     */
    void acceptBlockEvals(const std::map<size_t, std::shared_ptr<BlockEval>> &);

    /*!
     * Disconnect any connections that involve shouldDisconnect() blocks
     */
    void disconnect(void);

    /*!
     * Perform update work after changes applied.
     */
    void update(void);

    /*!
     * Commit after changes with error handling
     */
    void commit(void);

    //! Get access to the active topology
    Pothos::Topology *getTopology(void) const
    {
        return _topology;
    }

    //! An error caused the topology to go into failure state
    bool isFailureState(void) const
    {
        return _failureState;
    }

private:
    ConnectionInfos _newConnectionInfo;
    ConnectionInfos _lastConnectionInfo;

    std::map<size_t, std::shared_ptr<BlockEval>> _newBlockEvals;
    std::map<size_t, std::shared_ptr<BlockEval>> _lastBlockEvals;

    //! The topology object thats executing this design
    Pothos::Topology *_topology;
    ConnectionInfos _currentConnections;

    bool _failureState;
    Poco::Logger &_logger;
};
