// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"

QString makeIconPath(const QString &name)
{
    return ":/icons/"+name;
}

QIcon makeIconFromTheme(const QString &name)
{
    return QIcon::fromTheme(name, QIcon(makeIconPath(name+".png")));
}
