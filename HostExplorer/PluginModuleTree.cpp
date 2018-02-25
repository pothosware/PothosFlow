// Copyright (c) 2013-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "HostExplorer/PluginModuleTree.hpp"
#include <Pothos/System/Version.hpp> //POTHOS_API_VERSION
#include <Pothos/Remote.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Plugin.hpp>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <Poco/Logger.h>
#include <map>
#include <functional> //std::bind

#if POTHOS_API_VERSION >= 0x00070000
#define HAS_MODULE_VERSION
#endif

/***********************************************************************
 * recursive algorithm to create widget information
 **********************************************************************/
static void loadModuleMap(PluginModuleTree::ModInfoType &info, const Pothos::PluginRegistryInfoDump &dump)
{
    if (not dump.objectType.empty())
    {
        info.modMap[dump.modulePath].push_back(dump.pluginPath);
        #ifdef HAS_MODULE_VERSION
        info.modVers[dump.modulePath] = dump.moduleVersion;
        #endif
    }

    for (const auto &subInfo : dump.subInfo)
    {
        loadModuleMap(info, subInfo);
    }
}

/***********************************************************************
 * information aquisition
 **********************************************************************/
static PluginModuleTree::ModInfoType getRegistryDump(const std::string &uriStr)
{
    PluginModuleTree::ModInfoType info;
    try
    {
        auto env = Pothos::RemoteClient(uriStr).makeEnvironment("managed");
        const Pothos::PluginRegistryInfoDump dump = env->findProxy("Pothos/PluginRegistry").call("dump");
        loadModuleMap(info, dump);
    }
    catch (const Pothos::Exception &ex)
    {
        static auto &logger = Poco::Logger::get("PothosFlow.PluginModuleTree");
        logger.error("Failed to dump registry %s - %s", uriStr, ex.displayText());
    }
    return info;
}

/***********************************************************************
 * plugin module tree widget
 **********************************************************************/
PluginModuleTree::PluginModuleTree(QWidget *parent):
    QTreeWidget(parent),
    _watcher(new QFutureWatcher<ModInfoType>(this))
{
    QStringList columnNames;
    columnNames.push_back(tr("Plugin Path"));
    columnNames.push_back(tr("Count"));
    #ifdef HAS_MODULE_VERSION
    columnNames.push_back(tr("Version"));
    #endif
    this->setColumnCount(columnNames.size());
    this->setHeaderLabels(columnNames);

    connect(
        _watcher, SIGNAL(finished(void)),
        this, SLOT(handleWatcherDone(void)));
}

void PluginModuleTree::handeInfoRequest(const std::string &uriStr)
{
    if (_watcher->isRunning()) return;
    while (this->topLevelItemCount() > 0) delete this->topLevelItem(0);
    _watcher->setFuture(QtConcurrent::run(std::bind(&getRegistryDump, uriStr)));
    emit startLoad();
}

void PluginModuleTree::handleWatcherDone(void)
{
    const auto info = _watcher->result();

    for (const auto &entry : info.modMap)
    {
        std::string name = entry.first;
        if (name.empty()) name = "Builtin";
        const auto &pluginPaths = entry.second;
        QStringList columns;
        columns.push_back(QString::fromStdString(name));
        columns.push_back(QString("%1").arg(pluginPaths.size()));
        #ifdef HAS_MODULE_VERSION
        columns.push_back(QString::fromStdString(info.modVers.at(entry.first)));
        #endif
        auto modRoot = new QTreeWidgetItem(this, columns);
        modRoot->setExpanded(pluginPaths.size() < 21);
        for (size_t i = 0; i < pluginPaths.size(); i++)
        {
            new QTreeWidgetItem(modRoot, QStringList(QString::fromStdString(pluginPaths[i])));
        }
    }

    for (int i = 0; i < this->columnCount(); i++)
        this->resizeColumnToContents(i);
    emit stopLoad();
}
