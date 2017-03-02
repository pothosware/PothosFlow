// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/GraphEditor.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphObjects/GraphBreaker.hpp"
#include "GraphObjects/GraphConnection.hpp"
#include "GraphObjects/GraphWidget.hpp"
#include <Pothos/Exception.hpp>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <Poco/Logger.h>
#include <cassert>

/***********************************************************************
 * Per-type creation routine
 **********************************************************************/
void GraphEditor::loadPages(const QJsonArray &pages, const QString &type)
{
    for (int pageNo = 0; pageNo < pages.size(); pageNo++)
    {
        const auto pageObj = pages.at(pageNo).toObject();
        const auto graphObjects = pageObj["graphObjects"].toArray();
        auto parent = this->widget(pageNo);

        for (const auto &graphVal : graphObjects)
        {
            GraphObject *obj = nullptr;
            const auto jGraphObj = graphVal.toObject();
            if (jGraphObj.isEmpty()) continue;
            const auto what = jGraphObj["what"].toString();
            if (what != type) continue;
            POTHOS_EXCEPTION_TRY
            {
                if (type == "Block") obj = new GraphBlock(parent);
                if (type == "Breaker") obj = new GraphBreaker(parent);
                if (type == "Connection") obj = new GraphConnection(parent);
                if (type == "Widget") obj = new GraphWidget(parent);
                if (obj != nullptr) obj->deserialize(jGraphObj);
            }
            POTHOS_EXCEPTION_CATCH(const Pothos::Exception &ex)
            {
                _logger.error("Error creating %s(%s): %s", type.toStdString(),
                    jGraphObj["what"].toString().toStdString(), ex.displayText());
                delete obj;
            }
        }
    }
}

/***********************************************************************
 * Deserialization routine
 **********************************************************************/
void GraphEditor::loadState(const QByteArray &data)
{
    QJsonParseError parseError;
    const auto jsonDoc = QJsonDocument::fromJson(data, &parseError);
    if (jsonDoc.isNull())
    {
        _logger.error("Error parsing JSON: %s", parseError.errorString().toStdString());
        return;
    }

    //extract topObj, old style is page array only
    QJsonObject topObj;
    if (jsonDoc.isArray()) topObj["pages"] = jsonDoc.array();
    else topObj = jsonDoc.object();

    //extract global variables
    this->clearGlobals();
    for (const auto &globalVal : topObj["globals"].toArray())
    {
        const auto globalObj = globalVal.toObject();
        if (not globalObj.contains("name")) continue;
        if (not globalObj.contains("value")) continue;
        this->setGlobalExpression(globalObj["name"].toString(), globalObj["value"].toString());
    }

    ////////////////////////////////////////////////////////////////////
    // clear existing stuff
    ////////////////////////////////////////////////////////////////////
    for (int pageNo = 0; pageNo < this->count(); pageNo++)
    {
        for (auto graphObj : this->getGraphDraw(pageNo)->getGraphObjects())
        {
            delete graphObj;
        }

        //delete page later so we dont mess up the tabs
        this->widget(pageNo)->deleteLater();
    }
    this->clear(); //removes all tabs from this widget

    ////////////////////////////////////////////////////////////////////
    // create pages
    ////////////////////////////////////////////////////////////////////
    const auto pages = topObj["pages"].toArray();
    for (int pageNo = 0; pageNo < pages.size(); pageNo++)
    {
        const auto pageObj = pages.at(pageNo).toObject();
        const auto pageName = pageObj["pageName"].toString();
        this->insertTab(pageNo, new GraphDraw(this), pageName);
        if (pageObj["selected"].toBool(false)) this->setCurrentIndex(pageNo);
    }

    ////////////////////////////////////////////////////////////////////
    // create graph objects
    ////////////////////////////////////////////////////////////////////
    this->loadPages(pages, "Block");
    this->loadPages(pages, "Breaker");
    this->loadPages(pages, "Connection");
    this->loadPages(pages, "Widget");
}
