// Copyright (c) 2016-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/GraphEditor.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphEditor/Constants.hpp"
#include "EvalEngine/TopologyEval.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

static QJsonValue parseArgDesc(
    const GraphBlock *block,
    const QJsonValueRef &arg,
    QSet<QString> &usedProperties)
{
    //the property is used literally without an expression
    if (block->getProperties().contains(arg.toString()))
    {
        return block->getPropertyValue(arg.toString());
    }

    //use locals to handle the evaluation
    //and record all properties needed for the expression
    for (const auto &propKey : block->getProperties())
    {
        if (arg.toString().contains(propKey))
        {
            usedProperties.insert(propKey);
        }
    }

    //its an expression, return the argument as-is
    return arg;
}

void GraphEditor::exportToJSONTopology(const QString &fileName)
{
    _logger.information("Exporting %s", fileName.toStdString());
    QJsonObject topObj;

    //all graph objects excluding widgets which are not exported
    //we will have to filter out graphical blocks as well
    const auto graphObjects = this->getGraphObjects(~GRAPH_WIDGET);

    //global variables
    QJsonArray globals;
    for (const auto &name : this->listGlobals())
    {
        const auto &value = this->getGlobalExpression(name);
        QJsonObject globalObj;
        globalObj["name"] = name;
        globalObj["value"] = value;
        globals.push_back(globalObj);
    }
    if (not globals.isEmpty()) topObj["globals"] = globals;

    //thread pools (filled in by blocks loop)
    QJsonObject threadPools;
    auto affinityZones = AffinityZonesDock::global();

    //blocks
    QJsonArray blocks;
    std::map<size_t, const GraphBlock*> uidToBlock;
    for (const auto *obj : graphObjects)
    {
        const auto block = dynamic_cast<const GraphBlock *>(obj);
        if (block == nullptr) continue;
        if (not block->isEnabled()) continue;
        if (block->isGraphWidget()) continue;
        uidToBlock[block->uid()] = block;

        //copy in the the id and path
        QJsonObject blockObj;
        blockObj["id"] = block->getId();
        blockObj["path"] = block->getBlockDescPath();

        //setup the thread pool when specified
        const auto affinityZone = block->getAffinityZone();
        if (not affinityZone.isEmpty() and affinityZone != "gui")
        {
            const auto config = affinityZones->zoneToConfig(affinityZone);
            threadPools[affinityZone] = config;
            blockObj["threadPool"] = affinityZone;
        }

        QSet<QString> usedProperties;

        //copy in the constructor args in array format
        const auto desc = block->getBlockDesc();
        if (desc.contains("args"))
        {
            QJsonArray args;
            for (const auto &arg : desc["args"].toArray())
            {
                args.push_back(parseArgDesc(block, arg, usedProperties));
            }
            blockObj["args"] = args;
        }

        //copy in the named calls in array format
        QJsonArray calls;
        for (const auto &callVal : desc["calls"].toArray())
        {
            const auto callObj = callVal.toObject();
            QJsonArray call;
            call.push_back(callObj["name"]);
            for (const auto &arg : callObj["args"].toArray())
            {
                call.push_back(parseArgDesc(block, arg, usedProperties));
            }
            calls.push_back(call);
        }
        blockObj["calls"] = calls;

        //copy in the parameters as local variables
        QJsonArray locals;
        for (const auto &name : usedProperties)
        {
            QJsonObject local;
            local["name"] = name;
            local["value"] = block->getPropertyValue(name);
            locals.push_back(local);
        }
        if (not locals.isEmpty()) blockObj["locals"] = locals;

        blocks.push_back(blockObj);
    }
    topObj["blocks"] = blocks;
    if (not threadPools.isEmpty()) topObj["threadPools"] = threadPools;

    //connections
    QJsonArray connections;
    for (const auto &connInfo : TopologyEval::getConnectionInfo(graphObjects))
    {
        //the block may have been filtered out
        //check that its found in the mapping
        auto srcIt = uidToBlock.find(connInfo.srcBlockUID);
        if (srcIt == uidToBlock.end()) continue;
        auto dstIt = uidToBlock.find(connInfo.dstBlockUID);
        if (dstIt == uidToBlock.end()) continue;

        //create the connection information in order
        QJsonArray connArr;
        connArr.push_back(srcIt->second->getId());
        connArr.push_back(connInfo.srcPort);
        connArr.push_back(dstIt->second->getId());
        connArr.push_back(connInfo.dstPort);
        connections.push_back(connArr);
    }
    topObj["connections"] = connections;

    //export to byte array
    const QJsonDocument jsonDoc(topObj);
    const auto data = jsonDoc.toJson(QJsonDocument::Indented);

    //write to file
    QFile jsonFile(fileName);
    if (not jsonFile.open(QFile::WriteOnly) or jsonFile.write(data) == -1)
    {
        _logger.error("Error exporting %s: %s", fileName.toStdString(), jsonFile.errorString().toStdString());
    }
}
