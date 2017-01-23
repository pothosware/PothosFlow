// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/GraphDraw.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include "GraphObjects/GraphBreaker.hpp"
#include "GraphObjects/GraphConnection.hpp"
#include "GraphEditor/Constants.hpp"
#include "PropertiesPanel/PropertiesPanelDock.hpp"
#include "MainWindow/MainActions.hpp"
#include "MainWindow/MainMenu.hpp"
#include <Poco/JSON/Parser.h>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QAction>
#include <QChildEvent>
#include <QScrollBar>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <iostream>
#include <cassert>
#include <limits>
#include <algorithm> //min/max

GraphDraw::GraphDraw(QWidget *parent):
    QGraphicsView(parent),
    _graphEditor(dynamic_cast<GraphEditor *>(parent)),
    _zoomScale(1.0),
    _selectionState(0)
{
    //setup scene
    this->setScene(new QGraphicsScene(QRectF(QPointF(), GraphDrawCanvasSize), this));
    //required: BspTreeIndex is too smart for its own good, connections will not render properly
    this->scene()->setItemIndexMethod(QGraphicsScene::NoIndex);
    this->scene()->setBackgroundBrush(QColor(GraphDrawBackgroundColor));
    this->setDragMode(QGraphicsView::RubberBandDrag);
    this->ensureVisible(QRectF()); //set scrolls to 0, 0 position

    //set high quality rendering
    this->setRenderHint(QPainter::Antialiasing);
    this->setRenderHint(QPainter::HighQualityAntialiasing);
    this->setRenderHint(QPainter::SmoothPixmapTransform);

    //init settings
    assert(this->getGraphEditor() != nullptr);
    this->setMouseTracking(true);
    this->setAcceptDrops(true);
    this->setZoomScale(1.0);
    this->clearSelectionState();
    this->setFocusPolicy(Qt::ClickFocus);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)),
        this, SLOT(handleCustomContextMenuRequested(const QPoint &)));
    connect(this, SIGNAL(modifyProperties(QObject *)),
        PropertiesPanelDock::global(), SLOT(launchEditor(QObject *)));
    connect(this->scene(), SIGNAL(selectionChanged(void)), this, SLOT(updateEnabledActions(void)));

    //debug view - connect and initialize
    auto actions = MainActions::global();
    connect(actions->showGraphConnectionPointsAction, SIGNAL(triggered(void)),
        this, SLOT(handleGraphDebugViewChange(void)));
    connect(actions->showGraphBoundingBoxesAction, SIGNAL(triggered(void)),
        this, SLOT(handleGraphDebugViewChange(void)));
    this->handleGraphDebugViewChange();
}

void GraphDraw::handleGraphDebugViewChange(void)
{
    auto actions = MainActions::global();
    _graphConnectionPoints.reset();
    if (actions->showGraphConnectionPointsAction->isChecked())
    {
        _graphConnectionPoints.reset(new QGraphicsPixmapItem());
        this->scene()->addItem(_graphConnectionPoints.get());
    }

    _graphBoundingBoxes.reset();
    if (actions->showGraphBoundingBoxesAction->isChecked())
    {
        _graphBoundingBoxes.reset(new QGraphicsPixmapItem());
        this->scene()->addItem(_graphBoundingBoxes.get());
    }

    if (not this->isVisible()) return;
    this->render();
}

void GraphDraw::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/json/pothos_block") and
        not event->mimeData()->data("text/json/pothos_block").isEmpty())
    {
        event->acceptProposedAction();
    }
    else QGraphicsView::dragEnterEvent(event);
}

void GraphDraw::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
}

void GraphDraw::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("text/json/pothos_block") and
        not event->mimeData()->data("text/json/pothos_block").isEmpty())
    {
        event->acceptProposedAction();
    }
    else QGraphicsView::dragMoveEvent(event);
}

void GraphDraw::dropEvent(QDropEvent *event)
{
    const auto byteArray = event->mimeData()->data("text/json/pothos_block");
    const auto result = Poco::JSON::Parser().parse(std::string(byteArray.constData(), byteArray.size()));
    const auto blockDesc = result.extract<Poco::JSON::Object::Ptr>();

    this->getGraphEditor()->handleAddBlock(blockDesc, this->mapToScene(event->pos()));
    event->acceptProposedAction();
}

void GraphDraw::setZoomScale(const qreal zoom)
{
    //stash the old coordinate
    auto mousePos = this->mapFromGlobal(QCursor::pos());
    const auto p0 = this->mapToScene(mousePos);

    //perform the zoom
    _zoomScale = zoom;
    this->setTransform(QTransform()); //clear
    this->scale(this->zoomScale(), this->zoomScale());
    this->render();

    //calculate the scroll movement
    const auto p1 = this->mapToScene(mousePos);
    if (this->rect().contains(mousePos))
    {
        this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value()-p1.x()+p0.x());
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->value()-p1.y()+p0.y());
    }
}

void GraphDraw::showEvent(QShowEvent *event)
{
    this->updateEnabledActions();
    emit this->modifyProperties(nullptr); //resets the state of whoever is modding the properties
    this->render();
    QGraphicsView::showEvent(event);
}

void GraphDraw::keyPressEvent(QKeyEvent *event)
{
    auto actions = MainActions::global();

    //map a key-press to an action name
    QAction *action = nullptr;
    switch(event->key())
    {
    case Qt::Key_Plus:     action = actions->incrementAction; break;
    case Qt::Key_Minus:    action = actions->decrementAction; break;
    case Qt::Key_Return:   action = actions->objectPropertiesAction; break;
    case Qt::Key_E:        action = actions->enableAction; break;
    case Qt::Key_D:        action = actions->disableAction; break;
    case Qt::Key_R:        action = actions->reevalAction; break;
    case Qt::Key_Left:     action = actions->rotateLeftAction; break;
    case Qt::Key_Right:    action = actions->rotateRightAction; break;
    }

    //default action is taken when nothing is selected
    //or a graph widget has the focus or unmatched key.
    if (
        this->getObjectsSelected().isEmpty() or
        this->graphWidgetHasFocus() or action == nullptr)
    {
        QGraphicsView::keyPressEvent(event);
    }

    //otherwise trigger the action
    else
    {
        action->activate(QAction::Trigger);
    }
}

void GraphDraw::updateEnabledActions(void)
{
    auto selectedObjsNoC = this->getObjectsSelected(~GRAPH_CONNECTION);
    const bool selectedNoC = not selectedObjsNoC.empty();

    auto selectedObjs = this->getObjectsSelected();
    const bool selected = not selectedObjs.empty();

    auto selectedObjBlocks = this->getObjectsSelected(GRAPH_BLOCK);
    const bool selectedBlocks = not selectedObjBlocks.empty();

    auto actions = MainActions::global();
    auto mainMenu = MainMenu::global();
    actions->cutAction->setEnabled(selectedNoC);
    actions->copyAction->setEnabled(selectedNoC);
    actions->deleteAction->setEnabled(selected);
    actions->rotateLeftAction->setEnabled(selectedNoC);
    actions->rotateRightAction->setEnabled(selectedNoC);
    actions->objectPropertiesAction->setEnabled(selected);
    actions->incrementAction->setEnabled(selectedBlocks);
    actions->decrementAction->setEnabled(selectedBlocks);
    actions->enableAction->setEnabled(selected);
    actions->disableAction->setEnabled(selected);
    actions->reevalAction->setEnabled(selectedBlocks);
    mainMenu->affinityZoneMenu->setEnabled(selectedBlocks);

    //and enable/disable the actions in the move graph objects submenu
    for (auto child : mainMenu->moveGraphObjectsMenu->children())
    {
        auto action = dynamic_cast<QAction *>(child);
        if (action != nullptr) action->setEnabled(selectedNoC);
    }
}

void GraphDraw::render(void)
{
    if (not this->isVisible()) return;

    //pre-render to perform connection calculations
    const auto allObjs = this->getGraphObjects();
    for (auto obj : allObjs) obj->prerender();

    //clip the bounds
    for (auto obj : allObjs)
    {
        auto oldPos = obj->pos();
        oldPos.setX(std::min(std::max(oldPos.x(), 0.0), this->sceneRect().width()-obj->boundingRect().width()));
        oldPos.setY(std::min(std::max(oldPos.y(), 0.0), this->sceneRect().height()-obj->boundingRect().height()));
        obj->setPos(oldPos);
    }

    //optional debug pixmap overlay for connection points
    if (_graphConnectionPoints)
    {
        QPixmap pixmap(this->sceneRect().size().toSize());
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        for (auto obj : allObjs)
        {
            painter.save();
            painter.translate(obj->pos());
            painter.rotate(obj->rotation());
            obj->renderConnectablePoints(painter);
            painter.restore();
        }
        _graphConnectionPoints->setPixmap(pixmap);
        _graphConnectionPoints->setZValue(std::numeric_limits<qreal>::max());
    }

    //optional debug pixmap overlay for bounding boxes
    if (_graphBoundingBoxes)
    {
        QPixmap pixmap(this->sceneRect().size().toSize());
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        for (auto obj : allObjs)
        {
            painter.save();
            painter.translate(obj->pos());
            painter.rotate(obj->rotation());
            painter.setPen(QPen(Qt::red));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(obj->shape());
            painter.restore();
        }
        _graphBoundingBoxes->setPixmap(pixmap);
        _graphBoundingBoxes->setZValue(std::numeric_limits<qreal>::max());
    }

    //cause full redraw
    this->scene()->update();
    this->repaint();
}

void GraphDraw::handleCustomContextMenuRequested(const QPoint &pos)
{
    auto mainMenu = MainMenu::global();
    _lastContextMenuPos = this->mapToScene(pos);
    mainMenu->editMenu->exec(this->mapToGlobal(pos));
}
