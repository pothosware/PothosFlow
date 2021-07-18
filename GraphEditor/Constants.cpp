// Copyright (c) 2021-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/Constants.hpp"
#include "ColorUtils/ColorUtils.hpp"
#include <QLabel>

static QColor getDefaultPaletteColor(const QPalette::ColorRole role)
{
    return QLabel().palette().color(role);
}

QString defaultPaletteBackground(void)
{
    const static QString color = getDefaultPaletteColor(QPalette::Window).name();
    return color;
}

QString defaultPaletteForeground(void)
{
    const static QString color = getDefaultPaletteColor(QPalette::WindowText).name();
    return color;
}

static bool isDarkMode(void)
{
    const static bool isDark = isColorDark(defaultPaletteBackground());
    return isDark;
}

QString darkColorSupport(const QString &light, const QString &dark)
{
    return isDarkMode()?dark:light;
}
