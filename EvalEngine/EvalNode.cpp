// Copyright (c) 2017-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "EvalNode.hpp"
#include "EvalGraph.hpp"

EvalNode::EvalNode(EvalGraph *parent):
    QObject(parent),
    _graph(parent),
    _ready(false)
{
    return;
}

EvalNode::~EvalNode(void)
{
    return;
}

bool EvalNode::ready(void) const
{
    for (auto node : this->dependencies())
    {
        if (not node->ready()) return false;
    }
    return _ready and _errorMsg.isEmpty();
}

bool EvalNode::process(QString &errorMsg)
{
    errorMsg = "";
    return true;
}

QSet<EvalNode *> EvalNode::dependencies(void) const
{
    for (const auto &pair : this->graph()->_connections)
    {
        if (pair.first == this) return pair.second;
    }
    return QSet<EvalNode *>();
}

void EvalNode::invokeProcess(void)
{
    _errorMsg = "";
    _ready = this->process(_errorMsg);
}
