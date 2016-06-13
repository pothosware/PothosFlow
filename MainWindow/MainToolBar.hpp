// Copyright (c) 2016-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QToolBar>

class PothosGuiMainActions;

/*!
 * Create the main toolbar out of main window actions.
 */
class PothosGuiMainToolBar : public QToolBar
{
    Q_OBJECT
public:

    PothosGuiMainToolBar(QWidget *parent, PothosGuiMainActions *actions);
};
