// Copyright (c) 2017-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "EvalTracer.hpp"

EvalTracer::EvalTracer(void)
{
    return;
}

QString EvalTracer::trace(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    QString out;
    QString indent;
    for (const auto &elem : _stack)
    {
        if (not out.isEmpty())
        {
            out += "\n" + indent;
            indent += "  ";
        }
        out += elem;
    }
    return out;
}

void EvalTracer::push(const QString &pos)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _stack.push_back(pos);
}

void EvalTracer::pop(void)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _stack.pop_back();
}

EvalTraceEntry::EvalTraceEntry(EvalTracer &stack, const QString &what):
    _stack(stack)
{
    _stack.push(what);
}

EvalTraceEntry::~EvalTraceEntry(void)
{
    _stack.pop();
}
