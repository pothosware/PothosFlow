// Copyright (c) 2016-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QSplashScreen>

/*!
 * Customized splash screen with message hooks
 */
class PothosGuiMainSplash : public QSplashScreen
{
    Q_OBJECT
public:

    //! Global access to main splash
    static PothosGuiMainSplash *global(void);

    PothosGuiMainSplash(QWidget *parent);

    void postMessage(const QString &msg);
};
