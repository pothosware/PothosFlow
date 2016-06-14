// Copyright (c) 2016-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QToolBar>

class MainActions;

/*!
 * Create the main toolbar out of main window actions.
 */
class MainToolBar : public QToolBar
{
    Q_OBJECT
public:

    MainToolBar(QWidget *parent, MainActions *actions);
};
