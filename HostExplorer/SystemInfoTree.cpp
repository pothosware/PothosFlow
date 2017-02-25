// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "HostExplorer/SystemInfoTree.hpp"
#include <Pothos/Remote.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/System.hpp>
#include <QJsonDocument>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <Poco/Logger.h>

/***********************************************************************
 * information aquisition
 **********************************************************************/
static InfoResult getInfo(const std::string &uriStr)
{
    InfoResult info;
    POTHOS_EXCEPTION_TRY
    {
        auto env = Pothos::RemoteClient(uriStr).makeEnvironment("managed");
        info.hostInfo = env->findProxy("Pothos/System/HostInfo").call<Pothos::System::HostInfo>("get");
        info.numaInfo = env->findProxy("Pothos/System/NumaInfo").call<std::vector<Pothos::System::NumaInfo>>("get");
        auto deviceInfo = env->findProxy("Pothos/Util/DeviceInfoUtils").call<std::string>("dumpJson");
        const QByteArray devInfoBytes(deviceInfo.data(), deviceInfo.size());
        QJsonParseError errorParser;
        const auto jsonDoc = QJsonDocument::fromJson(devInfoBytes, &errorParser);
        if (jsonDoc.isNull()) throw Poco::Exception(errorParser.errorString().toStdString());
        info.deviceInfo = jsonDoc.array();
    }
    POTHOS_EXCEPTION_CATCH(const Pothos::Exception &ex)
    {
        poco_error_f2(Poco::Logger::get("PothosGui.SystemInfoTree"), "Failed to query system info %s - %s", uriStr, ex.displayText());
    }
    return info;
}

/***********************************************************************
 * system info tree widget implementation
 **********************************************************************/
SystemInfoTree::SystemInfoTree(QWidget *parent):
    QTreeWidget(parent),
    _watcher(new QFutureWatcher<InfoResult>(this))
{
    QStringList columnNames;
    columnNames.push_back(tr("Name"));
    columnNames.push_back(tr("Value"));
    columnNames.push_back(tr("Unit"));
    this->setColumnCount(columnNames.size());
    this->setHeaderLabels(columnNames);

    connect(
        _watcher, SIGNAL(finished(void)),
        this, SLOT(handleWatcherDone(void)));
}

void SystemInfoTree::handeInfoRequest(const std::string &uriStr)
{
    if (_watcher->isRunning()) return;
    while (this->topLevelItemCount() > 0) delete this->topLevelItem(0);
    _watcher->setFuture(QtConcurrent::run(std::bind(&getInfo, uriStr)));
    emit startLoad();
}

void SystemInfoTree::handleWatcherDone(void)
{
    const auto info = _watcher->result();

    const auto &hostInfo = info.hostInfo;
    {
        QStringList columns;
        columns.push_back(tr("Host Info"));
        auto rootItem = new QTreeWidgetItem(this, columns);
        rootItem->setExpanded(true);
        makeEntry(rootItem, "OS Name", QString::fromStdString(hostInfo.osName));
        makeEntry(rootItem, "OS Version", QString::fromStdString(hostInfo.osVersion));
        makeEntry(rootItem, "OS Architecture", QString::fromStdString(hostInfo.osArchitecture));
        makeEntry(rootItem, "Node Name", QString::fromStdString(hostInfo.nodeName));
        makeEntry(rootItem, "Node ID", QString::fromStdString(hostInfo.nodeId));
        makeEntry(rootItem, "Processors", QString::number(hostInfo.processorCount), "CPUs");
    }

    for (const auto &numaInfo : info.numaInfo)
    {
        QStringList columns;
        columns.push_back(tr("NUMA Node %1 Info").arg(numaInfo.nodeNumber));
        auto rootItem = new QTreeWidgetItem(this, columns);
        rootItem->setExpanded(numaInfo.nodeNumber == 0);
        if (numaInfo.totalMemory != 0) makeEntry(rootItem, "Total Memory", QString::number(numaInfo.totalMemory/1024/1024), "MB");
        if (numaInfo.freeMemory != 0) makeEntry(rootItem, "Free Memory", QString::number(numaInfo.freeMemory/1024/1024), "MB");
        QString cpuStr;
        for (auto i : numaInfo.cpus)
        {
            if (not cpuStr.isEmpty()) cpuStr += ", ";
            cpuStr += QString::number(i);
        }
        makeEntry(rootItem, "CPUs", cpuStr);
    }

    //adjust value column before arbitrary values from device info
    this->resizeColumnToContents(1);

    for (const auto &infoVal : info.deviceInfo)
    {
        this->loadJsonObject(this, "", infoVal.toObject(), true/*expand*/);
    }

    //adjust names and units columns after all information is loaded
    this->resizeColumnToContents(0);
    //this->resizeColumnToContents(1);
    this->resizeColumnToContents(2);
    emit stopLoad();
}
