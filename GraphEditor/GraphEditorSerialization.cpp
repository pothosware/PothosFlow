// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/GraphEditor.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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
    if (not config.isEmpty()) topObj["config"] = config;

    //store pages
    QJsonArray pages;
    for (int pageNo = 0; pageNo < this->count(); pageNo++)
    {
        QJsonObject page;
        QJsonArray graphObjects;
        page["pageName"] = this->tabText(pageNo);
        page["selected"] = (this->currentIndex() == pageNo);

        for (auto graphObj : this->getGraphDraw(pageNo)->getGraphObjects())
        {
            graphObjects.push_back(graphObj->serialize());
        }

        page["graphObjects"] = graphObjects;
        pages.push_back(page);
    }
    topObj["pages"] = pages;

    //export to byte array
    const QJsonDocument jsonDoc(topObj);
    return jsonDoc.toJson(QJsonDocument::Indented);
}
