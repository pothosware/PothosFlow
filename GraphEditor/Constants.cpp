// Copyright (c) 2021-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/Constants.hpp"
#include "ColorUtils/ColorUtils.hpp"
#include <QLabel>

QString defaultPaletteBackground(void)
{
    QLabel label;
    auto color = label.palette().color(QPalette::Background);
    return color.name();
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
