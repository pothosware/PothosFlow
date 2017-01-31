// Copyright (c) 2017-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QObject>
#include <QSet>
#include <map>

class EvalNode;

class EvalGraph : public QObject
{
    Q_OBJECT
public:

    //! Create a new graph with parent
    EvalGraph(QObject *parent = nullptr);

    /*!
     * Create a dependency connection.
     * No change if connection already exists.
     */
    void connect(EvalNode *node, EvalNode *dependency);

    /*!
     * Remove a dependency connection.
     * Return true if removed, false if not present.
     */
    bool disconnect(EvalNode *node, EvalNode *dependency);

private:
    std::map<EvalNode *, QSet<EvalNode*>> _connections;

    friend class EvalNode;
};
