// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QString>
#include <QIcon>

//! Get the icon file path given a icon name
QString makeIconPath(const QString &name);

//! Get the Icon Object given a icon name
QIcon makeIconFromTheme(const QString &name);
