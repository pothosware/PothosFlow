// Copyright (c) 2016-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QSettings>

/*!
 * This class contains conf settings used by various classes.
 */
class PothosGuiMainSettings : public QSettings
{
    Q_OBJECT
public:

    //! Global access to main settings
    static PothosGuiMainSettings *global(void);

    PothosGuiMainSettings(QObject *parent);
};
