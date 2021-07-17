// Copyright (c) 2013-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"
#include "HostExplorer/HostExplorerDock.hpp"
#include "HostExplorer/HostSelectionTable.hpp"
#include "HostExplorer/PluginModuleTree.hpp"
#include "HostExplorer/PluginRegistryTree.hpp"
#include "HostExplorer/SystemInfoTree.hpp"
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QMovie>
#include <QTabBar>
#include <Pothos/Remote.hpp>
#include <iostream>

/***********************************************************************
 * top level window for host info implementation
 **********************************************************************/
HostExplorerDock::HostExplorerDock(QWidget *parent):
    QDockWidget(parent),
    _table(new HostSelectionTable(this)),
    _tabs(new QTabWidget(this))
{
    this->setObjectName("HostExplorerDock");
    this->setWindowTitle(tr("Host Explorer"));
    this->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    this->setWidget(new QWidget(this));

    auto layout = new QVBoxLayout(this->widget());
    layout->addWidget(_table);

    addTabAndConnect(new SystemInfoTree(_tabs), tr("System Info"));
    addTabAndConnect(new PluginRegistryTree(_tabs), tr("Plugin Registry"));
    addTabAndConnect(new PluginModuleTree(_tabs), tr("Plugin Modules"));
    layout->addWidget(_tabs, 1);

    connect(_table, &HostSelectionTable::hostUriListChanged, this, &HostExplorerDock::hostUriListChanged);
}

template <typename T>
void HostExplorerDock::addTabAndConnect(T *tabWidget, const QString &name)
{
    const auto index = _tabs->addTab(tabWidget, name);
    connect(_table, &HostSelectionTable::hostInfoRequest, tabWidget, &T::handleInfoRequest);
    connect(tabWidget, &T::startLoad, [=](void){emit this->start(index);});
    connect(tabWidget, &T::stopLoad, [=](void){emit this->stop(index);});
}

QStringList HostExplorerDock::hostUriList(void) const
{
    return _table->hostUriList();
}

void HostExplorerDock::start(const int index)
{
    auto label = new QLabel(_tabs);
    auto movie = new QMovie(makeIconPath("loading.gif"), QByteArray(), label);
    label->setMovie(movie);
    movie->start();
    _tabs->tabBar()->setTabButton(index, QTabBar::LeftSide, label);
}

void HostExplorerDock::stop(const int index)
{
    _tabs->tabBar()->setTabButton(index, QTabBar::LeftSide, new QLabel(_tabs));
}
