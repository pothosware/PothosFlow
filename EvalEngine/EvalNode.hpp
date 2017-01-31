// Copyright (c) 2017-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QObject>
#include <QString>
#include <QSet>

class EvalGraph;

class EvalNode : public QObject
{
    Q_OBJECT
public:

    //! Create a new node with graph parent
    EvalNode(EvalGraph *parent);

    //! Base class destructor
    virtual ~EvalNode(void);

    //! Get the parent graph
    EvalGraph *graph(void) const;

    /*!
     * Is this node ready?
     * Meaning not outdated and not in error,
     * and the same for any dependencies.
     */
    bool ready(void) const;

    /*!
     * Is the node in error after performing processing?
     */
    bool error(void) const;

    /*!
     * Get the error message if present.
     */
    const QString &errorMsg(void) const;

    /*!
     * Get a list of dependencies for this node
     */
    QSet<EvalNode *> dependencies(void) const;

    //! Master evaluator invokes the process operation
    void invokeProcess(void);

    //! Triggered by the caller after updating state
    void changed(void);

protected:

    /*!
     * Perform processing after state change.
     * To be overloaded by the derived class.
     * Return true for complete, or false or error.
     * Set the error message when applicable.
     */
    virtual bool process(QString &errorMsg);

private:
    EvalGraph *_graph;
    bool _ready;
    QString _errorMsg;
};

inline EvalGraph *EvalNode::graph(void) const
{
    return _graph;
}

inline bool EvalNode::error(void) const
{
    return not _errorMsg.isEmpty();
}

inline const QString &EvalNode::errorMsg(void) const
{
    return _errorMsg;
}

inline void EvalNode::changed(void)
{
    _ready = false;
}
