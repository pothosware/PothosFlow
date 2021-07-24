// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include "AffinitySupport/AffinityZonesMenu.hpp"
#include "AffinitySupport/AffinityZonesComboBox.hpp"
#include "AffinitySupport/AffinityZoneEditor.hpp"
#include "ColorUtils/ColorUtils.hpp"
#include "MainWindow/MainSettings.hpp"
#include <QToolTip>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTabWidget>
#include <QJsonDocument>
#include <Poco/Logger.h>
#include <cassert>

static AffinityZonesDock *globalAffinityZonesDock = nullptr;

AffinityZonesDock *AffinityZonesDock::global(void)
{
    return globalAffinityZonesDock;
}

AffinityZonesDock::AffinityZonesDock(QWidget *parent, HostExplorerDock *hostExplorer):
    QDockWidget(parent),
    _hostExplorerDock(hostExplorer),
    _zoneEntry(new QLineEdit(this)),
    _createButton(new QPushButton(makeIconFromTheme("list-add"), tr("Create zone"), this)),
    _editorsTabs(new QTabWidget(this))
{
    globalAffinityZonesDock = this;
    this->setObjectName("AffinityZonesDock");
    this->setWindowTitle(tr("Affinity Zones"));
    this->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    this->setWidget(new QWidget(this));

    //layout setup
    auto mainLayout = new QVBoxLayout(this->widget());
    this->widget()->setLayout(mainLayout);

    //editors area
    {
        mainLayout->addWidget(_editorsTabs);
        _editorsTabs->setTabsClosable(true);
        _editorsTabs->setMovable(true);
        _editorsTabs->setUsesScrollButtons(true);
        _editorsTabs->setTabPosition(QTabWidget::North);
        _editorsTabs->setStyleSheet(
            QString("QTabBar::close-button {image: url(%1);}").arg(makeIconPath("standardbutton-closetab-16.png"))+
            QString("QTabBar::close-button:hover {image: url(%1);}").arg(makeIconPath("standardbutton-closetab-hover-16.png"))+
            QString("QTabBar::close-button:pressed {image: url(%1);}").arg(makeIconPath("standardbutton-closetab-down-16.png")));
    }

    //zone creation area
    {
        auto hbox = new QHBoxLayout();
        mainLayout->addLayout(hbox);
        hbox->addWidget(_zoneEntry);
        hbox->addWidget(_createButton);
        _zoneEntry->setPlaceholderText(tr("Enter a new zone name..."));
        _createButton->setToolTip(tr("Create a new affinity zone editor panel."));
        connect(_zoneEntry, &QLineEdit::returnPressed, this, &AffinityZonesDock::handleCreateZone);
        connect(_createButton, &QPushButton::pressed, this, &AffinityZonesDock::handleCreateZone);
    }

    this->initAffinityZoneEditors();
}

QMenu *AffinityZonesDock::makeMenu(QWidget *parent)
{
    return new AffinityZonesMenu(this, parent);
}

QComboBox *AffinityZonesDock::makeComboBox(QWidget *parent)
{
    return new AffinityZonesComboBox(this, parent);
}

QStringList AffinityZonesDock::zones(void) const
{
    QStringList zones;
    for (int i = 0; i < _editorsTabs->count(); i++)
    {
        auto editor = qobject_cast<AffinityZoneEditor *>(_editorsTabs->widget(i));
        assert(editor != nullptr);
        zones.push_back(editor->zoneName());
    }
    return zones;
}

QColor AffinityZonesDock::zoneToColor(const QString &zone)
{
    for (int i = 0; i < _editorsTabs->count(); i++)
    {
        auto editor = qobject_cast<AffinityZoneEditor *>(_editorsTabs->widget(i));
        assert(editor != nullptr);
        if (zone == editor->zoneName()) return editor->color();
    }
    return QColor();
}

QJsonObject AffinityZonesDock::zoneToConfig(const QString &zone)
{
    for (int i = 0; i < _editorsTabs->count(); i++)
    {
        auto editor = qobject_cast<AffinityZoneEditor *>(_editorsTabs->widget(i));
        assert(editor != nullptr);
        if (zone == editor->zoneName()) return editor->getCurrentConfig();
    }
    return QJsonObject();
}

void AffinityZonesDock::handleTabCloseRequested(const int index)
{
    _editorsTabs->removeTab(index);
    this->ensureDefault();
    this->saveAffinityZoneEditorsState();
}

void AffinityZonesDock::handleCreateZone(void)
{
    auto zoneName = _zoneEntry->text();
    _zoneEntry->setText("");
    if (zoneName.isEmpty()) return;
    for (const auto &name : this->zones())
    {
        if (name == zoneName)
        {
            this->handleErrorMessage(tr("%1 already exists!").arg(zoneName));
            return;
        }
    }
    auto editor = this->createZoneFromName(zoneName);
    _editorsTabs->setCurrentWidget(editor);
    this->saveAffinityZoneEditorsState();
}

AffinityZoneEditor *AffinityZonesDock::createZoneFromName(const QString &zoneName)
{
    auto settings = MainSettings::global();
    auto editor = new AffinityZoneEditor(zoneName, this, _hostExplorerDock);
    _editorsTabs->addTab(editor, zoneName);
    if (zoneName == settings->value("AffinityZones/currentZone").toString()) _editorsTabs->setCurrentWidget(editor);

    //restore the settings from save -- even if this is a new panel with the same name as a previous one
    const auto value = settings->value("AffinityZones/zones/"+zoneName);
    if (value.isValid())
    {
        QJsonParseError parseError;
        const auto jsonDoc = QJsonDocument::fromJson(value.toByteArray(), &parseError);
        if (not jsonDoc.isNull())
        {
            editor->loadFromConfig(jsonDoc.object());
        }
        else
        {
            static auto &logger = Poco::Logger::get("PothosFlow.AffinityZonesDock");
            logger.error("Failed to load editor for zone '%s' -- %s",
                zoneName.toStdString(), parseError.errorString().toStdString());
        }
    }

    //now connect the changed signal after initialization+restore changes
    connect(editor, &AffinityZoneEditor::settingsChanged, [=](void){this->saveAffinityZoneEditorsState();});
    connect(editor, &AffinityZoneEditor::settingsChanged, [=](void){emit this->zoneChanged(zoneName);});

    //when to update colors
    connect(editor, &AffinityZoneEditor::settingsChanged, this, &AffinityZonesDock::updateTabColors);
    this->updateTabColors();

    return editor;
}

void AffinityZonesDock::ensureDefault(void)
{
    if (_editorsTabs->count() == 0) this->createZoneFromName("default");
}

void AffinityZonesDock::initAffinityZoneEditors(void)
{
    auto settings = MainSettings::global();
    auto names = settings->value("AffinityZones/zoneNames").toStringList();
    for (const auto &name : names) this->createZoneFromName(name);
    this->ensureDefault();
    connect(_editorsTabs, &QTabWidget::tabCloseRequested, this, &AffinityZonesDock::handleTabCloseRequested);
    connect(_editorsTabs, &QTabWidget::currentChanged, this, &AffinityZonesDock::handleTabSelectionChanged);
}

void AffinityZonesDock::updateTabColors(void)
{
    for (int i = 0; i < _editorsTabs->count(); i++)
    {
        auto editor = qobject_cast<AffinityZoneEditor *>(_editorsTabs->widget(i));
        _editorsTabs->setTabIcon(i, colorToWidgetIcon(editor->color()));
    }
}

void AffinityZonesDock::handleTabSelectionChanged(const int index)
{
    auto settings = MainSettings::global();
    settings->setValue("AffinityZones/currentZone", _editorsTabs->tabText(index));
}

void AffinityZonesDock::saveAffinityZoneEditorsState(void)
{
    auto settings = MainSettings::global();
    settings->setValue("AffinityZones/zoneNames", this->zones());

    for (int i = 0; i < _editorsTabs->count(); i++)
    {
        auto editor = qobject_cast<AffinityZoneEditor *>(_editorsTabs->widget(i));
        assert(editor != nullptr);
        const auto jsonDoc = QJsonDocument(editor->getCurrentConfig());
        const auto value = jsonDoc.toJson(QJsonDocument::Compact);
        settings->setValue("AffinityZones/zones/"+editor->zoneName(), value);
    }

    emit this->zonesChanged();
}

void AffinityZonesDock::handleErrorMessage(const QString &errMsg)
{
    QToolTip::showText(_zoneEntry->mapToGlobal(QPoint()), "<font color=\"red\">"+errMsg+"</font>");
}
