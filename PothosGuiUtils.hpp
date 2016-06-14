// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QString>
#include <QMap>
#include <QIcon>

class QWidget;
class QFormLayout;

//! icon utils
QString makeIconPath(const QString &name);
QIcon makeIconFromTheme(const QString &name);

//! make a form layout with consistent style across platforms
QFormLayout *makeFormLayout(QWidget *parent = nullptr);
