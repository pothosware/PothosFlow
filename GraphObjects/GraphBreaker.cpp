// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphObjects/GraphBreaker.hpp"
#include "GraphEditor/Constants.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QStaticText>
#include <vector>
#include <iostream>
#include <cassert>

struct GraphBreaker::Impl
{
    Impl(void):
        isInput(true),
        changed(true)
    {
        return;
    }
    bool isInput;
    bool changed;
    QString nodeName;
    QStaticText titleText;
    QPolygonF polygon;
    QRectF connectRect;
    QPointF connectPoint;
};

GraphBreaker::GraphBreaker(QObject *parent):
    GraphObject(parent),
    _impl(new Impl())
{
    this->setFlag(QGraphicsItem::ItemIsMovable);
}

void GraphBreaker::setInput(const bool isInput)
{
    assert(_impl);
    _impl->isInput = isInput;
}

bool GraphBreaker::isInput(void) const
{
    assert(_impl);
    return _impl->isInput;
}

void GraphBreaker::setNodeName(const QString &name)
{
    assert(_impl);
    _impl->nodeName = name;
    _impl->changed = true;
}

const QString &GraphBreaker::getNodeName(void) const
{
    assert(_impl);
    return _impl->nodeName;
}

QPainterPath GraphBreaker::shape(void) const
{
    QPainterPath path;
    path.addPolygon(_impl->polygon);
    return path;
}

std::vector<GraphConnectableKey> GraphBreaker::getConnectableKeys(void) const
{
    std::vector<GraphConnectableKey> keys;
    keys.push_back(GraphConnectableKey("0", this->isInput()?GRAPH_CONN_INPUT:GRAPH_CONN_OUTPUT));
    return keys;
}

GraphConnectableKey GraphBreaker::isPointingToConnectable(const QPointF &pos) const
{
    assert(_impl);
    GraphConnectableKey key("", this->isInput()?GRAPH_CONN_INPUT:GRAPH_CONN_OUTPUT);
    if (_impl->connectRect.contains(pos)) key.id = "0";
    return key;
}

GraphConnectableAttrs GraphBreaker::getConnectableAttrs(const GraphConnectableKey &) const
{
    assert(_impl);
    GraphConnectableAttrs attrs;
    attrs.rotation = this->rotation();
    if (this->isInput()) attrs.rotation += 180;
    attrs.direction = this->isInput()?GRAPH_CONN_INPUT:GRAPH_CONN_OUTPUT;
    attrs.point = _impl->connectPoint;
    return attrs;
}

void GraphBreaker::renderStaticText(void)
{
    assert(_impl);
    _impl->titleText = QStaticText(QString("<span style='font-size:%1;'><b>%2</b></span>")
        .arg(GraphBreakerTitleFontSize)
        .arg(_impl->nodeName.toHtmlEscaped()));
}

void GraphBreaker::render(QPainter &painter)
{
    assert(_impl);
    //render text
    if (_impl->changed)
    {
        _impl->changed = false;
        this->renderStaticText();
    }

    //setup rotations and translations
    QTransform trans;

    //dont rotate past 180 because we just do a breaker flip
    //this way text only ever has 2 rotations
    const bool breakerFlip = this->rotation() >= 180;
    if (breakerFlip) painter.rotate(-180);
    if (breakerFlip) trans.rotate(-180);

    //set painter for drawing the figure
    auto pen = QPen(QColor(GraphObjectDefaultPenColor));
    pen.setWidthF(GraphObjectBorderWidth);
    painter.setPen(pen);
    painter.setBrush(QBrush(QColor(this->isEnabled()?GraphObjectDefaultFillColor:GraphBlockDisabledColor)));

    qreal w = _impl->titleText.size().width() + 2*GraphBreakerTitleHPad;
    qreal h = _impl->titleText.size().height() + 2*GraphBreakerTitleVPad;

    QPolygonF polygon;
    const bool flipStyle = (this->isInput() and not breakerFlip) or (not this->isInput() and breakerFlip);
    if (flipStyle)
    {
        polygon << QPointF(0, 0);
        for (int i = 0; i <= 6; i++)
        {
            const qreal jut = ((i % 2) == (this->isInput()?0:1))? 0 : GraphBreakerEdgeJut;
            polygon << QPointF(w+jut, h*(i/6.));
        }
        polygon << QPointF(0, h);
    }
    else
    {
        polygon << QPointF(0, 0);
        polygon << QPointF(w, 0);
        polygon << QPointF(w, h);
        for (int i = 6; i >= 0; i--)
        {
            const qreal jut = ((i % 2) == (this->isInput()?0:1))? 0 : GraphBreakerEdgeJut;
            polygon << QPointF(-jut, h*(i/6.));
        }
    }

    QPointF p(-w/2, -h/2);
    polygon.translate(p);
    painter.save();
    if (isSelected()) painter.setPen(QColor(GraphObjectHighlightPenColor));

    //connection mode when the tracked endpoint is the oposing direction of the last clicked endpoint
    const auto &trackedKey = this->currentTrackedConnectable();
    const auto &clickedEp = this->draw()->lastClickedEndpoint();
    if (trackedKey.isValid() and clickedEp.isValid() and this->isInput() != clickedEp.getKey().isInput())
    {
        painter.setPen(QPen(QColor(ConnectModeHighlightPenColor), ConnectModeHighlightWidth));
    }

    painter.drawPolygon(polygon);
    painter.restore();
    _impl->polygon = trans.map(polygon);

    const auto textOff = QPointF(GraphBreakerTitleHPad, GraphBreakerTitleVPad);
    painter.drawStaticText(p+textOff, _impl->titleText);

    //connectable region (the first third)
    const QRectF connectRect(
        QPointF(flipStyle?0:w, 0) + p,
        QPointF(flipStyle?w/3:(2*w)/3, h) + p);
    _impl->connectRect = trans.mapRect(connectRect);

    //connection point
    const auto connectionPoint = QPointF(flipStyle?-GraphObjectBorderWidth:w+GraphObjectBorderWidth, h/2) + p;
    _impl->connectPoint = trans.map(connectionPoint);
}

QJsonObject GraphBreaker::serialize(void) const
{
    auto obj = GraphObject::serialize();
    obj["what"] = "Breaker";
    obj["nodeName"] = this->getNodeName();
    obj["isInput"] = this->isInput();
    return obj;
}

void GraphBreaker::deserialize(const QJsonObject &obj)
{
    this->setInput(obj["isInput"].toBool());
    this->setNodeName(obj["nodeName"].toString());

    GraphObject::deserialize(obj);
}
