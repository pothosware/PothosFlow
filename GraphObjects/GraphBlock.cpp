// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphObjects/GraphBlockImpl.hpp"
#include "GraphEditor/Constants.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include "BlockTree/BlockCache.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include "ColorUtils/ColorUtils.hpp"
#include "MainWindow/MainActions.hpp"
#include <Pothos/Exception.hpp>
#include <QGraphicsScene>
#include <QAction>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QFontMetrics>
#include <QSet>
#include <iostream>
#include <cassert>
#include <algorithm> //min/max
#include <Poco/Logger.h>

GraphBlock::GraphBlock(QObject *parent):
    GraphObject(parent),
    _impl(new Impl())
{
    this->setFlag(QGraphicsItem::ItemIsMovable);
}

QString GraphBlock::getBlockDescPath(void) const
{
    return _impl->blockDesc["path"].toString();
}

const QJsonObject &GraphBlock::getBlockDesc(void) const
{
    assert(_impl);
    return _impl->blockDesc;
}

static void paramKeysFromJSON(QSet<QString> &keys, const QJsonObject &desc)
{
    for (const auto &paramValue : desc["params"].toArray())
    {
        const auto param = paramValue.toObject();
        if (not param.contains("key")) continue;
        keys.insert(param["key"].toString());
    }
}

void GraphBlock::setOverlayDesc(const QJsonObject &desc)
{
    assert(_impl);

    //check for pointer equality since the evaluator should not
    //give us a new JSON pointer unless the string value changed
    const bool changed(_impl->overlayDesc != desc);
    auto oldOverlay = _impl->overlayDesc;
    _impl->overlayDesc = desc;

    if (not changed) return; //nothing to do? return

    //collect keys from the old and new overlay to cover all changed params
    QSet<QString> changedKeys;
    paramKeysFromJSON(changedKeys, oldOverlay);
    paramKeysFromJSON(changedKeys, desc);

    //emit description for all changed keys
    for (const auto &key : changedKeys)
    {
        emit this->paramDescChanged(key, this->getParamDesc(key));
    }
}

const QJsonObject &GraphBlock::getOverlayDesc(void) const
{
    assert(_impl);
    return _impl->overlayDesc;
}

bool GraphBlock::isGraphWidget(void) const
{
    assert(_impl);
    return _impl->isGraphWidget;
}

QWidget *GraphBlock::getGraphWidget(void) const
{
    return _impl->graphWidget.data();
}

void GraphBlock::setGraphWidget(QWidget *widget)
{
    if (_impl->graphWidget == widget) return;
    _impl->graphWidget = widget;
    this->markChanged();
}

void GraphBlock::setTitle(const QString &title)
{
    if (_impl->title == title) return;
    _impl->title = title;
    this->markChanged();
}

const QString &GraphBlock::getTitle(void) const
{
    return _impl->title;
}

void GraphBlock::clearBlockErrorMsgs(void)
{
    if (_impl->blockErrorMsgs.empty()) return;
    _impl->blockErrorMsgs.clear();
    this->markChanged();
}

void GraphBlock::addBlockErrorMsg(const QString &msg)
{
    assert(not msg.isEmpty());
    _impl->blockErrorMsgs.push_back(msg);
    this->markChanged();
}

const QStringList &GraphBlock::getBlockErrorMsgs(void) const
{
    return _impl->blockErrorMsgs;
}

void GraphBlock::addProperty(const QString &key)
{
    _properties.push_back(key);
    this->markChanged();
}

const QStringList &GraphBlock::getProperties(void) const
{
    return _properties;
}

QJsonObject GraphBlock::getParamDesc(const QString &key) const
{
    QJsonObject paramDesc;
    for (const auto &paramValue : this->getBlockDesc()["params"].toArray())
    {
        const auto param = paramValue.toObject();
        if (param["key"].toString() == key) paramDesc = param;
    }

    //try to apply the overlay if it exists
    const auto overlayDesc = this->getOverlayDesc();
    if (not paramDesc.isEmpty() and not overlayDesc.isEmpty())
    {
        const auto key = paramDesc["key"].toString();
        for (const auto &paramValue : overlayDesc["params"].toArray())
        {
            const auto param = paramValue.toObject();
            if (param["key"].toString() != key) continue;
            for (const auto &key : param.keys())
            {
                paramDesc[key] = param[key];
            }
        }
    }

    return paramDesc;
}

QString GraphBlock::getPropertyDisplayText(const QString &key) const
{
    auto value = this->getPropertyValue(key);
    auto paramDesc = this->getParamDesc(key);
    for (const auto &optionVal : paramDesc["options"].toArray())
    {
        const auto option = optionVal.toObject();
        if (value == option["value"].toString())
        {
            return option["name"].toString();
        }
    }

    //strip enclosing control characters to save space
    if (value.size() >= 2 and (
        (value.startsWith("\"") and value.endsWith("\"")) or
        (value.startsWith("'") and value.endsWith("'")) or
        (value.startsWith("(") and value.endsWith(")")) or
        (value.startsWith("[") and value.endsWith("]")) or
        (value.startsWith("{") and value.endsWith("}"))
    ))
    {
        value.remove(-1, 1).remove(0, 1);
    }

    return value;
}

QString GraphBlock::getPropertyValue(const QString &key) const
{
    auto it = _impl->propertiesValues.find(key);
    if (it != _impl->propertiesValues.end()) return it->second;
    return "";
}

void GraphBlock::setPropertyValue(const QString &key, const QString &value)
{
    if (_impl->propertiesValues[key] == value) return;
    _impl->propertiesValues[key] = value;
    this->markChanged();
}

QString GraphBlock::getPropertyName(const QString &key) const
{
    auto it = _impl->propertiesNames.find(key);
    if (it != _impl->propertiesNames.end()) return it->second;
    return key;
}

void GraphBlock::setPropertyName(const QString &key, const QString &name)
{
    if (_impl->propertiesNames[key] == name) return;
    _impl->propertiesNames[key] = name;
    this->markChanged();
}

QString GraphBlock::getPropertyEditMode(const QString &key) const
{
    auto it = _impl->propertiesEditMode.find(key);
    if (it != _impl->propertiesEditMode.end()) return it->second;
    return "";
}

void GraphBlock::setPropertyEditMode(const QString &key, const QString &mode)
{
    _impl->propertiesEditMode[key] = mode;
}

/*!
 * determine if a value is valid (boolean true)
 * empty data types are considered to be false.
 */
static bool isValid(const QString &value)
{
    if (value.isEmpty()) return false;
    if (value == "\"\"") return false;
    if (value == "''") return false;
    if (value == "0") return false;
    if (value == "{}") return false;
    if (value == "[]") return false;
    if (value == "()") return false;
    if (value == "false") return false;

    //check various forms of floating point 0.00, 0e0, etc...
    bool ok = false;
    if (value.toDouble(&ok) == 0.0 and ok) return false;

    return true;
}

bool GraphBlock::getPropertyPreview(const QString &key) const
{
    auto it = _impl->propertiesPreview.find(key);
    if (it == _impl->propertiesPreview.end()) return true; //default is ON
    if (it->second == "enable") return true;
    if (it->second == "disable") return false;
    if (it->second == "valid") return isValid(this->getPropertyValue(key));
    if (it->second == "invalid") return not isValid(this->getPropertyValue(key));
    if (it->second == "when")
    {
        //support the enum selection mode:
        //There is no parameter evaluation going on here.
        //A particular parameter, specified by the enum key,
        //is compared against a list of arguments.
        //A match means do the preview, otherwise hide it.
        if (_impl->propertiesPreviewArgs.at(key).isEmpty()) return true;
        if (_impl->propertiesPreviewKwargs.at(key).isEmpty()) return true;
        if (not _impl->propertiesPreviewKwargs.at(key).contains("enum")) return true;
        const auto enumParamKey = _impl->propertiesPreviewKwargs.at(key)["enum"].toString();
        const auto enumParamValue = this->getPropertyValue(enumParamKey);
        for (const auto &argVal : _impl->propertiesPreviewArgs.at(key))
        {
            auto argStr = argVal.toVariant().toString();

            //add quotes when the argument has no quotes and the enum value does
            if (not enumParamValue.isEmpty() and enumParamValue.startsWith('"')
                and not argStr.isEmpty() and not argStr.startsWith('"'))
            {
                argStr += "\"" + argStr + "\"";
            }

            if (argStr == enumParamValue) return true;
        }
        return false;
    }
    return true;
}

void GraphBlock::setPropertyPreviewMode(const QString &key, const QString &value,
    const QJsonArray &args, const QJsonObject &kwargs)
{
    if (_impl->propertiesPreview[key] == value) return;
    _impl->propertiesPreview[key] = value;
    _impl->propertiesPreviewArgs[key] = args;
    _impl->propertiesPreviewKwargs[key] = kwargs;
    this->markChanged();
}

void GraphBlock::setPropertyErrorMsg(const QString &key, const QString &msg)
{
    if (_impl->propertiesErrorMsg[key] == msg) return;
    _impl->propertiesErrorMsg[key] = msg;
    this->markChanged();
}

const QString &GraphBlock::getPropertyErrorMsg(const QString &key) const
{
    return _impl->propertiesErrorMsg[key];
}

void GraphBlock::setPropertyTypeStr(const QString &key, const QString &type)
{
    if (_impl->propertiesTypeStr[key] == type) return;
    _impl->propertiesTypeStr[key] = type;
    this->markChanged();
}

const QString &GraphBlock::getPropertyTypeStr(const QString &key) const
{
    return _impl->propertiesTypeStr[key];
}

void GraphBlock::addInputPort(const QString &portKey, const QString &portAlias)
{
    _inputPorts.push_back(portKey);
    _impl->inputPortsAliases[portKey] = portAlias;
    this->markChanged();
}

const QStringList &GraphBlock::getInputPorts(void) const
{
    return _inputPorts;
}

const QString &GraphBlock::getInputPortAlias(const QString &portKey) const
{
    return _impl->inputPortsAliases[portKey];
}

void GraphBlock::addOutputPort(const QString &portKey, const QString &portAlias)
{
    _outputPorts.push_back(portKey);
    _impl->outputPortsAliases[portKey] = portAlias;
    this->markChanged();
}

const QStringList &GraphBlock::getOutputPorts(void) const
{
    return _outputPorts;
}

const QString &GraphBlock::getOutputPortAlias(const QString &portKey) const
{
    return _impl->outputPortsAliases[portKey];
}

void GraphBlock::addSlotPort(const QString &portKey)
{
    _slotPorts.push_back(portKey);
    this->markChanged();
}

const QStringList &GraphBlock::getSlotPorts(void) const
{
    return _slotPorts;
}

void GraphBlock::addSignalPort(const QString &portKey)
{
    _signalPorts.push_back(portKey);
    this->markChanged();
}

const QStringList &GraphBlock::getSignalPorts(void) const
{
    return _signalPorts;
}

void GraphBlock::setInputPortTypeStr(const QString &key, const QString &type)
{
    if (_impl->inputPortTypeStr[key] == type) return;
    _impl->inputPortTypeStr[key] = type;
    this->markChanged();
}

const QString &GraphBlock::getInputPortTypeStr(const QString &key) const
{
    return _impl->inputPortTypeStr[key];
}

void GraphBlock::setOutputPortTypeStr(const QString &key, const QString &type)
{
    if (_impl->outputPortTypeStr[key] == type) return;
    _impl->outputPortTypeStr[key] = type;
    this->markChanged();
}

const QString &GraphBlock::getOutputPortTypeStr(const QString &key) const
{
    return _impl->outputPortTypeStr[key];
}

const QString &GraphBlock::getAffinityZone(void) const
{
    return _impl->affinityZone;
}

void GraphBlock::setAffinityZone(const QString &zone)
{
    if (_impl->affinityZone == zone) return;
    _impl->affinityZone = zone;
    this->markChanged();
}

const QString &GraphBlock::getActiveEditTab(void) const
{
    return _impl->activeEditTab;
}

void GraphBlock::setActiveEditTab(const QString &name)
{
    _impl->activeEditTab = name;
}

void GraphBlock::registerEndpoint(const GraphConnectionEndpoint &ep)
{
    assert(ep.getObj().data() == this);
    switch(ep.getKey().direction)
    {
    case GRAPH_CONN_INPUT: _impl->inputPortUseCount[ep.getKey().id]++; break;
    case GRAPH_CONN_OUTPUT: _impl->outputPortUseCount[ep.getKey().id]++; break;
    case GRAPH_CONN_SLOT: _impl->slotPortUseCount++; break;
    case GRAPH_CONN_SIGNAL: _impl->signalPortUseCount++; break;
    }
    this->markChanged();
}

void GraphBlock::unregisterEndpoint(const GraphConnectionEndpoint &ep)
{
    assert(ep.getObj().data() == this);
    switch(ep.getKey().direction)
    {
    case GRAPH_CONN_INPUT: _impl->inputPortUseCount[ep.getKey().id]--; break;
    case GRAPH_CONN_OUTPUT: _impl->outputPortUseCount[ep.getKey().id]--; break;
    case GRAPH_CONN_SLOT: _impl->slotPortUseCount--; break;
    case GRAPH_CONN_SIGNAL: _impl->signalPortUseCount--; break;
    }
    this->markChanged();
}

QVariant GraphBlock::itemChange(GraphicsItemChange change, const QVariant &value)
{
    //cause re-rendering of the text because we force show all port text
    if (change == QGraphicsItem::ItemSelectedHasChanged)
    {
        this->markChanged();
        this->update();
    }
    return QGraphicsItem::itemChange(change, value);
}

QPainterPath GraphBlock::shape(void) const
{
    QPainterPath path;
    for (const auto &portRect : _impl->inputPortRects) path.addRect(portRect);
    for (const auto &portRect : _impl->outputPortRects) path.addRect(portRect);
    if (not this->getSignalPorts().empty()) path.addRect(_impl->signalPortRect);
    path.addRect(_impl->mainBlockRect);
    return path;
}

std::vector<GraphConnectableKey> GraphBlock::getConnectableKeys(void) const
{
    std::vector<GraphConnectableKey> keys;
    for (int i = 0; i < _inputPorts.size(); i++)
    {
        keys.push_back(GraphConnectableKey(_inputPorts[i], GRAPH_CONN_INPUT));
    }
    for (int i = 0; i < _outputPorts.size(); i++)
    {
        keys.push_back(GraphConnectableKey(_outputPorts[i], GRAPH_CONN_OUTPUT));
    }
    if (not this->getSlotPorts().empty())
    {
        keys.push_back(GraphConnectableKey("slots", GRAPH_CONN_SLOT));
    }
    if (not this->getSignalPorts().empty())
    {
        keys.push_back(GraphConnectableKey("signals", GRAPH_CONN_SIGNAL));
    }
    return keys;
}

GraphConnectableKey GraphBlock::isPointingToConnectable(const QPointF &pos) const
{
    //note: the rectangles may not be allocated yet
    //so use the bounds for both ports and rectangles

    for (int i = 0; i < _inputPorts.size() and _impl->inputPortRects.size(); i++)
    {
        if (_impl->inputPortRects[i].contains(pos))
            return GraphConnectableKey(_inputPorts[i], GRAPH_CONN_INPUT);
    }
    for (int i = 0; i < _outputPorts.size() and _impl->outputPortRects.size(); i++)
    {
        if (_impl->outputPortRects[i].contains(pos))
            return GraphConnectableKey(_outputPorts[i], GRAPH_CONN_OUTPUT);
    }
    if (not this->getSlotPorts().empty())
    {
        if (_impl->mainBlockRect.contains(pos))
            return GraphConnectableKey("slots", GRAPH_CONN_SLOT);
    }
    if (not this->getSignalPorts().empty())
    {
        if (_impl->signalPortRect.contains(pos))
            return GraphConnectableKey("signals", GRAPH_CONN_SIGNAL);
    }
    return GraphConnectableKey();
}

GraphConnectableAttrs GraphBlock::getConnectableAttrs(const GraphConnectableKey &key) const
{
    GraphConnectableAttrs attrs;
    attrs.direction = key.direction;
    attrs.rotation = this->rotation();
    if (key.direction == GRAPH_CONN_INPUT) for (int i = 0; i < _inputPorts.size(); i++)
    {
        if (key.id == _inputPorts[i])
        {
            if (_impl->inputPortPoints.size() > size_t(i)) //may not be allocated yet
                attrs.point = _impl->inputPortPoints[i];
            attrs.rotation += 180;
            return attrs;
        }
    }
    if (key.direction == GRAPH_CONN_OUTPUT) for (int i = 0; i < _outputPorts.size(); i++)
    {
        if (key.id == _outputPorts[i])
        {
            if (_impl->outputPortPoints.size() > size_t(i)) //may not be allocated yet
                attrs.point = _impl->outputPortPoints[i];
            attrs.rotation += 0;
            return attrs;
        }
    }
    if (key.direction == GRAPH_CONN_SLOT and key.id == "slots")
    {
        attrs.point = _impl->slotPortPoint;
        attrs.rotation += _impl->eventPortsInline?180:270;
        return attrs;
    }
    if (key.direction == GRAPH_CONN_SIGNAL and key.id == "signals")
    {
        attrs.point = _impl->signalPortPoint;
        attrs.rotation += _impl->eventPortsInline?0:90;
        return attrs;
    }
    return attrs;
}

static QStaticText makeQStaticText(const QString &s)
{
    QStaticText st(s);
    QTextOption to;
    to.setWrapMode(QTextOption::NoWrap);
    st.setTextOption(to);
    return st;
}

static QString getTextColor(const bool isOk, const QColor &bg)
{
    if (isOk) return (bg.lightnessF() > 0.5)?"black":"white";
    else return (bg.lightnessF() > 0.5)?"red":"pink";
}

void GraphBlock::renderStaticText(void)
{
    //clear current state
    _impl->propertiesText.clear();
    _impl->inputPortsText.clear();
    _impl->inputPortsBorder.clear();
    _impl->outputPortsText.clear();
    _impl->outputPortsBorder.clear();

    //default rendering
    const QPen defaultPen(QColor(GraphObjectDefaultPenColor), GraphObjectBorderWidth);
    const QPen connectPen(QColor(ConnectModeHighlightPenColor), ConnectModeHighlightWidth);
    _impl->inputPortsText.resize(_inputPorts.size(), QStaticText(" "));
    _impl->inputPortsBorder.resize(_inputPorts.size(), defaultPen);
    _impl->outputPortsText.resize(_outputPorts.size(), QStaticText(" "));
    _impl->outputPortsBorder.resize(_outputPorts.size(), defaultPen);
    _impl->signalPortBorder = defaultPen;
    _impl->mainRectBorder = defaultPen;
    const bool forceShowPortNames = _impl->showPortNames or this->isSelected();
    const auto &trackedKey = this->currentTrackedConnectable();
    const auto &clickedEp = this->draw()->lastClickedEndpoint();
    const bool connectToInput = clickedEp.isValid() and not clickedEp.getKey().isInput();
    const bool connectToOutput = clickedEp.isValid() and clickedEp.getKey().isInput();

    //load the title text
    _impl->titleText = makeQStaticText(QString("<span style='color:%1;font-size:%2;'><b>%3</b></span>")
        .arg(getTextColor(this->getBlockErrorMsgs().isEmpty(), _impl->mainBlockColor))
        .arg(GraphBlockTitleFontSize)
        .arg(_impl->title.toHtmlEscaped()));

    //load the properties text
    for (int i = 0; i < _properties.size(); i++)
    {
        if (not this->getPropertyPreview(_properties[i])) continue;

        //shorten text with ellipsis
        QFont font; font.setPointSize(GraphBlockPropPointWidth);
        QFontMetrics metrics(font);
        auto propText = this->getPropertyDisplayText(_properties[i]);
        propText = metrics.elidedText(propText, Qt::ElideMiddle, GraphBlockPropMaxWidthPx);

        auto text = makeQStaticText(QString("<span style='color:%1;font-size:%2;'><b>%3: </b> %4</span>")
            .arg(getTextColor(this->getPropertyErrorMsg(_properties[i]).isEmpty(), _impl->mainBlockColor))
            .arg(GraphBlockPropFontSize)
            .arg(this->getPropertyName(_properties[i]).toHtmlEscaped())
            .arg(propText.toHtmlEscaped()));
        _impl->propertiesText.push_back(text);
    }

    //load the inputs text
    for (int i = 0; i < _inputPorts.size(); i++)
    {
        const bool tracked = (trackedKey == GraphConnectableKey(_inputPorts[i], GRAPH_CONN_INPUT));
        if (this->isSelected()) _impl->inputPortsBorder[i] = QPen(GraphObjectHighlightPenColor);
        if (tracked and connectToInput) _impl->inputPortsBorder[i] = connectPen;

        if (not forceShowPortNames and not tracked) continue;
        _impl->inputPortsText[i] = QStaticText(QString("<span style='color:%1;font-size:%2;'>%3</span>")
            .arg(getTextColor(true, _impl->inputPortColors.at(i)))
            .arg(GraphBlockPortFontSize)
            .arg(this->getInputPortAlias(_inputPorts[i]).toHtmlEscaped()));
    }

    //load the outputs text
    for (int i = 0; i < _outputPorts.size(); i++)
    {
        const bool tracked = (trackedKey == GraphConnectableKey(_outputPorts[i], GRAPH_CONN_OUTPUT));
        if (this->isSelected()) _impl->outputPortsBorder[i] = QPen(GraphObjectHighlightPenColor);
        if (tracked and connectToOutput) _impl->outputPortsBorder[i] = connectPen;

        if (not forceShowPortNames and not tracked) continue;
        _impl->outputPortsText[i] = QStaticText(QString("<span style='color:%1;font-size:%2;'>%3</span>")
            .arg(getTextColor(true, _impl->outputPortColors.at(i)))
            .arg(GraphBlockPortFontSize)
            .arg(this->getOutputPortAlias(_outputPorts[i]).toHtmlEscaped()));
    }

    //signal port setup
    {
        const bool tracked = (trackedKey == GraphConnectableKey("signals", GRAPH_CONN_SIGNAL));
        if (this->isSelected()) _impl->signalPortBorder = QPen(GraphObjectHighlightPenColor);
        if (tracked and connectToOutput) _impl->signalPortBorder = connectPen;
    }

    //slot port/main rect setup
    {
        const bool tracked = (trackedKey == GraphConnectableKey("slots", GRAPH_CONN_SLOT));
        if (this->isSelected()) _impl->mainRectBorder = QPen(GraphObjectHighlightPenColor);
        if (tracked and connectToInput) _impl->mainRectBorder = connectPen;
    }
}

void GraphBlock::changed(void)
{
    this->markChanged();
}

static QColor generateDisabledColor(const QColor c)
{
    const QColor d(GraphBlockDisabledColor);
    if (not c.isValid()) return d;
    static const qreal alpha(GraphBlockDisabledAlphaBlend);
    return QColor(
        c.red()*alpha + d.red()*(1-alpha),
        c.green()*alpha + d.green()*(1-alpha),
        c.blue()*alpha + d.blue()*(1-alpha)
    );
}

void GraphBlock::render(QPainter &painter)
{
    //render text
    if (this->isChanged())
    {
        auto actions = MainActions::global();
        _impl->showPortNames = actions->showPortNamesAction->isChecked();
        _impl->eventPortsInline = actions->eventPortsInlineAction->isChecked();

        this->update(); //call first because this will set changed again
        this->clearChanged();

        //update colors
        auto zoneColor = AffinityZonesDock::global()->zoneToColor(this->getAffinityZone());
        _impl->mainBlockColor = zoneColor.isValid()?zoneColor:QColor(GraphObjectDefaultFillColor);
        if (not this->isEnabled()) _impl->mainBlockColor = generateDisabledColor(zoneColor);
        _impl->inputPortColors.resize(_inputPorts.size(), GraphObjectDefaultFillColor);
        _impl->outputPortColors.resize(_outputPorts.size(), GraphObjectDefaultFillColor);
        for (int i = 0; i < _inputPorts.size(); i++) _impl->inputPortColors[i] = typeStrToColor(this->getInputPortTypeStr(_inputPorts.at(i)));
        for (int i = 0; i < _outputPorts.size(); i++) _impl->outputPortColors[i] = typeStrToColor(this->getOutputPortTypeStr(_outputPorts.at(i)));
        this->renderStaticText();

        //connection endpoints may have moved - flag the scene for re-draw
        this->scene()->update();
    }

    //setup rotations and translations
    QTransform trans;

    //dont rotate past 180 because we just do a port flip
    //this way text only ever has 2 rotations
    const bool portFlip = this->rotation() >= 180;
    if (portFlip) painter.rotate(-180);
    if (portFlip) trans.rotate(-180);

    //port totals for calculations below
    //slots are only present when connected
    const bool eventPortsInline = _impl->eventPortsInline;
    const size_t numInputs = this->getInputPorts().size();
    const size_t numOutputs = this->getOutputPorts().size();
    const bool hasSignals = not this->getSignalPorts().empty();
    const bool hasSlots = not this->getSlotPorts().empty() and _impl->slotPortUseCount != 0;
    const size_t numLeftSideEndpoints = numInputs + ((eventPortsInline and hasSlots)?1:0);
    const size_t numRightSideEndpoints = numOutputs + ((eventPortsInline and hasSignals)?1:0);

    //calculate dimensions for input side
    qreal inputPortsMinHeight = GraphBlockPortVOutterPad*2;
    if (numLeftSideEndpoints == 0) inputPortsMinHeight = 0;
    else inputPortsMinHeight += (numLeftSideEndpoints-1)*GraphBlockPortVGap;
    for (const auto &text : _impl->inputPortsText)
    {
        inputPortsMinHeight += text.size().height() + GraphBlockPortTextVPad*2;
    }
    if (hasSlots and eventPortsInline) inputPortsMinHeight += GraphBlockSignalPortSpan;

    //calculate dimensions for output side
    qreal outputPortsMinHeight = GraphBlockPortVOutterPad*2;
    if (numRightSideEndpoints == 0) outputPortsMinHeight = 0;
    else outputPortsMinHeight += (numRightSideEndpoints-1)*GraphBlockPortVGap;
    for (const auto &text : _impl->outputPortsText)
    {
        outputPortsMinHeight += text.size().height() + GraphBlockPortTextVPad*2;
    }
    if (hasSignals and eventPortsInline) outputPortsMinHeight += GraphBlockSignalPortSpan;

    qreal propertiesMinHeight = 0;
    qreal propertiesMaxWidth = 0;
    for (const auto &text : _impl->propertiesText)
    {
        propertiesMinHeight += text.size().height() + GraphBlockPropTextVPad*2;
        propertiesMaxWidth = std::max<qreal>(propertiesMaxWidth, text.size().width() + GraphBlockPropTextHPad*2);
    }

    const qreal propertiesWithTitleMinHeight = GraphBlockTitleVPad + _impl->titleText.size().height() + GraphBlockTitleVPad + propertiesMinHeight;
    const qreal overallHeight = std::max(std::max(inputPortsMinHeight, outputPortsMinHeight), propertiesWithTitleMinHeight);
    const qreal overallWidth = std::max(GraphBlockTitleHPad + _impl->titleText.size().width() + GraphBlockTitleHPad, propertiesMaxWidth);

    //new dimensions for the main rectangle
    QRectF mainRect(QPointF(), QPointF(overallWidth, overallHeight));
    mainRect.moveCenter(QPointF());
    _impl->mainBlockRect = trans.mapRect(mainRect);
    auto p = mainRect.topLeft();

    //set painter for drawing the rectangles
    auto pen = QPen(QColor(GraphObjectDefaultPenColor));
    pen.setWidthF(GraphObjectBorderWidth);
    painter.setPen(pen);

    //create input ports
    _impl->inputPortRects.resize(numInputs);
    _impl->inputPortPoints.resize(numInputs);
    qreal inPortVdelta = (overallHeight - inputPortsMinHeight)/2.0 + GraphBlockPortVOutterPad;
    for (size_t i = 0; i < numInputs; i++)
    {
        const auto &text = _impl->inputPortsText.at(i);
        QSizeF rectSize = text.size() + QSizeF(GraphBlockPortTextHPad*2, GraphBlockPortTextVPad*2);
        const qreal hOff = (portFlip)? overallWidth :  1-rectSize.width();
        QRectF portRect(p+QPointF(hOff, inPortVdelta), rectSize);
        inPortVdelta += rectSize.height() + GraphBlockPortVGap;
        painter.save();
        painter.setBrush(QBrush(_impl->inputPortColors.at(i)));
        painter.setPen(_impl->inputPortsBorder[i]);
        painter.drawRect(portRect);
        painter.restore();
        _impl->inputPortRects[i] = trans.mapRect(portRect);

        const qreal availablePortHPad = portRect.width() - text.size().width();
        const qreal availablePortVPad = portRect.height() - text.size().height();
        painter.drawStaticText(portRect.topLeft()+QPointF(availablePortHPad/2.0, availablePortVPad/2.0), text);

        //connection point logic
        const auto connPoint = portRect.topLeft() + QPointF(portFlip?rectSize.width()+GraphObjectBorderWidth:-GraphObjectBorderWidth, rectSize.height()/2);
        _impl->inputPortPoints[i] = trans.map(connPoint);
    }

    //create output ports
    _impl->outputPortRects.resize(numOutputs);
    _impl->outputPortPoints.resize(numOutputs);
    qreal outPortVdelta = (overallHeight - outputPortsMinHeight)/2.0 + GraphBlockPortVOutterPad;
    for (size_t i = 0; i < numOutputs; i++)
    {
        const auto &text = _impl->outputPortsText.at(i);
        QSizeF rectSize = text.size() + QSizeF(GraphBlockPortTextHPad*2+GraphBlockPortArc, GraphBlockPortTextVPad*2);
        const qreal hOff = (portFlip)? 1-rectSize.width() :  overallWidth;
        const qreal arcFix = (portFlip)? GraphBlockPortArc : -GraphBlockPortArc;
        QRectF portRect(p+QPointF(hOff+arcFix, outPortVdelta), rectSize);
        outPortVdelta += rectSize.height() + GraphBlockPortVGap;
        painter.save();
        painter.setBrush(QBrush(_impl->outputPortColors.at(i)));
        painter.setPen(_impl->outputPortsBorder[i]);
        painter.drawRoundedRect(portRect, GraphBlockPortArc, GraphBlockPortArc);
        painter.restore();
        _impl->outputPortRects[i] = trans.mapRect(portRect);

        const qreal availablePortHPad = portRect.width() - text.size().width() + arcFix;
        const qreal availablePortVPad = portRect.height() - text.size().height();
        painter.drawStaticText(portRect.topLeft()+QPointF(availablePortHPad/2.0-arcFix, availablePortVPad/2.0), text);

        //connection point logic
        const auto connPoint = portRect.topLeft() + QPointF(portFlip?-GraphObjectBorderWidth:rectSize.width()+GraphObjectBorderWidth, rectSize.height()/2);
        _impl->outputPortPoints[i] = trans.map(connPoint);
    }

    //create signals port
    if (hasSignals)
    {
        QPointF connPoint;
        QRectF portRect;

        if (eventPortsInline)
        {
            QSizeF rectSize(GraphBlockSignalPortLength+GraphBlockPortArc, GraphBlockSignalPortSpan);
            const qreal hOff = (portFlip)? 1-rectSize.width() : overallWidth;
            const qreal arcFix = (portFlip)? GraphBlockPortArc : -GraphBlockPortArc;
            portRect = QRectF(p+QPointF(hOff+arcFix, outPortVdelta), rectSize);

            //center vertically on the midpoint between the top and the last output
            connPoint = portRect.topLeft() + QPointF(portFlip?-GraphObjectBorderWidth:rectSize.width()+GraphObjectBorderWidth, rectSize.height()/2);
        }
        else
        {
            QSizeF rectSize(GraphBlockSignalPortSpan, GraphBlockSignalPortLength+GraphBlockPortArc);
            const qreal vOff = (portFlip)? 1-rectSize.height() : overallHeight;
            const qreal arcFix = (portFlip)? GraphBlockPortArc : -GraphBlockPortArc;
            portRect = QRectF(p+QPointF(mainRect.width()/2-rectSize.width()/2, vOff + arcFix), rectSize);

            //center horizontally on the bottom of the main rect
            connPoint = portRect.topLeft() + QPointF(rectSize.width()/2, portFlip?-GraphObjectBorderWidth:rectSize.height()+GraphObjectBorderWidth);
        }

        painter.save();
        painter.setBrush(QBrush(_impl->mainBlockColor));
        painter.setPen(_impl->signalPortBorder);
        painter.drawRoundedRect(portRect, GraphBlockPortArc, GraphBlockPortArc);
        painter.restore();

        _impl->signalPortRect = trans.mapRect(portRect);
        _impl->signalPortPoint = trans.map(connPoint);
    }

    //create slots port
    if (hasSlots)
    {
        QPointF connPoint;

        if (eventPortsInline)
        {
            //center vertically on the midpoint between the top and the last input
            const auto hpos = inPortVdelta + GraphBlockSignalPortSpan/2;
            connPoint = mainRect.topLeft() + QPointF(portFlip?mainRect.width()+GraphObjectBorderWidth:-GraphObjectBorderWidth, hpos);
        }
        else
        {
            //center horizontally on the top of the main rect
            connPoint = mainRect.topLeft() + QPointF(mainRect.width()/2, portFlip?mainRect.height()+GraphObjectBorderWidth:-GraphObjectBorderWidth);
        }
        _impl->slotPortPoint = trans.map(connPoint);
    }
    else _impl->slotPortPoint = QPointF();

    //draw main body of the block
    painter.save();
    painter.setBrush(QBrush(_impl->mainBlockColor));
    painter.setPen(_impl->mainRectBorder);
    painter.drawRoundedRect(mainRect, GraphBlockMainArc, GraphBlockMainArc);
    painter.restore();

    //create title
    const qreal availableTitleHPad = overallWidth-_impl->titleText.size().width();
    painter.drawStaticText(p+QPointF(availableTitleHPad/2.0, GraphBlockTitleVPad), _impl->titleText);

    //create params
    qreal propVdelta = GraphBlockTitleVPad + _impl->titleText.size().height() + GraphBlockTitleVPad;
    for (const auto &text : _impl->propertiesText)
    {
        painter.drawStaticText(p+QPointF(GraphBlockPropTextHPad, propVdelta), text);
        propVdelta += GraphBlockPropTextVPad + text.size().height() + GraphBlockPropTextVPad;
    }
}

QJsonObject GraphBlock::serialize(void) const
{
    auto obj = GraphObject::serialize();
    obj["what"] = "Block";
    obj["path"] = this->getBlockDescPath();
    obj["affinityZone"] = this->getAffinityZone();
    if (not this->getActiveEditTab().isEmpty())
    {
        obj["activeEditTab"] = this->getActiveEditTab();
    }

    QJsonArray jPropsObj;
    for (const auto &propKey : this->getProperties())
    {
        QJsonObject jPropObj;
        jPropObj["key"] = propKey;
        jPropObj["value"] = this->getPropertyValue(propKey);
        auto editMode = this->getPropertyEditMode(propKey);
        if (not editMode.isEmpty()) jPropObj["editMode"] = editMode;
        jPropsObj.push_back(jPropObj);
    }
    obj["properties"] = jPropsObj;

    if (not _impl->inputDesc.isEmpty()) obj["inputDesc"] = _impl->inputDesc;
    if (not _impl->outputDesc.isEmpty()) obj["outputDesc"] = _impl->outputDesc;
    return obj;
}

void GraphBlock::deserialize(const QJsonObject &obj)
{
    const auto path = obj["path"].toString();
    const auto properties = obj["properties"].toArray();

    //init the block with the description
    const auto blockDesc = BlockCache::global()->getBlockDescFromPath(path);

    //Can't find the block description?
    //Generate a pseudo description so that the block will appear
    //in the editor with the same properties and connections.
    if (blockDesc.isEmpty())
    {
        QJsonObject blockDescFallback;
        blockDescFallback["path"] = path;
        blockDescFallback["name"] = obj["id"];

        QJsonArray blockParams;
        for (const auto &propVal : properties)
        {
            QJsonObject paramObj;
            paramObj["key"] = propVal.toObject()["key"].toString();
            blockParams.push_back(paramObj);
        }
        blockDescFallback["params"] = blockParams;
        Poco::Logger::get("PothosGui.GraphBlock.init").error("Cant find block factory with path: '%s'", path.toStdString());
        this->setBlockDesc(blockDescFallback);
    }

    else this->setBlockDesc(blockDesc);

    if (obj.contains("affinityZone")) this->setAffinityZone(obj["affinityZone"].toString());
    if (obj.contains("activeEditTab")) this->setActiveEditTab(obj["activeEditTab"].toString());

    for (const auto &propValue : properties)
    {
        const auto jPropObj = propValue.toObject();
        const auto propKey = jPropObj["key"].toString();
        this->setPropertyValue(propKey, jPropObj["value"].toString());
        this->setPropertyEditMode(propKey, jPropObj["editMode"].toString());
    }

    //load port description and init from it -- in the case eval fails
    this->setInputPortDesc(obj["inputDesc"].toArray());
    this->setOutputPortDesc(obj["outputDesc"].toArray());

    GraphObject::deserialize(obj);
}
