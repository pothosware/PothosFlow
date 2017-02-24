// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QIcon>
#include <QColor>
#include <QString>
#include <string>
#include <map>

//! Convert the type name to a displayable color
QColor typeStrToColor(const std::string &typeStr);

inline QColor typeStrToColor(const QString &typeStr)
{
    return typeStrToColor(typeStr.toStdString());
}

//! Get the map of known type strings to colors
std::map<std::string, QColor> getTypeStrToColorMap(void);

//! Create a decorative icon of the color
QIcon colorToWidgetIcon(const QColor &color);
