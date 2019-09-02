// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "GraphObjects/GraphObject.hpp"
#include <QPointer>
#include <QString>
#include <QPointF>
#include <QHash>

enum GraphConnectableDirection
{
    GRAPH_CONN_INPUT,
    GRAPH_CONN_OUTPUT,
    GRAPH_CONN_SLOT,
    GRAPH_CONN_SIGNAL,
};

//! An attribute struct used with connections
struct GraphConnectableAttrs
{
    int rotation; //multiple of 90 degrees
    GraphConnectableDirection direction;
    QPointF point;
};

//! A connection key describing name and direction
struct GraphConnectableKey
{
    explicit GraphConnectableKey(const QString &id = "", const GraphConnectableDirection direction = GRAPH_CONN_OUTPUT);
    QString id;
    GraphConnectableDirection direction;

    /*!
     * Is this key in the input direction?
     * True for regular inputs and slots.
     * False for regular outputs and signals.
     */
    bool isInput(void) const;

    //! Valid when the ID is non-empty
    bool isValid(void) const;
};

extern bool operator==(const GraphConnectableKey &key0, const GraphConnectableKey &key1);

namespace std
{
    template<>
    struct hash<GraphConnectableKey>
    {
        typedef GraphConnectableKey argument_type;
        typedef std::size_t value_type;

        value_type operator()(argument_type const& s) const
        {
            return qHash(s.id) ^ (qHash(s.direction) << 1);
        }
    };
}

//! A connectable endpoint described by an object and a key on that object
class GraphConnectionEndpoint
{
public:
    GraphConnectionEndpoint(const QPointer<GraphObject> &obj = QPointer<GraphObject>(), const GraphConnectableKey &key = GraphConnectableKey());

    const QPointer<GraphObject> &getObj(void) const;

    const GraphConnectableKey &getKey(void) const;

    GraphConnectableAttrs getConnectableAttrs(void) const;

    bool isValid(void) const;

private:
    QPointer<GraphObject> _obj;
    GraphConnectableKey _key;
};

extern bool operator==(const GraphConnectionEndpoint &ep0, const GraphConnectionEndpoint &ep1);

namespace std
{
    template<>
    struct hash<GraphConnectionEndpoint>
    {
        typedef GraphConnectionEndpoint argument_type;
        typedef std::size_t value_type;

        value_type operator()(argument_type const& s) const
        {
            return std::hash<GraphConnectableKey>()(s.getKey()) ^
            (std::hash<size_t>()(size_t(s.getObj().data())) << 1);
        }
    };
}
