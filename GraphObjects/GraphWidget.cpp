// Copyright (c) 2013-2016 Josh Blum
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
    Impl(GraphWidget *parent):
        container(new GraphWidgetContainer(parent/*reference*/)),
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
Poco::JSON::Object::Ptr GraphWidget::serialize(void) const
{
    auto obj = GraphObject::serialize();
    obj->set("what", std::string("Widget"));
    obj->set("blockId", _impl->block->getId().toStdString());
    obj->set("width", _impl->graphicsWidget->size().width());
    obj->set("height", _impl->graphicsWidget->size().height());

    //query the widget state
    auto state = this->saveWidgetState();

    //save the widget state to JSON
    if (state.isValid())
    {
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds << state;
        data = data.toBase64();
        obj->set("state", std::string(data.data(), data.size()));
        _impl->widgetState = state; //stash
    }

    return obj;
}

void GraphWidget::deserialize(Poco::JSON::Object::Ptr obj)
{
    auto editor = this->draw()->getGraphEditor();

    //locate the associated block
    if (not _impl->block)
    {
        auto blockId = QString::fromStdString(obj->getValue<std::string>("blockId"));
        auto graphObj = editor->getObjectById(blockId, GRAPH_BLOCK);
        if (graphObj == nullptr) throw Pothos::NotFoundException("GraphWidget::deserialize()", "cant resolve block with ID: '"+blockId.toStdString()+"'");
        auto graphBlock = dynamic_cast<GraphBlock *>(graphObj);
        assert(graphBlock != nullptr);
        this->setGraphBlock(graphBlock);
    }

    if (obj->has("width") and obj->has("height"))
    {
        _impl->graphicsWidget->resize(
            obj->getValue<int>("width"),
            obj->getValue<int>("height"));
    }

    //restore the widget state from JSON
    const auto state = obj->optValue<std::string>("state", "");
    if (state.empty()) _impl->widgetState.clear();
    else
    {
        auto data = QByteArray(state.data(), state.size());
        data = QByteArray::fromBase64(data);
        QDataStream ds(&data, QIODevice::ReadOnly);
        ds >> _impl->widgetState;
    }

    //restore the widgets state when there is an active widget
    //otherwise this is also called in handleBlockEvalDone()
    this->restoreWidgetState(_impl->widgetState);

    GraphObject::deserialize(obj);
}
