// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainSettings.hpp"
#include <Pothos/System.hpp>
#include <QDir>

static MainSettings *globalMainSettings = nullptr;

MainSettings *MainSettings::global(void)
{
    return globalMainSettings;
}

static QString getSettingsPath(void)
{
    const auto confPath = Pothos::System::getUserConfigPath();
    const QDir confDir(QString::fromStdString(confPath));
    return confDir.absoluteFilePath("PothosGui.conf");
}

MainSettings::MainSettings(QObject *parent):
    QSettings(getSettingsPath(), QSettings::IniFormat, parent)
{
    globalMainSettings = this;
}
