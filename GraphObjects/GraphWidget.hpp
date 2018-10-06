// Copyright (c) 2013-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "GraphObjects/GraphObject.hpp"
#include <QObject>
#include <QString>
#include <QPointF>
#include <QVariant>
#include <memory>

class GraphBlock;

/*!
 * A graph display represents a widget from a GraphBlock in graphWidget mode.
 */
class GraphWidget : public GraphObject
{
    Q_OBJECT
public:
    GraphWidget(QObject *parent);
    ~GraphWidget(void);

    void setGraphBlock(GraphBlock *block);
    GraphBlock *getGraphBlock(void) const;

    //! True when the container widget has focus
    bool containerHasFocus(void) const;

    QPainterPath shape(void) const;

    QJsonObject serialize(void) const;

    virtual void deserialize(const QJsonObject &obj);

    /*!
     * Get the saved state of the internal widget.
     */
    QVariant saveWidgetState(void) const;

    /*!
     * Restore the saved state to the internal widget.
     */
    void restoreWidgetState(const QVariant &state);

    //! True if the state changed since the last save
    bool didWidgetStateChange(void) const;

private slots:
    void handleBlockDestroyed(QObject *);
    void handleWidgetResized(void);
    void handleBlockIdChanged(const QString &id);
    void handleBlockEvalDone(void);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
