// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/FormLayout.hpp"
#include "PropertyEditWidget.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include "BlockPropertiesPanel.hpp"
#include "GraphObjects/GraphObject.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include <Poco/Logger.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QTabWidget>
#include <QLabel>
#include <QPainter>
#include <QJsonDocument>

/*!
 * We could remove the timer with the eval-background system.
 * But rather, it may still be useful to have an idle period
 * in which we accept new edit events before submitting changes.
 * So just leave this as a small number for the time-being.
 */
static const long UPDATE_TIMER_MS = 500;

BlockPropertiesPanel::BlockPropertiesPanel(GraphBlock *block, QWidget *parent):
    QWidget(parent),
    _ignoreChanges(true),
    _idLineEdit(nullptr),
    _affinityZoneLabel(new QLabel(this)),
    _affinityZoneBox(nullptr),
    _blockErrorLabel(new QLabel(this)),
    _infoTabs(new QTabWidget(this)),
    _blockInfoDesc(nullptr),
    _jsonBlockDesc(nullptr),
    _evalTypesDesc(nullptr),
    _formLayout(nullptr),
    _propertiesTabs(nullptr),
    _block(block)
{
    auto blockDesc = block->getBlockDesc();

    //master layout for this widget
    _formLayout = makeFormLayout(this);

    //title
    {
        auto label = new QLabel(QString("<h1>%1</h1>").arg(_block->getTitle().toHtmlEscaped()), this);
        label->setAlignment(Qt::AlignCenter);
        _formLayout->addRow(label);
    }

    //id
    {
        _idLineEdit = new PropertyEditWidget(_block->getId(), QJsonObject(), "", this);
        _formLayout->addRow(_idLineEdit->makeFormLabel(tr("ID"), this), _idLineEdit);
        connect(_idLineEdit, &PropertyEditWidget::widgetChanged, this, &BlockPropertiesPanel::handleWidgetChanged);
        connect(_idLineEdit, &PropertyEditWidget::widgetChanged, _block, &GraphBlock::triggerEvalEvent);
        connect(_idLineEdit, &PropertyEditWidget::entryChanged, this, &BlockPropertiesPanel::handleWidgetChanged);
        connect(_idLineEdit, &PropertyEditWidget::commitRequested, this, &BlockPropertiesPanel::handleCommit);
    }

    //create optional properties tabs
    _propertiesTabs = new QTabWidget(this);
    _formLayout->addRow(_propertiesTabs);
    for (const auto &propKey : _block->getProperties())
    {
        const auto tabName = _block->getParamDesc(propKey)["tab"].toString();
        if (_paramLayouts.count(tabName) != 0) continue;
        auto tab = new QWidget(_propertiesTabs);
        _paramLayouts[tabName] = makeFormLayout(tab);
        _propertiesTabs->addTab(tab, tabName.isEmpty()? tr("Default") : tabName);
        _tabWidgetToTabName[tab] = tabName;
    }

    //select the current active tab
    for (const auto &pair : _tabWidgetToTabName)
    {
        if (pair.second == _block->getActiveEditTab())
        {
            _propertiesTabs->setCurrentWidget(pair.first);
        }
    }

    //only one default tab? fall back to no tab widget
    if (_paramLayouts.size() == 1 and _paramLayouts.count("") == 1)
    {
        _paramLayouts[""] = _formLayout;
        delete _propertiesTabs;
        _propertiesTabs = nullptr;
    }

    //properties
    for (const auto &propKey : _block->getProperties())
    {
        auto paramDesc = _block->getParamDesc(propKey);
        const auto editMode = _block->getPropertyEditMode(propKey);

        //create editable widget
        auto editWidget = new PropertyEditWidget(_block->getPropertyValue(propKey), paramDesc, editMode, this);
        connect(editWidget, SIGNAL(widgetChanged(void)), this, SLOT(handleWidgetChanged(void)));
        connect(editWidget, SIGNAL(widgetChanged(void)), _block, SIGNAL(triggerEvalEvent(void)));
        connect(editWidget, SIGNAL(entryChanged(void)), this, SLOT(handleWidgetChanged(void)));
        connect(editWidget, SIGNAL(commitRequested(void)), this, SLOT(handleCommit(void)));
        _propIdToEditWidget[propKey] = editWidget;
        editWidget->setToolTip(this->getParamDocString(propKey));

        //install into appropriate form/tab
        auto layout = _paramLayouts.at(paramDesc["tab"].toString());
        layout->addRow(editWidget->makeFormLabel(_block->getPropertyName(propKey), this), editWidget);
    }

    //affinity zone
    {
        _affinityZoneOriginal = _block->getAffinityZone();
        _affinityZoneBox = AffinityZonesDock::global()->makeComboBox(this);
        _formLayout->addRow(_affinityZoneLabel, _affinityZoneBox);
        connect(_affinityZoneBox, SIGNAL(activated(const QString &)), this, SLOT(handleAffinityZoneChanged(const QString &)));
    }

    //errors
    {
        _formLayout->addRow(_blockErrorLabel);
    }

    //draw the block's preview onto a mini pixmap
    //this is cool, maybe useful, but its big, where can we put it?
    /*
    {
        const auto bounds = _block->getBoundingRect();
        QPixmap pixmap(bounds.size().toSize()+QSize(2,2));
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.translate(-bounds.topLeft()+QPoint(1,1));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        _block->render(painter);
        painter.end();
        auto label = new QLabel(this);
        label->setPixmap(pixmap);
        _formLayout->addRow(label);
        _formLayout->setAlignment(label, Qt::AlignHCenter);
    }
    */

    //block level description
    connect(_infoTabs, &QTabWidget::currentChanged, this, &BlockPropertiesPanel::handleDocTabChanged);
    _formLayout->addRow(_infoTabs);
    {
        QString output;
        output += QString("<h1>%1</h1>").arg(block->getTitle());
        output += QString("<p>%1</p>").arg(block->getBlockDescPath());
        output += "<p>";
        for (const auto &lineVal : blockDesc["docs"].toArray())
        {
            const auto line = lineVal.toString();
            if (line.isEmpty()) output += "<p /><p>";
            else output += line+"\n";
        }
        output += "</p>";

        //enumerate properties
        if (not _block->getProperties().empty())
        {
            output += QString("<h2>%1</h2>").arg(tr("Properties"));
            for (const auto &propKey : _block->getProperties())
            {
                output += this->getParamDocString(propKey);
            }
        }

        //enumerate slots
        if (not block->getSlotPorts().empty())
        {
            output += QString("<h2>%1</h2>").arg(tr("Slots"));
            output += "<ul>";
            for (const auto &portKey : block->getSlotPorts())
            {
                output += QString("<li>%1(...)</li>").arg(portKey);
            }
            output += "</ul>";
        }

        //enumerate signals
        if (not block->getSignalPorts().empty())
        {
            output += QString("<h2>%1</h2>").arg(tr("Signals"));
            output += "<ul>";
            for (const auto &portKey : block->getSignalPorts())
            {
                output += QString("<li>%1(...)</li>").arg(portKey);
            }
            output += "</ul>";
        }

        _blockInfoDesc = new QLabel(output, _infoTabs);
        _blockInfoDesc->setStyleSheet("QLabel{background:white;margin:1px;}");
        _blockInfoDesc->setWordWrap(true);
        _blockInfoDesc->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        _blockInfoDesc->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        _blockInfoDesc->setOpenExternalLinks(true);
        connect(_blockInfoDesc, &QLabel::linkHovered, _blockInfoDesc, &QLabel::setToolTip);
        _infoTabs->addTab(_blockInfoDesc, tr("Documentation"));
    }

    //block desc JSON (filled in by event handler)
    {
        _jsonBlockDesc = new QLabel(_infoTabs);
        _infoTabs->addTab(_jsonBlockDesc, tr("JSON description"));
        _jsonBlockDesc->setStyleSheet("QLabel{background:white;margin:1px;}");
        _jsonBlockDesc->setWordWrap(true);
        _jsonBlockDesc->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        _jsonBlockDesc->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    //evaluated types (filled in by event handler)
    {
        _evalTypesDesc = new QLabel(_infoTabs);
        _infoTabs->addTab(_evalTypesDesc, tr("Evaluated types"));
        _evalTypesDesc->setStyleSheet("QLabel{background:white;margin:1px;}");
        _evalTypesDesc->setWordWrap(true);
        _evalTypesDesc->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        _evalTypesDesc->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    connect(_block, &GraphBlock::destroyed, this, &BlockPropertiesPanel::handleBlockDestroyed);
    connect(_block, &GraphBlock::evalDoneEvent, this, &BlockPropertiesPanel::handleBlockEvalDone);
    connect(_block, SIGNAL(paramDescChanged(const QString &, const QJsonObject &)),
            this, SLOT(handleParamDescChanged(const QString &, const QJsonObject &)));
    this->updateAllForms();
    _ignoreChanges = false;
}

QString BlockPropertiesPanel::getParamDocString(const QString &propKey)
{
    const auto paramDesc = _block->getParamDesc(propKey);
    QString unitsStr;
    if (paramDesc.contains("units")) unitsStr = QString(" (%1)")
        .arg(paramDesc["units"].toString().toHtmlEscaped());
    QString output;
    output += QString("<h3>%1%2</h3>").arg(_block->getPropertyName(propKey).toHtmlEscaped()).arg(unitsStr);
    if (paramDesc.contains("desc")) for (const auto &lineVal : paramDesc["desc"].toArray())
    {
        const auto line = lineVal.toString();
        if (line.isEmpty()) output += "<p /><p>";
        else output += line+"\n";
    }
    else output += QString("<p>%1</p>").arg(tr("Undocumented"));
    return output;
}

void BlockPropertiesPanel::handleBlockDestroyed(QObject *)
{
    this->deleteLater();
}

void BlockPropertiesPanel::handleWidgetChanged(void)
{
    if (_ignoreChanges) return;

    //dump editor id to block
    _block->setId(_idLineEdit->value());

    //dump the affinity zone to block
    _block->setAffinityZone(_affinityZoneBox->itemData(_affinityZoneBox->currentIndex()).toString());

    //dump all values from edit widgets into the block's property values
    for (const auto &propKey : _block->getProperties())
    {
        QString newValue = _propIdToEditWidget[propKey]->value();
        newValue.replace("\n", ""); //cannot handle multi-line values
        _block->setPropertyValue(propKey, newValue);
    }

    this->updateAllForms(); //quick update for labels
}

void BlockPropertiesPanel::handleAffinityZoneChanged(const QString &)
{
    this->handleWidgetChanged();
    emit _block->triggerEvalEvent();
}

void BlockPropertiesPanel::handleBlockEvalDone(void)
{
    this->updateAllForms();
}

void BlockPropertiesPanel::handleParamDescChanged(const QString &key, const QJsonObject &desc)
{
    _propIdToEditWidget[key]->reloadParamDesc(desc);
}

void BlockPropertiesPanel::handleCancel(void)
{
    //reset values in block to original setting
    _block->setId(_idLineEdit->initialValue());
    _idLineEdit->cancelEvents();
    _block->setAffinityZone(_affinityZoneOriginal);
    for (const auto &propKey : _block->getProperties())
    {
        _block->setPropertyValue(propKey, _propIdToEditWidget[propKey]->initialValue());
        _propIdToEditWidget[propKey]->cancelEvents();
    }
    emit _block->triggerEvalEvent(); //update after change reversion

    //an edit widget return press signal may have us here,
    //and not the commit button, so make sure panel is deleted
    this->deleteLater();
}

void BlockPropertiesPanel::handleCommit(void)
{
    //were there changes?
    std::vector<QString> propertiesModified;
    for (const auto &propKey : _block->getProperties())
    {
        if (_propIdToEditWidget[propKey]->changed())
        {
            propertiesModified.push_back(_block->getPropertyName(propKey));
        }
        _block->setPropertyEditMode(propKey, _propIdToEditWidget[propKey]->editMode());
    }

    //was the ID changed?
    if (_idLineEdit->changed()) propertiesModified.push_back(tr("ID"));

    //was the affinity zone changed?
    if (_affinityZoneOriginal != _block->getAffinityZone()) propertiesModified.push_back(tr("Affinity Zone"));

    if (propertiesModified.empty()) return this->handleCancel();

    //stash the latest active tab to restore for next open
    if (_propertiesTabs != nullptr)
    {
        auto it = _tabWidgetToTabName.find(_propertiesTabs->currentWidget());
        if (it != _tabWidgetToTabName.end()) _block->setActiveEditTab(it->second);
    }

    //emit a new graph state event
    auto desc = (propertiesModified.size() == 1)? propertiesModified.front() : tr("properties");
    emit this->stateChanged(GraphState("document-properties", tr("Edit %1 %2").arg(_block->getId()).arg(desc)));

    //an edit widget return press signal may have us here,
    //and not the commit button, so make sure panel is deleted
    this->deleteLater();
}

void BlockPropertiesPanel::handleDocTabChanged(int index)
{
    if (_jsonBlockDesc == nullptr) return;
    if (_evalTypesDesc == nullptr) return;
    _jsonBlockDesc->setText("");
    _evalTypesDesc->setText("");

    if (_infoTabs->widget(index) == _jsonBlockDesc)
    {
        const QJsonDocument jsonDoc(_block->getBlockDesc());
        _jsonBlockDesc->setText(jsonDoc.toJson(QJsonDocument::Indented));
    }

    if (_infoTabs->widget(index) == _evalTypesDesc)
    {
        QString output;
        if (not _block->getProperties().empty())
        {
            output += QString("<h2>%1</h2>").arg(tr("Properties"));
            output += "<ul>";
            for (const auto &propKey : _block->getProperties())
            {
                const auto typeStr = _block->getPropertyTypeStr(propKey);
                if (not typeStr.isEmpty()) output += QString("<li><b>%1</b> - %2</li>")
                    .arg(propKey).arg(typeStr.toHtmlEscaped());
            }
            output += "</ul>";
        }
        if (not _block->getInputPorts().empty())
        {
            output += QString("<h2>%1</h2>").arg(tr("Inputs"));
            output += "<ul>";
            for (const auto &portKey : _block->getInputPorts())
            {
                const auto typeStr = _block->getInputPortTypeStr(portKey);
                if (not typeStr.isEmpty()) output += QString("<li><b>%1</b> - %2</li>")
                    .arg(_block->getInputPortAlias(portKey).toHtmlEscaped())
                    .arg(typeStr.toHtmlEscaped());
            }
            output += "</ul>";
        }
        if (not _block->getOutputPorts().empty())
        {
            output += QString("<h2>%1</h2>").arg(tr("Outputs"));
            output += "<ul>";
            for (const auto &portKey : _block->getOutputPorts())
            {
                const auto typeStr = _block->getOutputPortTypeStr(portKey);
                if (not typeStr.isEmpty()) output += QString("<li><b>%1</b> - %2</li>")
                    .arg(_block->getOutputPortAlias(portKey).toHtmlEscaped())
                    .arg(typeStr.toHtmlEscaped());
            }
            output += "</ul>";
        }
        _evalTypesDesc->setText(output);
    }
}

void BlockPropertiesPanel::updateAllForms(void)
{
    //block id
    if (_idLineEdit->value() != _block->getId())
    {
        _idLineEdit->setValue(_block->getId());
    }

    //affinity zone
    {
        for (int i = 0; i < _affinityZoneBox->count(); i++)
        {
            if (_affinityZoneBox->itemData(i).toString() == _block->getAffinityZone())
            {
                _affinityZoneBox->setCurrentIndex(i);
            }
        }
        _affinityZoneLabel->setText(QString("<b>%1%2</b>")
            .arg(tr("Affinity Zone"))
            .arg((_affinityZoneOriginal != _block->getAffinityZone())?"*":""));
    }

    //update block errors
    {
        const auto &errors = _block->getBlockErrorMsgs();
        _blockErrorLabel->setVisible(not errors.isEmpty());
        _blockErrorLabel->setWordWrap(true);
        QString errorList;
        for (const auto &errMsg : errors)
        {
            errorList += QString("<li><i>%1</i></li>").arg(errMsg.toHtmlEscaped());
        }
        if (errors.size() > 1) errorList = QString("<ul>%1</ul>").arg(errorList);
        _blockErrorLabel->setText(QString(
            "<p><span style='color:red;'>%1</span></p>")
            .arg(errorList));
    }

    //update properties
    for (const auto &key : _block->getProperties()) this->updatePropForms(key);

    //update info tabs
    this->handleDocTabChanged(_infoTabs->currentIndex());
}

void BlockPropertiesPanel::updatePropForms(const QString &propKey)
{
    auto editWidget = _propIdToEditWidget[propKey];

    //update the edit widget state
    editWidget->setTypeStr(_block->getPropertyTypeStr(propKey));
    editWidget->setErrorMsg(_block->getPropertyErrorMsg(propKey));

    //set the editor's value if changed
    if (_block->getPropertyValue(propKey) != editWidget->value())
    {
        editWidget->setValue(_block->getPropertyValue(propKey));
    }
}
