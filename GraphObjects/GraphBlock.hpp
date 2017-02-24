// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "GraphObjects/GraphObject.hpp"
#include <QStringList>
#include <QObject>
#include <QString>
#include <QPointF>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>
#include <vector>

class QWidget;

class GraphBlock : public GraphObject
{
    Q_OBJECT
public:
    GraphBlock(QObject *parent);

    //! set the block description from JSON object
    void setBlockDesc(const QJsonObject &);
    const QJsonObject &getBlockDesc(void) const;
    QString getBlockDescPath(void) const;

    //! set the input ports description from JSON array
    void setInputPortDesc(const QJsonArray &);

    //! set the output ports description from JSON array
    void setOutputPortDesc(const QJsonArray &);

    //! set the dynamically queried block description overlay
    void setOverlayDesc(const QJsonObject &);
    const QJsonObject &getOverlayDesc(void) const;

    //! Does this graph block represent a display widget
    bool isGraphWidget(void) const;
    QWidget *getGraphWidget(void) const;
    void setGraphWidget(QWidget *);

    void setTitle(const QString &title);
    const QString &getTitle(void) const;

    QPainterPath shape(void) const;

    //! indicate an external change -- thats not applied through the other setters
    void changed(void);

    void render(QPainter &painter);

    //! Set an error message when trying to eval the block
    void clearBlockErrorMsgs(void);
    void addBlockErrorMsg(const QString &msg);
    const QStringList &getBlockErrorMsgs(void) const;

    void addProperty(const QString &key);
    const QStringList &getProperties(void) const;

    //! Get the param desc from the block description
    QJsonObject getParamDesc(const QString &key) const;

    QString getPropertyValue(const QString &key) const;
    void setPropertyValue(const QString &key, const QString &value);

    QString getPropertyName(const QString &key) const;
    void setPropertyName(const QString &key, const QString &name);

    /*!
     * Editable widget can be in default mode "" or "raw" mode.
     * Future modes may be possible - like multi-line support.
     */
    QString getPropertyEditMode(const QString &key) const;
    void setPropertyEditMode(const QString &key, const QString &mode);

    //! Get the property display text: varies from actual value, enum name, error...
    QString getPropertyDisplayText(const QString &key) const;

    //! Set the error message when trying to eval this property -- blank msg for no error
    void setPropertyErrorMsg(const QString &key, const QString &msg);
    const QString &getPropertyErrorMsg(const QString &key) const;

    //! Set a descriptive type string for this property
    void setPropertyTypeStr(const QString &key, const QString &type);
    const QString &getPropertyTypeStr(const QString &key) const;

    bool getPropertyPreview(const QString &key) const;
    void setPropertyPreviewMode(const QString &key, const QString &value,
        const QJsonArray &args = QJsonArray(),
        const QJsonObject &kwargs = QJsonObject());

    void addInputPort(const QString &portKey, const QString &portAlias);
    const QStringList &getInputPorts(void) const;
    const QString &getInputPortAlias(const QString &portKey) const;

    void addOutputPort(const QString &portKey, const QString &portAlias);
    const QStringList &getOutputPorts(void) const;
    const QString &getOutputPortAlias(const QString &portKey) const;

    void addSlotPort(const QString &portKey);
    const QStringList &getSlotPorts(void) const;

    void addSignalPort(const QString &portKey);
    const QStringList &getSignalPorts(void) const;

    //! Set a descriptive type string for input ports
    void setInputPortTypeStr(const QString &key, const QString &type);
    const QString &getInputPortTypeStr(const QString &key) const;

    //! Set a descriptive type string for output ports
    void setOutputPortTypeStr(const QString &key, const QString &type);
    const QString &getOutputPortTypeStr(const QString &key) const;

    std::vector<GraphConnectableKey> getConnectableKeys(void) const;
    GraphConnectableKey isPointingToConnectable(const QPointF &pos) const;
    GraphConnectableAttrs getConnectableAttrs(const GraphConnectableKey &key) const;

    QJsonObject serialize(void) const;

    virtual void deserialize(const QJsonObject &obj);

    //! affinity zone support
    const QString &getAffinityZone(void) const;
    void setAffinityZone(const QString &zone);

    //! get current edit tab (empty for no selection)
    const QString &getActiveEditTab(void) const;
    void setActiveEditTab(const QString &name);

    //! Called by Connection to track active graph connections
    void registerEndpoint(const GraphConnectionEndpoint &ep);
    void unregisterEndpoint(const GraphConnectionEndpoint &ep);

signals:

    //! Called by the evaluator when eval completed
    void evalDoneEvent(void);

    //! Called externally to trigger individual eval
    void triggerEvalEvent(void);

    //! Called when the overlay changes the param description
    void paramDescChanged(const QString &key, const QJsonObject &desc);

protected:

    //! Block uses this to respond to selection changes
    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

private:
    void renderStaticText(void);
    struct Impl;
    std::shared_ptr<Impl> _impl;
    QStringList _properties;
    QStringList _inputPorts;
    QStringList _outputPorts;
    QStringList _slotPorts;
    QStringList _signalPorts;
};
