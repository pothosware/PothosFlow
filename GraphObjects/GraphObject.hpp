// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "GraphObjects/GraphEndpoint.hpp"
#include <QList>
#include <QGraphicsObject>
#include <QPointF>
#include <QRectF>
#include <vector>
#include <memory>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>

class QObject;
class QPainter;
class GraphObject;
class GraphDraw;

//! Represent a list of graph objects
typedef QList<GraphObject *> GraphObjectList;

//! Base class for graph objects
class GraphObject : public QGraphicsObject
{
    Q_OBJECT
public:
    GraphObject(QObject *parent);

    ~GraphObject(void);

    GraphDraw *draw(void) const;

    virtual QRectF boundingRect(void) const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);
    virtual QPainterPath shape(void) const;

    virtual void setId(const QString &id);
    const QString &getId(void) const;

    /*!
     * A process-wide unique ID per object.
     * This is used by the evaluator to identify objects.
     * Pointers get reused and are therfore not reliable.
     */
    size_t uid(void) const;

    //! render without a painter to do-precalculations
    void prerender(void);
    virtual void render(QPainter &painter);

    void rotateLeft(void);
    void rotateRight(void);

    bool isEnabled(void) const;
    void setEnabled(const bool enb);

    //! Called internally or externally to indicate property changes
    void markChanged(void);

    //! Has change been marked on this object?
    bool isChanged(void) const;

    //! Clear the changed state (called after handling change)
    void clearChanged(void);

    //! empty string when not pointing, otherwise connectable key
    virtual std::vector<GraphConnectableKey> getConnectableKeys(void) const;
    virtual GraphConnectableKey isPointingToConnectable(const QPointF &pos) const;
    virtual GraphConnectableAttrs getConnectableAttrs(const GraphConnectableKey &key) const;
    virtual void renderConnectablePoints(QPainter &painter);

    //! Get the current key tracked by the mouse
    virtual const GraphConnectableKey &currentTrackedConnectable(void) const;

    bool isFlaggedForDelete(void) const;
    void flagForDelete(void);

    virtual Poco::JSON::Object::Ptr serialize(void) const;

    virtual void deserialize(Poco::JSON::Object::Ptr obj);

signals:
    void IDChanged(const QString &);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);

    /*!
     * Called by the graph draw class to handle mouse tracking.
     * The position is relative to this graph object's reference.
     * This is a manual replacement for mouseMoveEvent(),
     * which was not being called in the graph objects.
     */
    virtual void updateMouseTracking(const QPointF &pos);

    friend class GraphDraw;
private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
};

/*!
 * Immobilize a graph object on construction.
 * And automatically restore on deletion.
 */
struct GraphObjectImmobilizer
{
    template<typename ObjectType>
    GraphObjectImmobilizer(const ObjectType &obj):
        obj(obj),
        flags(obj->flags())
    {
        obj->setFlag(QGraphicsItem::ItemIsMovable, false);
    }

    ~GraphObjectImmobilizer(void)
    {
        if (obj) obj->setFlags(flags);
    }

    QPointer<QGraphicsObject> obj;
    QGraphicsItem::GraphicsItemFlags flags;
};
