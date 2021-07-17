// Copyright (c) 2013-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QTreeWidget>
#include <QFutureWatcher>
#include <QTreeWidgetItem>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <Pothos/System.hpp>
#include <string>

struct InfoResult
{
    Pothos::System::HostInfo hostInfo;
    std::vector<Pothos::System::NumaInfo> numaInfo;
    QJsonArray deviceInfo;
};

//! tree widget display for a host's system info
class SystemInfoTree : public QTreeWidget
{
    Q_OBJECT
public:
    SystemInfoTree(QWidget *parent);

signals:
    void startLoad(void);
    void stopLoad(void);

public slots:
    void handleInfoRequest(const std::string &uriStr);

private slots:

    void handleWatcherDone(void);

private:

    template <typename Parent>
    static QTreeWidgetItem *makeEntry(Parent *root, const QString &name, const QString &value, const char *unit = "")
    {
        QStringList columns;
        columns.push_back(name);
        columns.push_back(value);
        columns.push_back(unit);
        return new QTreeWidgetItem(root, columns);
    }

    template <typename Parent>
    void loadJsonObject(Parent *root, const QString &rootName, const QJsonObject &obj, const bool expand = false)
    {
        for (const auto &name : obj.keys())
        {
            QString newName = name;
            if (not rootName.isEmpty()) newName = rootName + " " + name;
            loadJsonVar(root, newName, obj[name], expand);
        }
    }

    template <typename Parent>
    void loadJsonArray(Parent *root, const QString &rootName, const QJsonArray &arr, const bool expand = false)
    {
        for (int i = 0; i < arr.size(); i++)
        {
            loadJsonVar(root, rootName + " " + QString::number(i), arr.at(i), expand and (i == 0));
        }
    }

    template <typename Parent>
    void loadJsonVar(Parent *root, const QString &rootName, const QJsonValue &var, const bool expand = false)
    {
        if (var.isArray())
        {
            this->loadJsonArray(root, rootName, var.toArray(), expand);
        }
        else if (var.isObject())
        {
            QStringList columns;
            columns.push_back(rootName);
            auto rootItem = new QTreeWidgetItem(root, columns);
            rootItem->setExpanded(expand);
            this->loadJsonObject(rootItem, "", var.toObject());
        }
        else
        {
            auto entry = makeEntry(root, rootName, var.toVariant().toString());
            entry->setExpanded(expand);
        }
    }

    QFutureWatcher<InfoResult> *_watcher;
};
