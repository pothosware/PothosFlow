// Copyright (c) 2013-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphObjects/GraphObject.hpp"
#include "GraphEditor/Constants.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <cassert>
#include <atomic>
#include <iostream>

Q_GLOBAL_STATIC(std::atomic<size_t>, getUIDAtomic)

struct GraphObject::Impl
{
    Impl(void):
        deleteFlag(false),
        uid((*getUIDAtomic())++),
        enabled(true),
        locked(false),
        changed(true),
        canMove(false)
    {
        return;
    }
    QString id;
    bool deleteFlag;
    size_t uid;
    bool enabled;
    bool locked;
    bool changed;
    bool canMove;
    GraphConnectableKey trackedKey;
};

GraphObject::GraphObject(QObject *parent):
    QGraphicsObject(),
    _impl(new Impl())
{
    auto view = qobject_cast<QGraphicsView *>(parent);
    assert(view != nullptr);
    view->scene()->addItem(this);
    this->setFlag(QGraphicsItem::ItemIsSelectable);
}

GraphObject::~GraphObject(void)
{
    return;
}

GraphDraw *GraphObject::draw(void) const
{
    auto draw = qobject_cast<GraphDraw *>(this->scene()->views().at(0));
    assert(draw != nullptr);
    return draw;
}

void GraphObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    this->render(*painter);
}

void GraphObject::setId(const QString &id)
{
    assert(_impl);
    _impl->id = id;
    emit this->IDChanged(id);
}

const QString &GraphObject::getId(void) const
{
    assert(_impl);
    return _impl->id;
}

size_t GraphObject::uid(void) const
{
    assert(_impl);
    return _impl->uid;
}

QRectF GraphObject::boundingRect(void) const
{
    return this->shape().boundingRect();
}

QPainterPath GraphObject::shape(void) const
{
    return QGraphicsItem::shape();
}

void GraphObject::prerender(void)
{
    QImage i0(1, 1, QImage::Format_ARGB32);
    QPainter p0(&i0);
    this->render(p0);
}

void GraphObject::render(QPainter &)
{
    return;
}

void GraphObject::rotateLeft(void)
{
    this->setRotation(int(this->rotation() + 270) % 360);
}

void GraphObject::rotateRight(void)
{
    this->setRotation(int(this->rotation() + 90) % 360);
}

bool GraphObject::isEnabled(void) const
{
    return _impl->enabled;
}

void GraphObject::setEnabled(const bool enb)
{
    if (_impl->enabled == enb) return;
    _impl->enabled = enb;
    this->markChanged();
}

bool GraphObject::isLocked(void) const
{
    return _impl->locked;
}

void GraphObject::setLocked(const bool locked)
{
    if (_impl->locked == locked) return;

    //can move is sticky, track it so we can restore it
    if ((this->flags() & QGraphicsItem::ItemIsMovable) != 0)
    {
        _impl->canMove = true;
    }

    //if movable, modify the setting based on locked
    if (_impl->canMove)
    {
        this->setFlag(QGraphicsItem::ItemIsMovable, not locked);
    }

    this->setFlag(QGraphicsItem::ItemIsSelectable, not locked);

    _impl->locked = locked;
    emit this->lockedChanged(locked);
}

void GraphObject::markChanged(void)
{
    _impl->changed = true;
}

bool GraphObject::isChanged(void) const
{
    return _impl->changed;
}

void GraphObject::clearChanged(void)
{
    _impl->changed = false;
}

std::vector<GraphConnectableKey> GraphObject::getConnectableKeys(void) const
{
    return std::vector<GraphConnectableKey>();
}

GraphConnectableKey GraphObject::isPointingToConnectable(const QPointF &) const
{
    return GraphConnectableKey();
}

GraphConnectableAttrs GraphObject::getConnectableAttrs(const GraphConnectableKey &) const
{
    return GraphConnectableAttrs();
}

bool GraphObject::isFlaggedForDelete(void) const
{
    assert(_impl);
    return _impl->deleteFlag;
}

void GraphObject::flagForDelete(void)
{
    assert(_impl);
    _impl->deleteFlag = true;
    emit this->deleteLater();
}

void GraphObject::renderConnectablePoints(QPainter &painter)
{
    for (const auto &key : this->getConnectableKeys())
    {
        const auto attrs = this->getConnectableAttrs(key);

        //draw circle
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(GraphObjectConnPointColor)));
        painter.drawEllipse(attrs.point, GraphObjectConnPointRadius, GraphObjectConnPointRadius);

        //draw vector
        painter.setPen(QPen(QColor(GraphObjectConnLineColor)));
        QTransform trans; trans.rotate(attrs.rotation-this->rotation());
        painter.drawLine(attrs.point, attrs.point + trans.map(QPointF(GraphObjectConnLineLength, 0)));
    }
}

//A display widget may accept the mouse event, and we wouldnt want to allow a context.
//To support this design expectation, dont use contextMenuEvent, use mousePressEvent.

void GraphObject::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mousePressEvent(event);
    if (event->button() != Qt::RightButton) return;
    //select the current object so the edit menu will be aware of it
    //leave all other objects selected so the menu applies to all selected
    this->setSelected(true);
    auto pos = this->draw()->mapFromScene(this->mapToScene(event->pos()));
    //emit this->draw()->customContextMenuRequested(pos); see below...
    event->accept();

    //enqueue the request so the menu is not invoked from within this event
    //moving objects re-parents them and when done from this context
    //can cause a second context menu to appear after the move is complete
    QMetaObject::invokeMethod(this->draw(), "customContextMenuRequested", Qt::QueuedConnection, Q_ARG(QPoint, pos));
}

void GraphObject::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseDoubleClickEvent(event);
    if (event->button() == Qt::LeftButton and not this->isLocked())
    {
        emit this->draw()->modifyProperties(this);
    }
}

void GraphObject::updateMouseTracking(const QPointF &pos)
{
    const auto newKey = this->isPointingToConnectable(pos);
    if (newKey == _impl->trackedKey) return;
    _impl->trackedKey = newKey;

    //cause re-rendering of the text because we force show hovered port text
    this->markChanged();
    this->update();
}

const GraphConnectableKey &GraphObject::currentTrackedConnectable(void) const
{
    return _impl->trackedKey;
}

QJsonObject GraphObject::serialize(void) const
{
    QJsonObject obj;
    obj["id"] = this->getId();
    obj["zValue"] = this->zValue();
    obj["positionX"] = this->pos().x();
    obj["positionY"] = this->pos().y();
    obj["rotation"] = this->rotation();
    obj["selected"] = this->isSelected();
    obj["enabled"] = this->isEnabled();
    return obj;
}

void GraphObject::deserialize(const QJsonObject &obj)
{
    this->setId(obj["id"].toString());
    this->setZValue(obj["zValue"].toDouble(0.0));
    this->setPos(QPointF(obj["positionX"].toDouble(0.0), obj["positionY"].toDouble(0.0)));
    this->setRotation(obj["rotation"].toDouble(0.0));
    this->setSelected(obj["selected"].toBool(false));
    this->setEnabled(obj["enabled"].toBool(true));
}
