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
#include <QMetaObject>
#include <QMetaMethod>
#include <QJsonArray>
#include <QJsonObject>
#include <vector>
#include <iostream>
#include <cassert>
#include <Pothos/Util/TypeInfo.hpp>
#include <Poco/Logger.h>

/***********************************************************************
 * GraphWidget private container
 **********************************************************************/
struct GraphWidget::Impl
{
    Impl(QGraphicsItem *parent):
        container(new GraphWidgetContainer()),
        graphicsWidget(new QGraphicsProxyWidget(parent))
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
    QMetaObject::Connection stateChangedConnection;
    QMetaObject::Connection stateRestoreConnection;
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

    //disconnect old widget
    if (_impl->stateChangedConnection)
    {
        this->disconnect(_impl->stateChangedConnection);
        _impl->stateChangedConnection = QMetaObject::Connection();
    }
    if (_impl->stateRestoreConnection)
    {
        this->disconnect(_impl->stateRestoreConnection);
        _impl->stateRestoreConnection = QMetaObject::Connection();
    }

    //connect new widget
    if (graphWidget == nullptr) return;
    auto mo = graphWidget->metaObject();
    if (mo->indexOfSignal(QMetaObject::normalizedSignature("stateChanged(QVariant)").constData()) == -1) return;
    if (mo->indexOfSlot(QMetaObject::normalizedSignature("restoreState(QVariant)").constData()) == -1) return;
    _impl->stateChangedConnection = this->connect(graphWidget, SIGNAL(stateChanged(const QVariant &)), this, SLOT(handleWidgetStateChanged(const QVariant &)));
    _impl->stateRestoreConnection = this->connect(this, SIGNAL(restoreWidgetState(const QVariant &)), graphWidget, SLOT(restoreState(const QVariant &)));
    emit this->restoreWidgetState(_impl->widgetState); //emit state to new widget (also done in deserialize)
}

void GraphWidget::handleWidgetStateChanged(const QVariant &state)
{
    //stash the state -- will actually be saved by serialize
    _impl->widgetState = state;
}

/***********************************************************************
 * Hooks between Poco JSON and QVariant
 **********************************************************************/
static Poco::Dynamic::Var QVariantToJsonState(const QVariant &state, std::string *stateType = nullptr)
{
    if (not state.isValid()) return Poco::Dynamic::Var();

    //binary data -- only for top level element
    if (state.type() == qMetaTypeId<QByteArray>() and stateType != nullptr)
    {
        *stateType = "binary";
        return state.toByteArray().toBase64().toStdString();
    }

    //json array -- recurse for list conversion
    if (state.type() == qMetaTypeId<QJsonArray>() and stateType != nullptr)
    {
        *stateType = "json";
        return QVariantToJsonState(state.toJsonArray().toVariantList());
    }

    //json object -- recurse for map conversion
    if (state.type() == qMetaTypeId<QJsonObject>() and stateType != nullptr)
    {
        *stateType = "json";
        return QVariantToJsonState(state.toJsonObject().toVariantMap());
    }

    //boolean
    if (state.type() == qMetaTypeId<bool>()) return state.toBool();

    //string
    if (state.type() == qMetaTypeId<QString>()) return state.toString().toStdString();

    //signed 64-bit integers
    if (state.type() == qMetaTypeId<qint64>()) return state.toLongLong();
    if (state.type() == qMetaTypeId<qlonglong>()) return state.toLongLong();

    //unsigned 64-bit integers
    if (state.type() == qMetaTypeId<quint64>()) return state.toULongLong();
    if (state.type() == qMetaTypeId<qulonglong>()) return state.toULongLong();

    //signed integers
    if (state.type() == qMetaTypeId<qint32>()) return state.toInt();
    if (state.type() == qMetaTypeId<qint16>()) return state.toInt();
    if (state.type() == qMetaTypeId<qint8>()) return state.toInt();

    //unsigned integers
    if (state.type() == qMetaTypeId<quint32>()) return state.toUInt();
    if (state.type() == qMetaTypeId<quint16>()) return state.toUInt();
    if (state.type() == qMetaTypeId<quint8>()) return state.toUInt();

    //floating point types
    if (state.type() == qMetaTypeId<float>()) return state.toDouble();
    if (state.type() == qMetaTypeId<double>()) return state.toDouble();

    //list type
    if (state.canConvert<QVariantList>())
    {
        Poco::JSON::Array::Ptr arr(new Poco::JSON::Array);
        for (const auto &elem : state.toList())
        {
            arr->add(QVariantToJsonState(elem));
        }
        return arr;
    }

    //map type
    if (state.canConvert<QVariantMap>())
    {
        Poco::JSON::Object::Ptr obj(new Poco::JSON::Object);
        for (const auto &pair : state.toMap().toStdMap())
        {
            obj->set(pair.first.toStdString(), QVariantToJsonState(pair.second));
        }
        return obj;
    }

    //present an error when the conversion is not possible
    poco_error_f1(Poco::Logger::get("PothosGui.GraphWidget.serialize"),
        "No conversion to JSON with type=%s", std::string(state.typeName()));
    return Poco::Dynamic::Var();
}

static QVariant JsonStateToQVariant(const Poco::Dynamic::Var &var, const std::string &stateType = "")
{
    if (var.isEmpty()) return QVariant();

    //binary data -- only for top level element
    if (stateType == "binary" and var.isString())
    {
        return QByteArray::fromBase64(QByteArray::fromStdString(var.extract<std::string>()));
    }

    //json array -- recurse for list conversion
    if (stateType == "json" and var.type() == typeid(Poco::JSON::Array::Ptr))
    {
        return QJsonArray::fromVariantList(JsonStateToQVariant(var).toList());
    }

    //json object -- recurse for map conversion
    if (stateType == "json" and var.type() == typeid(Poco::JSON::Object::Ptr))
    {
        return QJsonObject::fromVariantMap(JsonStateToQVariant(var).toMap());
    }

    //boolean
    if (var.isBoolean()) return var.extract<bool>();

    //string
    if (var.isString()) return QString::fromStdString(var.extract<std::string>());

    //signed integer, use signed 64-bit destination
    if (var.isInteger() and var.isSigned()) return var.extract<qint64>();

    //unsigned integer, use unsigned 64-bit destination
    if (var.isInteger() and not var.isSigned()) return var.extract<quint64>();

    //floating point types, use double
    if (var.isNumeric()) return var.extract<double>();

    //array type
    if (var.type() == typeid(Poco::JSON::Array::Ptr))
    {
        QVariantList list;
        for (const auto &elem : *var.extract<Poco::JSON::Array::Ptr>())
        {
            list.append(JsonStateToQVariant(elem));
        }
        return list;
    }

    //object type
    if (var.type() == typeid(Poco::JSON::Object::Ptr))
    {
        QVariantMap map;
        for (const auto &pair : *var.extract<Poco::JSON::Object::Ptr>())
        {
            map[QString::fromStdString(pair.first)] = JsonStateToQVariant(pair.second);
        }
        return map;
    }

    //present an error when the conversion is not possible
    poco_error_f2(Poco::Logger::get("PothosGui.GraphWidget.deserialize"),
        "No conversion to QVariant: varType=%s, stateType=%s",
        Pothos::Util::typeInfoToString(var.type()), stateType);
    return QVariant();
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

    //save the widget state to JSON
    std::string stateType;
    auto stateVar = QVariantToJsonState(_impl->widgetState, &stateType);
    if (not stateVar.isEmpty()) obj->set("state", stateVar);
    if (not stateType.empty()) obj->set("stateType", stateType);

    return obj;
}

void GraphWidget::deserialize(Poco::JSON::Object::Ptr obj)
{
    auto editor = this->draw()->getGraphEditor();

    //locate the associated block
    auto blockId = QString::fromStdString(obj->getValue<std::string>("blockId"));
    auto graphObj = editor->getObjectById(blockId, GRAPH_BLOCK);
    if (graphObj == nullptr) throw Pothos::NotFoundException("GraphWidget::deserialize()", "cant resolve block with ID: '"+blockId.toStdString()+"'");
    auto graphBlock = dynamic_cast<GraphBlock *>(graphObj);
    assert(graphBlock != nullptr);
    this->setGraphBlock(graphBlock);

    if (obj->has("width") and obj->has("height"))
    {
        _impl->graphicsWidget->resize(
            obj->getValue<int>("width"),
            obj->getValue<int>("height"));
    }

    //restore the widget state from JSON
    Poco::Dynamic::Var stateVar;
    if (obj->has("state")) stateVar = obj->get("state");
    const auto stateType = obj->optValue<std::string>("stateType", "text");
    _impl->widgetState = JsonStateToQVariant(stateVar, stateType);

    //emit the current state to the widget when connected
    if (_impl->stateRestoreConnection and _impl->widgetState.isValid())
    {
        emit this->restoreWidgetState(_impl->widgetState);
    }

    GraphObject::deserialize(obj);
}
