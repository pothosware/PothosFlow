// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphObjects/GraphWidget.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphObjects/GraphWidgetContainer.hpp"
#include "GraphEditor/Constants.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include <Pothos/Exception.hpp>
#include <QGraphicsProxyWidget>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <vector>
#include <iostream>
#include <cassert>

/***********************************************************************
 * GraphWidget private container
 **********************************************************************/
struct GraphWidget::Impl
{
    Impl(QGraphicsItem *parent):
        container(new GraphWidgetContainer()),
        graphicsWidget(new QGraphicsProxyWidget(parent)),
        hasStateInterface(false)
    {
        graphicsWidget->setWidget(container);
    }

    ~Impl(void)
    {
        graphicsWidget->setWidget(nullptr);
    }

    QPointer<GraphBlock> block;

    GraphWidgetContainer *container;
    QGraphicsProxyWidget *graphicsWidget;

    QVariant widgetState;
    bool hasStateInterface;
};

/***********************************************************************
 * GraphWidget Implementation
 **********************************************************************/
GraphWidget::GraphWidget(QObject *parent):
    GraphObject(parent),
    _impl(new Impl(this))
{
    this->setFlag(QGraphicsItem::ItemIsMovable);
    connect(_impl->container, SIGNAL(resized(void)), this, SLOT(handleWidgetResized(void)));
    connect(this, SIGNAL(lockedChanged(bool)), _impl->container, SLOT(handleLockedChanged(bool)));
}

GraphWidget::~GraphWidget(void)
{
    return;
}

void GraphWidget::setGraphBlock(GraphBlock *block)
{
    //can only set block once
    assert(_impl);
    assert(not _impl->block);

    _impl->block = block;
    connect(block, SIGNAL(destroyed(QObject *)), this, SLOT(handleBlockDestroyed(QObject *)));
    connect(block, SIGNAL(IDChanged(const QString &)), this, SLOT(handleBlockIdChanged(const QString &)));
    connect(block, SIGNAL(evalDoneEvent(void)), this, SLOT(handleBlockEvalDone(void)));
    this->handleBlockIdChanged(block->getId()); //init value
}

GraphBlock *GraphWidget::getGraphBlock(void) const
{
    return _impl->block;
}

bool GraphWidget::containerHasFocus(void) const
{
    auto fw = _impl->container->focusWidget();
    return fw != nullptr and fw->hasFocus();
}

void GraphWidget::handleBlockDestroyed(QObject *)
{
    //an endpoint was destroyed, schedule for deletion
    //however, the top level code should handle this deletion
    _impl->container->setWidget(nullptr);
    this->flagForDelete();
}

void GraphWidget::handleWidgetResized(void)
{
    auto editor = this->draw()->getGraphEditor();
    editor->handleStateChange(GraphState("transform-scale", tr("Resize %1").arg(this->getId())));
}

void GraphWidget::handleBlockIdChanged(const QString &id)
{
    _impl->container->setGripLabel(id);
}

QPainterPath GraphWidget::shape(void) const
{
    return _impl->graphicsWidget->shape();
}

QVariant GraphWidget::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged)
    {
        _impl->container->setSelected(this->isSelected());
    }

    return QGraphicsItem::itemChange(change, value);
}

void GraphWidget::handleBlockEvalDone(void)
{
    //the widget could have changed when a block eval completes
    auto graphWidget = _impl->block->getGraphWidget();
    auto oldWidget = _impl->container->widget();
    _impl->container->setWidget(graphWidget);

    //no change, ignore logic below
    if (oldWidget == graphWidget) return;

    //clear state info from old widget
    _impl->hasStateInterface = false;

    //inspect the new widget
    if (graphWidget == nullptr) return;
    auto mo = graphWidget->metaObject();
    if (mo->indexOfMethod(QMetaObject::normalizedSignature("saveState(void)").constData()) == -1) return;
    if (mo->indexOfMethod(QMetaObject::normalizedSignature("restoreState(QVariant)").constData()) == -1) return;
    _impl->hasStateInterface = true;

    //restore state after a new widget has been set
    this->restoreWidgetState(_impl->widgetState);
}

/***********************************************************************
 * tie-ins for widget state
 **********************************************************************/

QVariant GraphWidget::saveWidgetState(void) const
{
    QVariant state;
    if (_impl->hasStateInterface)
    {
        QMetaObject::invokeMethod(_impl->container->widget(), "saveState", Qt::DirectConnection, Q_RETURN_ARG(QVariant, state));
    }

    return state;
}

void GraphWidget::restoreWidgetState(const QVariant &state)
{
    if (_impl->hasStateInterface and state.isValid())
    {
        QMetaObject::invokeMethod(_impl->container->widget(), "restoreState", Qt::DirectConnection, Q_ARG(QVariant, state));
    }
}

bool GraphWidget::didWidgetStateChange(void) const
{
    //query the current state
    //declare empty states/not implemented as no change
    auto state = this->saveWidgetState();
    if (not state.isValid()) return false;

    //previous was not valid, don't declare this as changed
    //but stash the current state so we can check next time
    if (not _impl->widgetState.isValid())
    {
        _impl->widgetState = state;
        return false;
    }

    //perform qvariant comparison for change
    return state != _impl->widgetState;
}

/***********************************************************************
 * serialize/deserialize hooks
 **********************************************************************/
QJsonObject GraphWidget::serialize(void) const
{
    auto obj = GraphObject::serialize();
    obj["what"] = QString("Widget");
    obj["blockId"] = _impl->block->getId();
    obj["width"] = _impl->graphicsWidget->size().width();
    obj["height"] = _impl->graphicsWidget->size().height();

    //query the widget state
    auto state = this->saveWidgetState();

    //save the widget state to JSON
    if (state.isValid())
    {
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds << state;
        obj["state"] = QString(data.toBase64());
        _impl->widgetState = state; //stash
    }

    return obj;
}

void GraphWidget::deserialize(const QJsonObject &obj)
{
    auto editor = this->draw()->getGraphEditor();

    //locate the associated block
    if (not _impl->block)
    {
        auto blockId = obj["blockId"].toString();
        auto graphObj = editor->getObjectById(blockId, GRAPH_BLOCK);
        if (graphObj == nullptr) throw Pothos::NotFoundException("GraphWidget::deserialize()", "cant resolve block with ID: '"+blockId.toStdString()+"'");
        auto graphBlock = dynamic_cast<GraphBlock *>(graphObj);
        assert(graphBlock != nullptr);
        this->setGraphBlock(graphBlock);
    }

    if (obj.contains("width") and obj.contains("height"))
    {
        _impl->graphicsWidget->resize(
            obj["width"].toInt(), obj["height"].toInt());
    }

    //restore the widget state from JSON
    if (obj.contains("state"))
    {
        const auto state = obj["state"].toString();
        auto data = QByteArray::fromBase64(state.toUtf8());
        QDataStream ds(&data, QIODevice::ReadOnly);
        ds >> _impl->widgetState;
    }
    else _impl->widgetState.clear();

    //restore the widgets state when there is an active widget
    //otherwise this is also called in handleBlockEvalDone()
    this->restoreWidgetState(_impl->widgetState);

    GraphObject::deserialize(obj);
}
