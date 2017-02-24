// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QTreeWidgetItem>
#include <QJsonObject>
#include <QString>
#include <map>

class BlockTreeWidgetItem : public QTreeWidgetItem
{
public:
    template <typename ParentType>
    BlockTreeWidgetItem(ParentType *parent, const QString &name):
        QTreeWidgetItem(parent, QStringList(name))
    {
        return;
    }

    void load(const QJsonObject &blockDesc, const QString &category, const size_t depth = 0);

    const QJsonObject &getBlockDesc(void) const
    {
        return _blockDesc;
    }

private:
    //this sets a tool tip -- but only when requested
    QVariant data(int column, int role) const;

    void setToolTipOnRequest(void);

    static QString extractDocString(const QJsonObject &blockDesc);

    std::map<QString, BlockTreeWidgetItem *> _subNodes;
    QJsonObject _blockDesc;
};
