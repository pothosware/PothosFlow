// Copyright (c) 2014-2021 Josh Blum
//                    2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include "BlockEval.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "ThreadPoolEval.hpp"
#include "EnvironmentEval.hpp"
#include "EvalTracer.hpp"
#include <Pothos/Proxy.hpp>
#include <Pothos/Framework.hpp>
#include <QJsonDocument>
#include <QWidget>
#include <cassert>
#include <iostream>
#include <QApplication>
#include <QRegExp>
#include <QSet>

//! Number of milliseconds until the overlay is considered expired
static const int OVERLAY_EXPIRED_MS = 5000;

//! helper to convert the port info vector into JSON for serialization of the block
static QJsonArray portInfosToJSON(const std::vector<Pothos::PortInfo> &infos)
{
    QJsonArray array;
    for (const auto &info : infos)
    {
        QJsonObject portInfo;
        portInfo["name"] = QString::fromStdString(info.name);
        portInfo["alias"] = QString::fromStdString(info.alias);
        portInfo["isSigSlot"] = info.isSigSlot;
        portInfo["size"] = int(info.dtype.size());
        portInfo["dtype"] = QString::fromStdString(info.dtype.toMarkup());
        array.push_back(portInfo);
    }
    return array;
}

BlockEval::BlockEval(void):
    _queryPortDesc(false),
    _logger(Poco::Logger::get("PothosFlow.BlockEval"))
{
    qRegisterMetaType<BlockStatus>("BlockStatus");
    this->moveToThread(QApplication::instance()->thread());
}

BlockEval::~BlockEval(void)
{
    return;
}

bool BlockEval::isInfoMatch(const BlockInfo &info) const
{
    if (info.id != _newBlockInfo.id) return false;
    if (info.desc.isEmpty()) return false;
    if (_newBlockInfo.desc.isEmpty()) return false;
    return info.desc["path"] == _newBlockInfo.desc["path"];
}

bool BlockEval::isReady(void) const
{
    if (not _proxyBlock) return false;

    if (not _newBlockInfo.enabled) return false;

    if (not _lastBlockStatus.blockErrorMsgs.empty()) return false;

    for (const auto &pair : _lastBlockStatus.propertyErrorMsgs)
    {
        if (not pair.second.isEmpty()) return false;
    }

    return true;
}

bool BlockEval::shouldDisconnect(void) const
{
    //there is no block, no disconnect occurs
    if (not _proxyBlock) return false;

    //disconnect a block that was disabled recently
    if (not _newBlockInfo.enabled) return true;

    //moved eval environments
    if (_newEnvironmentEval != _lastEnvironmentEval) return true;

    //critical change, the block will be torn down
    if (this->hasCriticalChange()) return true;

    return false;
}

bool BlockEval::portExists(const QString &name, const bool isInput) const
{
    const auto portDesc = isInput?_lastBlockStatus.inPortDesc:_lastBlockStatus.outPortDesc;
    if (portDesc.isSpecified())
    {
        for (const auto &val : portDesc.value())
        {
            if (val.toObject()["name"].toString() == name) return true;
        }
    }
    return false;
}

Pothos::Proxy BlockEval::getProxyBlock(void) const
{
    return _proxyBlock;
}

void BlockEval::acceptInfo(const BlockInfo &info)
{
    _newBlockInfo = info;
    _lastBlockStatus.block = _newBlockInfo.block;
}

void BlockEval::acceptEnvironment(const std::shared_ptr<EnvironmentEval> &env)
{
    _newEnvironmentEval = env;
}

void BlockEval::acceptThreadPool(const std::shared_ptr<ThreadPoolEval> &tp)
{
    _newThreadPoolEval = tp;
}

void BlockEval::update(void)
{
    EVAL_TRACER_FUNC_ARG(_newBlockInfo.id);
    _newEnvironment = _newEnvironmentEval->getEnv();
    _newThreadPool = _newThreadPoolEval->getThreadPool();

    //clear old error messages -- lets make new ones
    _lastBlockStatus.blockErrorMsgs.clear();
    _lastBlockStatus.propertyErrorMsgs.clear();

    //perform evaluation
    const bool evalSuccess = this->evaluationProcedure();

    //When eval fails, do a re-check on the environment.
    //Because block eval could have killed the environment.
    if (not evalSuccess) _newEnvironmentEval->update();

    //When environment fails, replace the block error messages
    //with the error message from the evaluation environment.
    if (_newEnvironmentEval->isFailureState())
    {
        _lastBlockStatus.blockErrorMsgs.clear();
        _lastBlockStatus.propertyErrorMsgs.clear();
        _lastBlockStatus.blockErrorMsgs.push_back(_newEnvironmentEval->getErrorMsg());
    }

    //we should have at least one error reported when not success
    assert(evalSuccess or not _lastBlockStatus.blockErrorMsgs.empty());

    //post the most recent status into the block in the gui thread context
    QMetaObject::invokeMethod(this, "postStatusToBlock", Qt::QueuedConnection, Q_ARG(BlockStatus, _lastBlockStatus));
}

/***********************************************************************
 * evaluation procedure implementation
 **********************************************************************/
bool BlockEval::evaluationProcedure(void)
{
    EVAL_TRACER_FUNC();
    if (_newEnvironmentEval->isFailureState())
    {
        assert(not _newEnvironmentEval->getErrorMsg().isEmpty());
        _lastBlockStatus.blockErrorMsgs.push_back(_newEnvironmentEval->getErrorMsg());
        return false;
    }
    bool evalSuccess = true;

    //the environment changed? clear everything
    //or the block changed enabled or disabled
    if (_newEnvironment != _lastEnvironment or
        _newBlockInfo.enabled != _lastBlockInfo.enabled)
    {
        _lastEnvironmentEval = _newEnvironmentEval;
        _lastEnvironment = _newEnvironment;
        _lastBlockInfo = BlockInfo();
        _blockEval = Pothos::Proxy();
        _proxyBlock = Pothos::Proxy();
    }

    //when disabled, we only evaluate the properties
    //however there is no object to apply properties.
    if (not _newBlockInfo.enabled)
    {
        evalSuccess = this->updateAllProperties();
        goto handle_property_errors;
    }

    //special case: apply settings only
    //no critical changes, block already exists
    else if (_blockEval and _proxyBlock and not this->hasCriticalChange())
    {
        //update all properties - regardless of changes
        bool setterError = not this->updateAllProperties();
        if (not setterError) for (const auto &setter : this->settersChangedList())
        {
            try
            {
                EVAL_TRACER_ACTION("call " + setter);
                _blockEval.call("handleCall", setter.toStdString());
            }
            catch (const Pothos::Exception &ex)
            {
                this->reportError(setter, ex);
                setterError = true;
                break;
            }
        }
        if (setterError) evalSuccess = false;
    }

    //otherwise, make a new block and all calls
    //update all properties - regardless of changes
    //this may create a new _blockEval if needed
    else if (this->updateAllProperties())
    {
        _proxyBlock = Pothos::Proxy(); //drop old handle

        //widget blocks have to be evaluated in the GUI thread context, otherwise, eval here
        if (_newBlockInfo.isGraphWidget)
        {
            EVAL_TRACER_ACTION("blockEvalInGUIContext");
            QMetaObject::invokeMethod(this, "blockEvalInGUIContext", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, evalSuccess));
        }
        else try
        {
            EVAL_TRACER_ACTION("eval " + _newBlockInfo.id);
            _blockEval.call("eval", _newBlockInfo.id.toStdString());
            _proxyBlock = _blockEval.call("getProxyBlock");
        }
        catch(const Pothos::Exception &ex)
        {
            this->reportError("eval", ex);
            evalSuccess = false;
        }

        //port info must be required after re-eval
        _queryPortDesc = true;
    }
    else evalSuccess = false;

    //validate the id
    if (_newBlockInfo.id.isEmpty())
    {
        _lastBlockStatus.blockErrorMsgs.push_back(tr("Error: empty ID"));
    }
    else if (_newBlockInfo.id.count(QRegExp("^[a-zA-Z]\\w*$")) != 1)
    {
        _lastBlockStatus.blockErrorMsgs.push_back(
            tr("'%1' is not a legal ID").arg(_newBlockInfo.id));
    }

    //query description overlay, even if in error
    //the overlay could be valuable even when a setup call fails
    if (std::chrono::high_resolution_clock::now() > _lastBlockStatus.overlayExpired or _queryPortDesc)
    {
        auto proxyBlock = this->getProxyBlock();
        if (proxyBlock) try
        {
            EVAL_TRACER_ACTION("get overlay");
            const std::string overlayStr = proxyBlock.call("overlay");
            const QByteArray overlayBytes(overlayStr.data(), overlayStr.size());
            if (overlayBytes != _lastBlockStatus.overlayDescStr)
            {
                QJsonParseError errorParser;
                const auto jsonDoc = QJsonDocument::fromJson(overlayBytes, &errorParser);
                if (jsonDoc.isNull())
                {
                    _logger.warning("Failed to parse JSON description overlay from %s: %s",
                        _newBlockInfo.id.toStdString(), errorParser.errorString().toStdString());
                }
                else
                {
                    _lastBlockStatus.overlayDesc = jsonDoc.object();
                    _lastBlockStatus.overlayDescStr = overlayBytes;
                }
            }
        }
        catch(const Pothos::ProxyExceptionMessage &ex)
        {
            // If overlay() throws an exception, log the exception's message. We need to specifically
            // check for the error string corresponding to the call not existing to avoid logging the
            // error caught when PothosFlow attempts to call a nonexistent function.
            static const std::string noOverlayErrorString = "call(overlay): method does not exist in registry";
            if(std::string::npos == ex.message().find(noOverlayErrorString))
            {
                poco_error_f2(
                    _logger,
                    "%s:overlay() threw the following exception: %s",
                    proxyBlock.call<std::string>("getName"),
                    ex.message());
            }
        }
        catch (...)
        {
            //the function may not exist, ignore error
        }

        //no matter what happens, mark the time so we don't over query the overlay
        _lastBlockStatus.overlayExpired = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(OVERLAY_EXPIRED_MS);
    }

    //load its port info
    if (evalSuccess and _queryPortDesc) try
    {
        EVAL_TRACER_ACTION("get port desc");
        auto proxyBlock = this->getProxyBlock();
        _lastBlockStatus.inPortDesc = portInfosToJSON(proxyBlock.call("inputPortInfo"));
        _lastBlockStatus.outPortDesc = portInfosToJSON(proxyBlock.call("outputPortInfo"));
        _queryPortDesc = false;
    }

    //parser errors report
    catch(const Pothos::Exception &ex)
    {
        this->reportError("portInfo", ex);
        evalSuccess = false;
    }

    //set the thread pool
    if (evalSuccess and _newThreadPoolEval->isFailureState())
    {
        assert(not _newThreadPoolEval->getErrorMsg().isEmpty());
        _lastBlockStatus.blockErrorMsgs.push_back(_newThreadPoolEval->getErrorMsg());
        evalSuccess = false;
    }

    //set the thread pool
    //Note: Do not set the thread pool for graphical blocks!
    //There are no configurable thread pool settings for the gui environment.
    //However, it would be useful to set the thread pool for split hierarchical widgets.
    //Currently this is not possible as there is no way to pass the remote thread pool.
    if (evalSuccess and not (_newThreadPool == _lastThreadPool))
    {
        if (not this->isGraphWidget()) try
        {
            EVAL_TRACER_ACTION("setThreadPool");
            if (_newThreadPool) this->getProxyBlock().call("setThreadPool", _newThreadPool);
            _lastThreadPool = _newThreadPool;
            _lastThreadPoolEval = _newThreadPoolEval;
        }
        catch(const Pothos::Exception &ex)
        {
            this->reportError("setThreadPool", ex);
            evalSuccess = false;
        }
    }

    //report block level error when properties errors are present
    handle_property_errors:
    if (not evalSuccess and _lastBlockStatus.blockErrorMsgs.empty())
    {
        for (const auto &pair : _lastBlockStatus.propertyErrorMsgs)
        {
            if (pair.second.isEmpty()) continue;
            _lastBlockStatus.blockErrorMsgs.push_back(tr("Error: cannot evaluate this block with property errors"));
            break;
        }
    }

    //stash the most recent state
    if (evalSuccess) _lastBlockInfo = _newBlockInfo;

    return evalSuccess;
}

/***********************************************************************
 * update graph block with the latest status
 **********************************************************************/
void BlockEval::postStatusToBlock(const BlockStatus &status)
{
    auto &block = status.block;
    if (not block) return; //block no longer exists

    //clear old error messages
    block->clearBlockErrorMsgs();
    for (const auto &propKey : block->getProperties())
    {
        block->setPropertyErrorMsg(propKey, "");
    }

    for (const auto &pair : status.propertyTypeInfos)
    {
        block->setPropertyTypeStr(pair.first, pair.second);
    }
    for (const auto &pair : status.propertyErrorMsgs)
    {
        block->setPropertyErrorMsg(pair.first, pair.second);
    }
    for (const auto &errMsg : status.blockErrorMsgs)
    {
        block->addBlockErrorMsg(errMsg);
    }
    if (status.inPortDesc.isSpecified())
    {
        block->setInputPortDesc(status.inPortDesc.value());
    }
    if (status.outPortDesc.isSpecified())
    {
        block->setOutputPortDesc(status.outPortDesc.value());
    }
    block->setGraphWidget(status.widget);
    block->setOverlayDesc(status.overlayDesc);

    block->update(); //cause redraw after changes
    emit block->evalDoneEvent(); //trigger done event subscribers
}

/***********************************************************************
 * helper routines to comprehend the block desc
 **********************************************************************/
bool BlockEval::hasCriticalChange(void) const
{
    const auto &blockDesc = _newBlockInfo.desc;

    for (const auto &arg : blockDesc["args"].toArray())
    {
        const auto propKey = arg.toString();
        if (propKey == "remoteEnv") {}
        else if (didPropKeyHaveChange(propKey)) return true;
    }
    for (const auto &callVal : blockDesc["calls"].toArray())
    {
        const auto callObj = callVal.toObject();
        if (callObj["type"].toString() != "initializer") continue;
        for (const auto &arg : callObj["args"].toArray())
        {
            const auto propKey = arg.toString();
            if (didPropKeyHaveChange(propKey)) return true;
        }
    }
    return false;
}

QStringList BlockEval::settersChangedList(void) const
{
    const auto &blockDesc = _newBlockInfo.desc;

    QStringList changedList;
    for (const auto &callVal : blockDesc["calls"].toArray())
    {
        const auto callObj = callVal.toObject();
        if (callObj["type"].toString() != "setter") continue;
        for (const auto &arg : callObj["args"].toArray())
        {
            const auto propKey = arg.toString();
            if (didPropKeyHaveChange(propKey))
            {
                changedList.push_back(callObj["name"].toString());
            }
        }
    }
    return changedList;
}

bool BlockEval::didPropKeyHaveChange(const QString &key) const
{
    if (_newBlockInfo.properties.count(key) == 0) return true;
    if (_lastBlockInfo.properties.count(key) == 0) return true;
    const auto newVal = _newBlockInfo.properties.at(key);
    const auto oldVal = _lastBlockInfo.properties.at(key);
    if (newVal != oldVal) return true;
    return (this->didExprHaveChange(newVal));
}

bool BlockEval::didExprHaveChange(const QString &expr) const
{
    for (const auto &name : this->getConstantsUsed(expr))
    {
        const bool foundInNew = _newBlockInfo.constants.count(name) != 0;
        const bool foundInLast = _lastBlockInfo.constants.count(name) != 0;

        //token is not a constant -- ignore
        if (not foundInNew and not foundInLast) continue;

        //constant removal detection -- report as changed
        if (foundInNew and not foundInLast) return true;
        if (not foundInNew and foundInLast) return true;

        //constant expression changed
        if (_newBlockInfo.constants.at(name) != _lastBlockInfo.constants.at(name)) return true;
    }

    return false;
}

bool BlockEval::isConstantUsed(const QString &name) const
{
    QStringList used;
    for (const auto &pair : _newBlockInfo.properties)
    {
        const auto &propVal = pair.second;
        used += this->getConstantsUsed(propVal);
    }
    return used.contains(name);
}

QStringList BlockEval::getConstantsUsed(const QString &expr, const size_t depth) const
{
    //probably encountered a loop, declare this a change
    if (depth > _newBlockInfo.constants.size()) return QStringList();

    //create a recursive list of used constants by traversing expressions
    QStringList used;
    #if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        #define behavior QString::SkipEmptyParts
    #else
        #define behavior Qt::SkipEmptyParts //old flags deprecated in 5.14
    #endif
    for (const auto &tok : expr.split(QRegExp("\\W"), behavior))
    {
        //is this token a constant? then inspect it
        if (_newBlockInfo.constants.count(tok) != 0)
        {
            used.push_back(tok);
            used += this->getConstantsUsed(_newBlockInfo.constants.at(tok), depth+1);
        }
        if (_lastBlockInfo.constants.count(tok) != 0)
        {
            used.push_back(tok);
            used += this->getConstantsUsed(_lastBlockInfo.constants.at(tok), depth+1);
        }
    }
    return used;
}

bool BlockEval::updateAllProperties(void)
{
    EVAL_TRACER_FUNC();
    //create a block evaluator if need-be
    if (not _blockEval) try
    {
        EVAL_TRACER_ACTION("create block evaluator");
        Pothos::Proxy evalEnv;
        if (_newBlockInfo.isGraphWidget)
        {
            EVAL_TRACER_ACTION("make EvalEnvironment");
            auto env = Pothos::ProxyEnvironment::make("managed");
            evalEnv = env->findProxy("Pothos/Util/EvalEnvironment").call("make");
        }
        else
        {
            EVAL_TRACER_ACTION("get EvalEnvironment");
            evalEnv = _newEnvironmentEval->getEval();
        }
        auto BlockEval = evalEnv.getEnvironment()->findProxy("Pothos/Util/BlockEval");
        _blockEval = BlockEval(_newBlockInfo.desc["path"].toString().toStdString(), evalEnv);
        _lastThreadPoolEval.reset();
    }
    catch (const Pothos::Exception &ex)
    {
        this->reportError("make", ex);
        return false;
    }

    //apply constants before eval property expressions
    if (not this->applyConstants()) return false;

    //update each property
    bool hasError = false;
    for (const auto &pair : _newBlockInfo.properties)
    {
        const auto &propKey = pair.first;
        const auto &propVal = pair.second;
        EVAL_TRACER_ACTION("update property " + propKey);
        try
        {
            auto obj = _blockEval.call("evalProperty", propKey.toStdString(), propVal.toStdString());
            _lastBlockStatus.propertyTypeInfos[propKey] = QString::fromStdString(obj.call<std::string>("getTypeString"));
        }
        catch (const Pothos::Exception &ex)
        {
            _lastBlockStatus.propertyErrorMsgs[propKey] = QString::fromStdString(ex.message());
            hasError = true;
        }
    }
    return not hasError;
}

bool BlockEval::applyConstants(void)
{
    EVAL_TRACER_FUNC();

    //determine which constants were removed from the last eval
    auto removedConstants = _lastBlockInfo.constantNames; //copy
    for (const auto &name : _newBlockInfo.constantNames)
    {
        removedConstants.removeAll(name);
    }

    //unregister all constants from the removed list
    for (const auto &name : removedConstants)
    {
        EVAL_TRACER_ACTION("removeConstant " + name);
        _blockEval.call("removeConstant", name.toStdString());
    }

    //apply all currently used constants in the order of dependency
    for (const auto &name : _newBlockInfo.constantNames)
    {
        if (not this->isConstantUsed(name)) continue;
        EVAL_TRACER_ACTION("applyConstant " + name);
        try
        {
            const auto &expr = _newBlockInfo.constants.at(name);
            _blockEval.call("applyConstant", name.toStdString(), expr.toStdString());
        }
        catch (const Pothos::Exception &ex)
        {
            this->reportError("applyConstants", ex);
            return false;
        }
    }
    return true;
}

void BlockEval::reportError(const QString &action, const Pothos::Exception &ex)
{
    _lastBlockStatus.blockErrorMsgs.push_back(tr("%1::%2(...) - %3")
        .arg(_newBlockInfo.id)
        .arg(action)
        .arg(QString::fromStdString(ex.message())));
}

bool BlockEval::blockEvalInGUIContext(void)
{
    try
    {
        _blockEval.call("setProperty", "remoteEnv", _newEnvironmentEval->getEval().getEnvironment());
        _blockEval.call("eval", _newBlockInfo.id.toStdString());
        _proxyBlock = _blockEval.call("getProxyBlock");
        _lastBlockStatus.widget = _proxyBlock.call<QWidget *>("widget");
        return true;
    }
    catch (const Pothos::Exception &ex)
    {
        _logger.error("Failed to eval in GUI context %s-%s", _newBlockInfo.id.toStdString(), ex.displayText());
        _lastBlockStatus.blockErrorMsgs.push_back(tr("Failed to eval in GUI context %1-%2").arg(_newBlockInfo.id).arg(QString::fromStdString(ex.message())));
        return false;
    }
}
