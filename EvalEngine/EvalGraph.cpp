// Copyright (c) 2017-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "EvalGraph.hpp"

EvalGraph::EvalGraph(QObject *parent):
    QObject(parent)
{
    return;
}

void EvalGraph::connect(EvalNode *node, EvalNode *dependency)
{
    _connections[node].insert(dependency);
}

bool EvalGraph::disconnect(EvalNode *node, EvalNode *dependency)
{
    return _connections[node].remove(dependency);
}
