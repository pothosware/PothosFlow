// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "GraphObjects/GraphBlock.hpp"
#include <QColor>
#include <QPen>
#include <QRectF>
#include <QPointF>
#include <QStaticText>
#include <vector>
#include <map>

struct GraphBlock::Impl
{
    Impl(void):
        signalPortUseCount(0),
        slotPortUseCount(0),
        showPortNames(false),
        eventPortsInline(false)
    {
        return;
    }

    Poco::JSON::Object::Ptr blockDesc;
    Poco::JSON::Object::Ptr overlayDesc;
    Poco::JSON::Array::Ptr inputDesc;
    Poco::JSON::Array::Ptr outputDesc;
    QString affinityZone;
    std::string activeEditTab;

    QStringList blockErrorMsgs;

    QString title;
    QStaticText titleText;
    QColor mainBlockColor;

    std::vector<QStaticText> propertiesText;
    std::map<QString, QString> propertiesValues;
    std::map<QString, QString> propertiesNames;
    std::map<QString, QString> propertiesEditMode;
    std::map<QString, QString> propertiesPreview;
    std::map<QString, Poco::JSON::Array::Ptr> propertiesPreviewArgs;
    std::map<QString, Poco::JSON::Object::Ptr> propertiesPreviewKwargs;
    std::map<QString, QString> propertiesErrorMsg;
    std::map<QString, std::string> propertiesTypeStr;

    std::map<QString, QString> inputPortsAliases;
    std::vector<QStaticText> inputPortsText;
    std::vector<QPen> inputPortsBorder;
    std::vector<QRectF> inputPortRects;
    std::vector<QPointF> inputPortPoints;
    std::vector<QColor> inputPortColors;
    std::map<QString, std::string> inputPortTypeStr;
    std::map<QString, size_t> inputPortUseCount;

    std::map<QString, QString> outputPortsAliases;
    std::vector<QStaticText> outputPortsText;
    std::vector<QPen> outputPortsBorder;
    std::vector<QRectF> outputPortRects;
    std::vector<QPointF> outputPortPoints;
    std::vector<QColor> outputPortColors;
    std::map<QString, std::string> outputPortTypeStr;
    std::map<QString, size_t> outputPortUseCount;

    QRectF signalPortRect;
    QPointF signalPortPoint;
    size_t signalPortUseCount;
    QPen signalPortBorder;

    QPointF slotPortPoint;
    size_t slotPortUseCount;
    QPen mainRectBorder;

    //display modes
    bool showPortNames;
    bool eventPortsInline;

    QRectF mainBlockRect;
    QPointer<QWidget> graphWidget;
};
