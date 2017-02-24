// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphObjects/GraphBlockImpl.hpp"
#include <QWidget>
#include <Pothos/Proxy.hpp>
#include <Poco/Logger.h>

/***********************************************************************
 * initialize the block's properties
 **********************************************************************/
void GraphBlock::setBlockDesc(const QJsonObject &blockDesc)
{
    if (_impl->blockDesc == blockDesc) return;
    _impl->blockDesc = blockDesc;

    //extract the name or title from the description
    if (not blockDesc.contains("name"))
    {
        poco_error(Poco::Logger::get("PothosGui.GraphBlock.init"), "Block missing 'name'");
        return;
    }
    this->setTitle(blockDesc["name"].toString());

    //reload properties description, clear the old first
    _properties.clear();

    //extract the params or properties from the description
    for (const auto &paramValue : blockDesc["params"].toArray())
    {
        const auto param = paramValue.toObject();
        if (not param.contains("key"))
        {
            Poco::Logger::get("PothosGui.GraphBlock.init").error("Block '%s' param missing 'key'", this->getTitle().toStdString());
            return;
        }
        const auto key = param["key"].toString();
        const auto name = param["name"].toString();
        this->addProperty(key);
        this->setPropertyName(key, name);

        if (param.contains("default"))
        {
            this->setPropertyValue(key, param["default"].toString());
        }
        else if (not param["options"].toArray().isEmpty())
        {
            auto opt0 = param["options"].toArray().at(0).toObject();
            if (not opt0.contains("value"))
            {
                Poco::Logger::get("PothosGui.GraphBlock.init").warning("Block '%s' [param %s] missing 'value'", this->getTitle().toStdString(), name.toStdString());
            }
            else this->setPropertyValue(key, opt0["value"].toString());
        }

        if (param.contains("preview"))
        {
            this->setPropertyPreviewMode(key, param["preview"].toString(),
                param["previewArgs"].toArray(), param["previewKwargs"].toObject());
        }
    }
}

/***********************************************************************
 * initialize the block's input ports
 **********************************************************************/
void GraphBlock::setInputPortDesc(const QJsonArray &inputDesc)
{
    if (_impl->inputDesc == inputDesc) return;
    _impl->inputDesc = inputDesc;

    //reload the port descriptions, clear the old first
    _inputPorts.clear();
    _slotPorts.clear();

    //reload inputs (and slots)
    for (const auto &inputPortDesc : inputDesc)
    {
        const auto &info = inputPortDesc.toObject();
        auto portKey = info["name"].toString();
        const auto portAlias = info["alias"].toString(portKey);
        if (info["isSigSlot"].toBool(false)) this->addSlotPort(portKey);
        else this->addInputPort(portKey, portAlias);
        if (info.contains("dtype")) this->setInputPortTypeStr(portKey, info["dtype"].toString());
    }
}

/***********************************************************************
 * initialize the block's output ports
 **********************************************************************/
void GraphBlock::setOutputPortDesc(const QJsonArray &outputDesc)
{
    if (_impl->outputDesc == outputDesc) return;
    _impl->outputDesc = outputDesc;

    //reload the port descriptions, clear the old first
    _outputPorts.clear();
    _signalPorts.clear();

    //reload outputs (and signals)
    for (const auto &outputPortDesc : outputDesc)
    {
        const auto &info = outputPortDesc.toObject();
        auto portKey = info["name"].toString();
        const auto portAlias = info["alias"].toString(portKey);
        if (info["isSigSlot"].toBool(false)) this->addSignalPort(portKey);
        else this->addOutputPort(portKey, portAlias);
        if (info.contains("dtype")) this->setOutputPortTypeStr(portKey, info["dtype"].toString());
    }
}
