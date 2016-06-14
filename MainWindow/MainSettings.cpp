// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainSettings.hpp"
#include <Pothos/System.hpp>
#include <Poco/Path.h>

static MainSettings *globalMainSettings = nullptr;

MainSettings *MainSettings::global(void)
{
    return globalMainSettings;
}

static QString getSettingsPath(void)
{
    Poco::Path path(Pothos::System::getUserConfigPath());
    path.append("PothosGui.conf");
    return QString::fromStdString(path.toString());
}

MainSettings::MainSettings(QObject *parent):
    QSettings(getSettingsPath(), QSettings::IniFormat, parent)
{
    globalMainSettings = this;
}
