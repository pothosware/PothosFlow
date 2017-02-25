// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <Pothos/Proxy/Environment.hpp>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <memory>
#include <utility>
#include <Poco/Logger.h>

typedef std::pair<QString, QString> HostProcPair;

class EnvironmentEval : public QObject
{
    Q_OBJECT
public:

    EnvironmentEval(void);

    ~EnvironmentEval(void);

    /*!
     * Called under re-eval to apply the latest config.
     * This call should take the info and not process.
     */
    void acceptConfig(const QString &zoneName, const QJsonObject &config);

    /*!
     * Deal with changes from the latest config.
     */
    void update(void);

    //! Shared method to parse the zone config into host uri and process name
    static HostProcPair getHostProcFromConfig(const QString &zoneName, const QJsonObject &config);

    //! Get access to the proxy environment
    Pothos::ProxyEnvironment::Sptr getEnv(void) const
    {
        return _env;
    }

    //! Get access to the expression evaluator
    Pothos::Proxy getEval(void) const
    {
        return _eval;
    }

    //! An error caused the environment to go into failure state
    bool isFailureState(void) const
    {
        return _failureState;
    }

    //! Get the error message associated with the failure state
    const QString &getErrorMsg(void) const
    {
        return _errorMsg;
    }

private:
    Pothos::ProxyEnvironment::Sptr makeEnvironment(void);

    QString _zoneName;
    QJsonObject _config;
    Pothos::ProxyEnvironment::Sptr _env;
    Pothos::Proxy _eval;
    bool _failureState;
    QString _errorMsg;
    Poco::Logger &_logger;
};
