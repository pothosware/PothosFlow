// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "EnvironmentEval.hpp"
#include <Pothos/Proxy.hpp>
#include <Pothos/Remote.hpp>
#include <Pothos/System/Logger.hpp>
#include <Pothos/Util/Network.hpp>
#include <Poco/URI.h>
#include <Poco/Net/SocketAddress.h>

EnvironmentEval::EnvironmentEval(void):
    _failureState(false),
    _logger(Poco::Logger::get("PothosGui.EnvironmentEval"))
{
    return;
}

EnvironmentEval::~EnvironmentEval(void)
{
    return;
}

void EnvironmentEval::acceptConfig(const QString &zoneName, const QJsonObject &config)
{
    _zoneName = zoneName;
    _config = config;
}

void EnvironmentEval::update(void)
{
    if (this->isFailureState()) _env.reset();

    try
    {
        //env already exists, try test communication
        if (_env)
        {
            _env->findProxy("Pothos/Util/EvalEnvironment");
        }
        //otherwise, make a new env
        else
        {
            _env = this->makeEnvironment();
            auto EvalEnvironment = _env->findProxy("Pothos/Util/EvalEnvironment");
            _eval = EvalEnvironment.callProxy("make");
            _failureState = false;
        }
    }
    catch (const Pothos::Exception &ex)
    {
        //dont report errors if we were already in failure mode
        if (_failureState) return;
        _failureState = true;

        //determine if the remote host is offline or the process just crashed
        const auto hostUri = getHostProcFromConfig(_zoneName, _config).first;
        try
        {
            Pothos::RemoteClient client(hostUri.toStdString());
            _errorMsg = tr("Remote environment %1 crashed").arg(_zoneName);
        }
        catch(const Pothos::RemoteClientError &)
        {
            _errorMsg = tr("Remote host %1 is offline").arg(hostUri);
        }
        _logger.error("zone[%s]: %s - %s", _zoneName.toStdString(), ex.displayText(), _errorMsg.toStdString());
    }
}

HostProcPair EnvironmentEval::getHostProcFromConfig(const QString &zoneName, const QJsonObject &config)
{
    if (zoneName == "gui") return HostProcPair(QString::fromStdString("gui://"+Pothos::Util::getLoopbackAddr()), "gui");

    const auto uriDefault = QString::fromStdString("tcp://"+Pothos::Util::getLoopbackAddr());
    const auto hostUri = config["hostUri"].toString(uriDefault);
    const auto processName = config["processName"].toString("");
    return HostProcPair(hostUri, processName);
}

Pothos::ProxyEnvironment::Sptr EnvironmentEval::makeEnvironment(void)
{
    if (_zoneName == "gui") return Pothos::ProxyEnvironment::make("managed");

    const auto hostUri = getHostProcFromConfig(_zoneName, _config).first.toStdString();

    //connect to the remote host and spawn a server
    auto serverEnv = Pothos::RemoteClient(hostUri).makeEnvironment("managed");
    auto serverHandle = serverEnv->findProxy("Pothos/RemoteServer")("tcp://"+Pothos::Util::getWildcardAddr(), false/*noclose*/);

    //construct the uri for the new server
    auto actualPort = serverHandle.call<std::string>("getActualPort");
    Poco::URI newHostUri(hostUri);
    newHostUri.setPort(std::stoul(actualPort));

    //connect the client environment
    auto client = Pothos::RemoteClient(newHostUri.toString());
    client.holdRef(Pothos::Object(serverHandle));
    auto env = client.makeEnvironment("managed");

    //determine log delivery address
    //FIXME syslog listener doesn't support IPv6, special precautions taken:
    const auto logSource = (not _zoneName.isEmpty())? _zoneName.toStdString() : newHostUri.getHost();
    const auto syslogListenPort = Pothos::System::Logger::startSyslogListener();
    Poco::Net::SocketAddress serverAddr(env->getPeeringAddress(), syslogListenPort);

    //deal with IPv6 addresses because the syslog server only binds to IPv4
    if (serverAddr.family() == Poco::Net::IPAddress::IPv6)
    {
        //convert IPv6 mapped ports to IPv4 format when possible
        if (serverAddr.host().isIPv4Mapped())
        {
            const Poco::Net::IPAddress v4Mapped(static_cast<const char *>(serverAddr.host().addr())+12, 4);
            serverAddr = Poco::Net::SocketAddress(v4Mapped, std::stoi(syslogListenPort));
        }

        //convert an IPv4 loopback address into an IPv4 loopback address
        else if (serverAddr.host().isLoopback())
        {
            serverAddr = Poco::Net::SocketAddress("127.0.0.1", syslogListenPort);
        }

        //otherwise warn because the forwarding will not work
        else
        {
            _logger.warning("Log forwarding not supported over IPv6: %s", logSource);
            return env;
        }
    }

    //setup log delivery from the server process
    env->findProxy("Pothos/System/Logger").callVoid("startSyslogForwarding", serverAddr.toString());
    env->findProxy("Pothos/System/Logger").callVoid("forwardStdIoToLogging", logSource);
    serverHandle.callVoid("startSyslogForwarding", serverAddr.toString(), logSource);

    return env;
}
