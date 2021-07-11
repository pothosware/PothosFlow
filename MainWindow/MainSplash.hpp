// Copyright (c) 2016-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QSplashScreen>

/*!
 * Customized splash screen with message hooks
 */
class MainSplash : public QSplashScreen
{
    Q_OBJECT
public:

    //! Global access to main splash
    static MainSplash *global(void);

    MainSplash(QScreen *parent);

    void postMessage(const QString &msg);
};
