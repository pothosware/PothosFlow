// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "PothosGuiUtils.hpp" //action maps
#include "GraphEditor/GraphDraw.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include "GraphEditor/Constants.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphObjects/GraphBreaker.hpp"
#include "GraphObjects/GraphConnection.hpp"
#include "GraphObjects/GraphWidget.hpp"
#include <Pothos/Exception.hpp>
#include <Poco/Logger.h>
#include <QApplication> //control modifier
#include <QGraphicsLineItem>
#include <QMouseEvent>
#include <QAction>
#include <QMenu>
#include <QScrollBar>
#include <iostream>
#include <algorithm>

static const int SELECTION_STATE_NONE = 0;
static const int SELECTION_STATE_PRESS = 1;
static const int SELECTION_STATE_MOVE = 2;

void GraphDraw::clearSelectionState(void)
{
    _selectionState = SELECTION_STATE_NONE;
    _connectLineItem.reset();
    _connectModeImmobilizer.reset();
}

void GraphDraw::contextMenuEvent(QContextMenuEvent *event)
{
    QGraphicsView::contextMenuEvent(event);
    //context menu when nothing selected
    const auto objs = this->getObjectsAtPos(event->pos());
    if (objs.empty()) this->customContextMenuRequested(event->pos());
}

void GraphDraw::wheelEvent(QWheelEvent *event)
{
    const bool ctrlDown = QApplication::keyboardModifiers() & Qt::ControlModifier;
    if (not ctrlDown) return QGraphicsView::wheelEvent(event);

    //ctrl was down, wheel event means zoom in or out:
    if (event->delta() > 0) getActionMap()["zoomIn"]->activate(QAction::Trigger);
    if (event->delta() < 0) getActionMap()["zoomOut"]->activate(QAction::Trigger);
}

void GraphDraw::mousePressEvent(QMouseEvent *event)
{
    QGraphicsView::mousePressEvent(event);
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) return;

    //record the conditions of this press event, nothing is changed
    if (event->button() == Qt::LeftButton)
    {
        _selectionState = SELECTION_STATE_PRESS;

        const auto objs = this->getObjectsAtPos(event->pos());
        if (objs.empty())
        {
            //perform deselect when background is clicked
            _lastClickSelectEp = GraphConnectionEndpoint();
            this->deselectAllObjs();
        }
        else
        {
            //make the clicked object topmost
            objs.front()->setZValue(this->getMaxZValue()+1);
        }

        //handle click selection for connections
        const auto thisEp = this->mousedEndpoint(event->pos());

        //enter the connect drag mode and object immobilization
        //slots are exempt because they are the block's body
        if (thisEp.isValid())
        {
            auto topObj = thisEp.getObj();
            if (thisEp.getKey().direction != GRAPH_CONN_SLOT)
            {
                _connectLineItem.reset(new QGraphicsLineItem(topObj));
                _connectLineItem->setPen(QPen(QColor(GraphObjectDefaultPenColor), ConnectModeLineWidth));
                _connectModeImmobilizer.reset(new GraphObjectImmobilizer(topObj));
            }

            //if separate clicks to connect when try to make connection
            if (getActionMap()["clickConnectMode"]->isChecked())
            {
                if (not this->tryToMakeConnection(thisEp))
                {
                    //stash this endpoint when connection is not made
                    _lastClickSelectEp = thisEp;
                }
            }

            //stash the connection endpoint for a drag+release connection
            else _lastClickSelectEp = thisEp;
        }
    }

    this->render();
}

void GraphDraw::mouseDoubleClickEvent(QMouseEvent *event)
{
    QGraphicsView::mouseDoubleClickEvent(event);

    //double clicked on graph to edit graph properties when nothing selected
    const auto objs = this->getObjectsAtPos(event->pos());
    if (objs.empty()) emit this->modifyProperties(this->getGraphEditor());
}

static void handleAutoScroll(QScrollBar *bar, const qreal length, const qreal offset)
{
    const qreal delta = offset - bar->value();
    if (delta + GraphDrawScrollFudge > length)
    {
        bar->setValue(std::min(bar->maximum(), int(bar->value() + (delta + GraphDrawScrollFudge - length)/2)));
    }
    if (delta - GraphDrawScrollFudge < 0)
    {
        bar->setValue(std::max(0, int(bar->value() + (delta - GraphDrawScrollFudge)/2)));
    }
}

void GraphDraw::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);

    //implement mouse tracking for blocks
    const auto scenePos = this->mapToScene(event->pos());
    for (auto obj : this->getGraphObjects())
    {
        obj->updateMouseTracking(obj->mapFromParent(scenePos));
    }

    //handle drawing in the click, drag, connect mode
    const auto topObj = _lastClickSelectEp.getObj();
    if (_connectLineItem and topObj)
    {
        const auto attrs = topObj->getConnectableAttrs(_lastClickSelectEp.getKey());
        const auto newPos = topObj->mapFromParent(scenePos);
        _connectLineItem->setLine(QLineF(attrs.point, newPos));
    }

    //handle the first move event transition from a press event
    if (_selectionState == SELECTION_STATE_PRESS)
    {
        _selectionState = SELECTION_STATE_MOVE;

        //record positions to determine movement on release
        _preMovePositions.clear();
        for (auto obj : getObjectsSelected(~GRAPH_CONNECTION))
        {
            _preMovePositions[obj] = obj->pos();
        }
    }

    //cause full render when moving objects for clean animation
    if (_selectionState == SELECTION_STATE_MOVE) this->render();

    //auto scroll near boundaries
    if (_selectionState != SELECTION_STATE_NONE)
    {
        handleAutoScroll(this->horizontalScrollBar(), this->size().width(), this->mapToScene(event->pos()).x());
        handleAutoScroll(this->verticalScrollBar(), this->size().height(), this->mapToScene(event->pos()).y());
    }
}

void GraphDraw::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) return;

    //releasing the connection line to create
    if (_connectLineItem)
    {
        const auto thisEp = this->mousedEndpoint(event->pos());
        this->tryToMakeConnection(thisEp);
        if (not getActionMap()["clickConnectMode"]->isChecked())
            _lastClickSelectEp = GraphConnectionEndpoint();
    }

    //emit the move event up to the graph editor
    if (_selectionState == SELECTION_STATE_MOVE)
    {
        bool moved = false;
        for (const auto &pair : _preMovePositions)
        {
            if (pair.first->pos() != pair.second) moved = true;
        }
        if (moved) this->getGraphEditor()->handleStateChange(
            GraphState("transform-move", tr("Move %1").arg(this->getSelectionDescription(~GRAPH_CONNECTION))));
    }

    this->clearSelectionState();
    this->render();
}

void GraphDraw::deselectAllObjs(void)
{
    for (auto obj : this->scene()->selectedItems())
    {
        obj->setSelected(false);
    }
}

qreal GraphDraw::getMaxZValue(void)
{
    qreal index = 0;
    bool first = true;
    for (auto obj : this->getGraphObjects())
    {
        if (first) index = obj->zValue();
        index = std::max(index, obj->zValue());
        first = false;
    }
    return index;
}

GraphObjectList GraphDraw::getObjectsSelected(const int selectionFlags)
{
    GraphObjectList objectsSelected;
    for (auto obj : this->getGraphObjects(selectionFlags))
    {
        if (obj->isSelected()) objectsSelected.push_back(obj);
    }
    return objectsSelected;
}

GraphObjectList GraphDraw::getGraphObjects(const int selectionFlags)
{
    GraphObjectList l;
    for (auto child : this->items())
    {
        auto o = dynamic_cast<GraphObject *>(child);
        if (o == nullptr) continue;
        if (((selectionFlags & GRAPH_BLOCK) != 0) and (dynamic_cast<GraphBlock *>(o) != nullptr)) l.push_back(o);
        if (((selectionFlags & GRAPH_BREAKER) != 0) and (dynamic_cast<GraphBreaker *>(o) != nullptr)) l.push_back(o);
        if (((selectionFlags & GRAPH_CONNECTION) != 0) and (dynamic_cast<GraphConnection *>(o) != nullptr)) l.push_back(o);
        if (((selectionFlags & GRAPH_WIDGET) != 0) and (dynamic_cast<GraphWidget *>(o) != nullptr)) l.push_back(o);
    }
    return l;
}

bool GraphDraw::graphWidgetHasFocus(void)
{
    for (auto obj : this->getGraphObjects(GRAPH_WIDGET))
    {
        auto widget = dynamic_cast<GraphWidget *>(obj);
        assert(widget != nullptr);
        if (widget->containerHasFocus()) return true;
    }
    return false;
}

GraphConnectionEndpoint GraphDraw::mousedEndpoint(const QPoint &pos)
{
    auto objs = this->items(pos);
    if (_connectLineItem) objs.removeOne(_connectLineItem.get());
    if (objs.empty()) return GraphConnectionEndpoint();
    auto topObj = dynamic_cast<GraphObject *>(objs.front());
    if (topObj == nullptr) return GraphConnectionEndpoint();
    const auto point = topObj->mapFromParent(this->mapToScene(pos));
    return GraphConnectionEndpoint(topObj, topObj->isPointingToConnectable(point));
}

bool GraphDraw::tryToMakeConnection(const GraphConnectionEndpoint &thisEp)
{
    QPointer<GraphConnection> conn;
    const auto &lastEp = _lastClickSelectEp;

    //valid keys, attempt to make a connection when the endpoints differ in direction
    if (thisEp.isValid() and lastEp.isValid() and //both are valid
        lastEp.getKey().isInput() != thisEp.getKey().isInput()) //directions differ
    {
        try
        {
            conn = this->getGraphEditor()->makeConnection(thisEp, _lastClickSelectEp);
            this->getGraphEditor()->handleStateChange(GraphState("connect-arrow", tr("Connect %1[%2] to %3[%4]").arg(
                conn->getOutputEndpoint().getObj()->getId(),
                conn->getOutputEndpoint().getKey().id,
                conn->getInputEndpoint().getObj()->getId(),
                conn->getInputEndpoint().getKey().id
            )));
        }
        catch (const Pothos::Exception &ex)
        {
            poco_warning(Poco::Logger::get("PothosGui.GraphDraw.connect"), Poco::format("Cannot connect port %s[%s] to port %s[%s]: %s",
                _lastClickSelectEp.getObj()->getId().toStdString(),
                _lastClickSelectEp.getKey().id.toStdString(),
                thisEp.getObj()->getId().toStdString(),
                thisEp.getKey().id.toStdString(),
                ex.message()));
        }

        //cleanup regardless of failure
        _lastClickSelectEp = GraphConnectionEndpoint();
        this->deselectAllObjs();
    }

    //if this is a signal or slot connection
    //open the properties panel for configuration
    if (conn and conn->isSignalOrSlot())
    {
        emit this->modifyProperties(conn);
    }

    return bool(conn);
}

GraphObjectList GraphDraw::getObjectsAtPos(const QPoint &pos)
{
    GraphObjectList graphObjs;
    const auto objs = this->items(pos);
    for (auto obj : objs)
    {
        auto graphObj = dynamic_cast<GraphObject *>(obj);
        if (graphObj != nullptr) graphObjs.push_back(graphObj);
    }
    return graphObjs;
}

QString GraphDraw::getSelectionDescription(const int selectionFlags)
{
    //generate names based on the selected objects
    const auto selected = this->getObjectsSelected(selectionFlags);
    if (selected.isEmpty()) return tr("no selection");

    //if a single connection is selected, pretty print its endpoint IDs
    if (selected.size() == 1)
    {
        auto conn = dynamic_cast<GraphConnection *>(selected.at(0));
        if (conn == nullptr) return selected.at(0)->getId();
        return tr("%1[%2] to %3[%4]").arg(
            conn->getOutputEndpoint().getObj()->getId(),
            conn->getOutputEndpoint().getKey().id,
            conn->getInputEndpoint().getObj()->getId(),
            conn->getInputEndpoint().getKey().id
        );
    }
    return tr("selected");
}

GraphObject *GraphDraw::getObjectById(const QString &id, const int selectionFlags)
{
    for (auto obj : this->getGraphObjects(selectionFlags))
    {
        if (obj->getId() == id) return obj;
    }
    return nullptr;
}
