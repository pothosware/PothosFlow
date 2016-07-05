// Copyright (c) 2016-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/GraphEditor.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphEditor/Constants.hpp"
#include "EvalEngine/TopologyEval.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <fstream>

void GraphEditor::exportToJSONTopology(const QString &fileName)
{
    poco_information_f1(_logger, "Exporting %s", fileName.toStdString());
    Poco::JSON::Object topObj;

    //all graph objects excluding widgets which are not exported
    //we will have to filter out graphical blocks as well
    const auto graphObjects = this->getGraphObjects(~GRAPH_WIDGET);

    //global variables
    Poco::JSON::Array globals;
    for (const auto &name : this->listGlobals())
    {
        const auto &value = this->getGlobalExpression(name);
        Poco::JSON::Object globalObj;
        globalObj.set("name", name.toStdString());
        globalObj.set("value", value.toStdString());
        globals.add(globalObj);
    }
    if (globals.size() > 0) topObj.set("globals", globals);

    //thread pools (filled in by blocks loop)
    Poco::JSON::Object threadPools;
    auto affinityZones = AffinityZonesDock::global();

    //blocks
    Poco::JSON::Array blocks;
    std::map<size_t, const GraphBlock*> uidToBlock;
    for (const auto *obj : graphObjects)
    {
        const auto block = dynamic_cast<const GraphBlock *>(obj);
        if (block == nullptr) continue;
        if (block->isGraphWidget()) continue;
        uidToBlock[block->uid()] = block;

        //copy in the the id and path
        Poco::JSON::Object blockObj;
        blockObj.set("id", block->getId().toStdString());
        blockObj.set("path", block->getBlockDescPath());

        //setup the thread pool when specified
        const auto affinityZone = block->getAffinityZone();
        if (not affinityZone.isEmpty() and affinityZone != "gui")
        {
            const auto config = affinityZones->zoneToConfig(affinityZone);
            threadPools.set(affinityZone.toStdString(), config);
            blockObj.set("threadPool", affinityZone.toStdString());
        }

        //block description args are in the same format
        const auto desc = block->getBlockDesc();
        if (desc->has("args")) blockObj.set("args", desc->get("args"));

        //copy in the named calls in array format
        Poco::JSON::Array calls;
        if (desc->isArray("calls")) for (const auto &elem : *desc->getArray("calls"))
        {
            const auto callObj = elem.extract<Poco::JSON::Object::Ptr>();
            Poco::JSON::Array call;
            call.add(callObj->get("name"));
            for (const auto &arg : *callObj->getArray("args")) call.add(arg);
            calls.add(call);
        }
        blockObj.set("calls", calls);

        //copy in the parameters as local variables
        Poco::JSON::Array locals;
        for (const auto &name : block->getProperties())
        {
            Poco::JSON::Object local;
            local.set("name", name.toStdString());
            local.set("value", block->getPropertyValue(name).toStdString());
            locals.add(local);
        }
        blockObj.set("locals", locals);

        blocks.add(blockObj);
    }
    topObj.set("blocks", blocks);
    if (threadPools.size() > 0) topObj.set("threadPools", threadPools);

    //connections
    Poco::JSON::Array connections;
    for (const auto &connInfo : TopologyEval::getConnectionInfo(graphObjects))
    {
        //the block may have been filtered out
        //check that its found in the mapping
        auto srcIt = uidToBlock.find(connInfo.srcBlockUID);
        if (srcIt == uidToBlock.end()) continue;
        auto dstIt = uidToBlock.find(connInfo.dstBlockUID);
        if (dstIt == uidToBlock.end()) continue;

        //create the connection information in order
        Poco::JSON::Array connArr;
        connArr.add(srcIt->second->getId().toStdString());
        connArr.add(connInfo.srcPort);
        connArr.add(dstIt->second->getId().toStdString());
        connArr.add(connInfo.dstPort);
        connections.add(connArr);
    }
    topObj.set("connections", connections);

    //write to file
    try
    {
        std::ofstream outFile(fileName.toStdString().c_str());
        topObj.stringify(outFile, 4/*indent*/);
    }
    catch (const std::exception &ex)
    {
        poco_error_f2(_logger, "Error exporting %s: %s", fileName, std::string(ex.what()));
    }
}
