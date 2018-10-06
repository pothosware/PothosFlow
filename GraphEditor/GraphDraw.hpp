// Copyright (c) 2013-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "GraphObjects/GraphObject.hpp"
#include <QGraphicsView>
#include <memory>
#include <map>

class GraphEditor;
class QGraphicsItem;
class QGraphicsPixmapItem;
class QGraphicsLineItem;

class GraphDraw : public QGraphicsView
{
    Q_OBJECT
public:
    GraphDraw(QWidget *parent);

    GraphObjectList getObjectsSelected(const int selectionFlags = ~0);

    GraphObjectList getGraphObjects(const int selectionFlags = ~0);

    //! True if any graph widget has focus
    bool graphWidgetHasFocus(void);

    /*!
     * Describe the selected objects in words.
     * This will be used with the event reporting.
     * A single object selected? use the ID.
     * Otherwise, just report "selections".
     */
    QString getSelectionDescription(const int selectionFlags = ~0);

    //! Get the graph object with the specified ID or nullptr
    GraphObject *getObjectById(const QString &id, const int selectionFlags = ~0);

    GraphEditor *getGraphEditor(void) const
    {
        return _graphEditor;
    }

    void render(void);

    qreal zoomScale(void) const
    {
        return _zoomScale;
    }

    void setZoomScale(const qreal zoom);

    QPointF getLastContextMenuPos(void) const
    {
        return _lastContextMenuPos;
    }

    //! get the largest z value of all objects
    qreal getMaxZValue(void);

    //! unselect all selected objects
    void deselectAllObjs(void);

    /*!
     * The GraphDraw maintains a selection state to detect drag events.
     * Calling this method will clear that selection state to stop a move.
     */
    void clearSelectionState(void);

    //! Get a list of graph objects at the given point
    GraphObjectList getObjectsAtPos(const QPoint &pos);

    /*!
     * Get the last endpoint that was clicked.
     * This is used by the blocks to determine connect mode highlighting.
     */
    const GraphConnectionEndpoint &lastClickedEndpoint(void) const
    {
        return _lastClickSelectEp;
    }

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void showEvent(QShowEvent *event);
    void keyPressEvent(QKeyEvent *event);

private slots:
    void handleCustomContextMenuRequested(const QPoint &);
    void handleGraphDebugViewChange(void);
    void updateEnabledActions(void);

signals:
    void modifyProperties(QObject *);

private:

    GraphConnectionEndpoint mousedEndpoint(const QPoint &);
    bool tryToMakeConnection(const GraphConnectionEndpoint &thisEp);

    GraphEditor *_graphEditor;
    qreal _zoomScale;
    int _selectionState;
    QPointF _lastContextMenuPos;
    GraphConnectionEndpoint _lastClickSelectEp;
    std::map<GraphObject *, QPointF> _preMovePositions;

    std::unique_ptr<QGraphicsPixmapItem> _graphConnectionPoints;
    std::unique_ptr<QGraphicsPixmapItem> _graphBoundingBoxes;
    std::unique_ptr<QGraphicsLineItem> _connectLineItem;
    std::unique_ptr<GraphObjectImmobilizer> _connectModeImmobilizer;
};
