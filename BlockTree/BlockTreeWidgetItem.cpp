// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "BlockTree/BlockTreeWidgetItem.hpp"
#include <QJsonArray>

void BlockTreeWidgetItem::load(const QJsonObject &blockDesc, const QString &category, const size_t depth)
{
    const auto slashIndex = category.indexOf('/');
    const auto catName = category.mid(0, slashIndex);
    if (slashIndex == -1)
    {
        _blockDesc = blockDesc;
    }
    else
    {
        const auto catRest = category.mid(slashIndex+1);
        const auto key = catRest.mid(0, catRest.indexOf("/"));
        if (_subNodes.find(key) == _subNodes.end())
        {
            _subNodes[key] = new BlockTreeWidgetItem(this, key);
            _subNodes[key]->setExpanded(depth < 2);
        }
        _subNodes[key]->load(blockDesc, catRest, depth+1);
    }
}

//this sets a tool tip -- but only when requested
QVariant BlockTreeWidgetItem::data(int column, int role) const
{
    if (role == Qt::ToolTipRole)
    {
        //sorry about this cast
        const_cast<BlockTreeWidgetItem *>(this)->setToolTipOnRequest();
    }
    return QTreeWidgetItem::data(column, role);
}

void BlockTreeWidgetItem::setToolTipOnRequest(void)
{
    const auto doc = extractDocString(_blockDesc);
    if (doc.isEmpty()) return;
    this->setToolTip(0, doc);
}

QString BlockTreeWidgetItem::extractDocString(const QJsonObject &blockDesc)
{
    if (not blockDesc.contains("docs")) return "";
    QString output;
    output += "<b>" + blockDesc["name"].toString() + "</b>";
    output += "<p>";
    for (const auto &lineVal : blockDesc["docs"].toArray())
    {
        const auto line = lineVal.toString();
        if (line.isEmpty()) output += "<p /><p>";
        else output += line+"\n";
    }
    output += "</p>";
    return "<div>" + output + "</div>";
}
