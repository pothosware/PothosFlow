// Copyright (c) 2014-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/GraphEditor.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include "GraphEditor/Constants.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtAlgorithms>

/***********************************************************************
 * Serialization routine
 **********************************************************************/
QByteArray GraphEditor::dumpState(void) const
{
    QJsonObject topObj;

    //store global variables
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

    //store other graph config
    QJsonObject config;
    if (_autoActivate) config["autoActivate"] = _autoActivate;
    if (_lockTopology) config["lockTopology"] = _lockTopology;
    if (_sceneSize.isValid())
    {
        config["graphWidth"] = _sceneSize.width();
        config["graphHeight"] = _sceneSize.height();
    }
    if (not config.isEmpty()) topObj["config"] = config;

    //store pages
    QJsonArray pages;
    for (int pageNo = 0; pageNo < this->count(); pageNo++)
    {
        QJsonObject page;
        QJsonArray graphObjects;
        page["pageName"] = this->tabText(pageNo);
        page["selected"] = (this->currentIndex() == pageNo);
        page["docked"] = this->isDocked(pageNo);
        auto geometry = this->saveGeometry(pageNo);
        if (not geometry.isEmpty()) page["geometry"] = QString(geometry.toBase64());

        //serialize an object list sorted by ID for each major graph object type
        for (const auto &graphType : {GRAPH_BLOCK, GRAPH_BREAKER, GRAPH_CONNECTION, GRAPH_WIDGET})
        {
            auto draw = this->getGraphDraw(pageNo);
            auto objs = draw->getGraphObjects(graphType);
            std::sort(objs.begin(), objs.end(), [](const GraphObject *lhs, const GraphObject *rhs)
            {
                return QString::compare(lhs->getId(), rhs->getId()) < 0;
            });
            for (auto obj : objs) graphObjects.push_back(obj->serialize());
        }

        page["graphObjects"] = graphObjects;
        pages.push_back(page);
    }
    topObj["pages"] = pages;

    //export to byte array
    const QJsonDocument jsonDoc(topObj);
    return jsonDoc.toJson(QJsonDocument::Indented);
}
