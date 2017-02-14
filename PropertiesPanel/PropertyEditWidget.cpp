// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "PropertyEditWidget.hpp"
#include "ColorUtils/ColorUtils.hpp"
#include <QLabel>
#include <QLocale>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <Pothos/Plugin.hpp>
#include <Poco/Logger.h>

/*!
 * We could remove the timer with the eval-background system.
 * But rather, it may still be useful to have an idle period
 * in which we accept new edit events before submitting changes.
 * So just leave this as a small number for the time-being.
 */
static const long UPDATE_TIMER_MS = 500;

PropertyEditWidget::PropertyEditWidget(const QString &initialValue, const Poco::JSON::Object::Ptr &paramDesc, QWidget *parent):
    _initialValue(initialValue),
    _editWidget(nullptr),
    _errorLabel(new QLabel(this)),
    _formLabel(nullptr),
    _entryTimer(new QTimer(this)),
    _editLayout(new QVBoxLayout(this)),
    _modeButton(new QToolButton(this)),
    _modeLayout(new QHBoxLayout()),
    _editParent(parent),
    _forceLineWidget(false)
{
    //setup entry timer - timeout acts like widget changed
    _entryTimer->setSingleShot(true);
    _entryTimer->setInterval(UPDATE_TIMER_MS);
    connect(_entryTimer, SIGNAL(timeout(void)), this, SIGNAL(widgetChanged(void)));

    //setup edit mode button
    _modeButton->setFixedSize(QSize(20, 20));
    //focus color is distracting, dont enable focus
    _modeButton->setFocusPolicy(Qt::NoFocus);
    connect(_modeButton, SIGNAL(clicked(void)), this, SLOT(handleModeButtonClicked(void)));

    //layout internal widgets
    _editLayout->setSpacing(0);
    _editLayout->setContentsMargins(QMargins());
    _editLayout->addLayout(_modeLayout);
    _editLayout->addWidget(_errorLabel);
    _modeLayout->setSpacing(3);
    _modeLayout->setContentsMargins(QMargins());
    _modeLayout->addWidget(_modeButton, 0, Qt::AlignRight);

    //initialize edit widget
    this->reloadParamDesc(paramDesc);
}

PropertyEditWidget::~PropertyEditWidget(void)
{
    //we dont own form label, so it has to be explicitly deleted
    delete _formLabel;
}

void PropertyEditWidget::reloadParamDesc(const Poco::JSON::Object::Ptr &paramDesc)
{
    _lastParamDesc = paramDesc;

    //value to set on replaced widget
    QString newValue = this->initialValue();
    if (_editWidget != nullptr) newValue = this->value();

    //delete a previous widget
    delete _editWidget;

    //extract widget type
    auto widgetType = paramDesc->optValue<std::string>("widgetType", "LineEdit");
    if (paramDesc->isArray("options")) widgetType = "ComboBox";
    if (widgetType.empty()) widgetType = "LineEdit";
    _unitsStr = QString::fromStdString(paramDesc->optValue<std::string>("units", ""));

    //check if the widget type exists in the plugin tree
    if (not Pothos::PluginRegistry::exists(Pothos::PluginPath("/gui/EntryWidgets").join(widgetType)))
    {
        poco_error_f1(Poco::Logger::get("PothosGui.BlockPropertiesPanel"), "widget type %s does not exist", widgetType);
        widgetType = "LineEdit";
    }

    //use line the line edit when forced by the button
    _modeButton->setVisible(widgetType != "LineEdit");
    if (_forceLineWidget) widgetType = "LineEdit";

    //lookup the plugin to get the entry widget factory
    const auto plugin = Pothos::PluginRegistry::get(Pothos::PluginPath("/gui/EntryWidgets").join(widgetType));
    const auto &factory = plugin.getObject().extract<Pothos::Callable>();
    _editWidget = factory.call<QWidget *>(paramDesc, static_cast<QWidget *>(_editParent));
    _editWidget->setLocale(QLocale::C);
    _editWidget->setObjectName("BlockPropertiesEditWidget"); //style-sheet id name
    _modeLayout->insertWidget(0, _editWidget, 1);

    //initialize value
    this->setValue(newValue);

    //signals to internal handler
    connect(_editWidget, SIGNAL(widgetChanged(void)), this, SLOT(handleWidgetChanged(void)));
    connect(_editWidget, SIGNAL(entryChanged(void)), this, SLOT(handleEntryChanged(void)));
    connect(_editWidget, SIGNAL(commitRequested(void)), this, SLOT(handleCommitRequested(void)));

    //update display
    this->updateInternals();
}

const QString &PropertyEditWidget::initialValue(void) const
{
    return _initialValue;
}

bool PropertyEditWidget::changed(void) const
{
    return this->value() != this->initialValue();
}

QString PropertyEditWidget::value(void) const
{
    QString value;
    QMetaObject::invokeMethod(_editWidget, "value", Qt::DirectConnection, Q_RETURN_ARG(QString, value));
    return value;
}

void PropertyEditWidget::setValue(const QString &value)
{
    QMetaObject::invokeMethod(_editWidget, "setValue", Qt::DirectConnection, Q_ARG(QString, value));
}

void PropertyEditWidget::setTypeStr(const std::string &typeStr)
{
    this->setBackgroundColor(typeStrToColor(typeStr));
}

void PropertyEditWidget::setErrorMsg(const QString &errorMsg)
{
    _errorMsg = errorMsg;
    this->updateInternals();
}

void PropertyEditWidget::setBackgroundColor(const QColor &color)
{
    _bgColor = color;
    this->updateInternals();
}

QLabel *PropertyEditWidget::makeFormLabel(const QString &text, QWidget *parent)
{
    if (not _formLabel)
    {
        _formLabelText = text;
        _formLabel = new QLabel(text, parent);
        this->updateInternals();
    }
    return _formLabel;
}

void PropertyEditWidget::updateInternals(void)
{
    //determine state
    const bool hasError = not _errorMsg.isEmpty();
    const bool hasUnits = not _unitsStr.isEmpty();

    //update the error label
    _errorLabel->setVisible(hasError);
    _errorLabel->setText(QString("<span style='color:red;'><p><i>%1</i></p></span>").arg(_errorMsg.toHtmlEscaped()));
    _errorLabel->setWordWrap(true);

    //generate the form label
    auto formLabelText = QString("<span style='color:%1;'><b>%2%3</b></span>")
        .arg(hasError?"red":"black")
        .arg(_formLabelText)
        .arg(this->changed()?"*":"");
    if (hasUnits) formLabelText += QString("<br /><i>%1</i>").arg(_unitsStr);
    if (_formLabel) _formLabel->setText(formLabelText);

    //swap mode button arrow based on state
    _modeButton->setArrowType(_forceLineWidget?Qt::LeftArrow:Qt::RightArrow);

    //set background color when its valid
    if (_bgColor.isValid()) _editWidget->setStyleSheet(
        QString("#BlockPropertiesEditWidget{background:%1;color:%2;}")
        .arg(_bgColor.name()).arg((_bgColor.lightnessF() > 0.5)?"black":"white"));
}

void PropertyEditWidget::handleWidgetChanged(void)
{
    this->updateInternals();
    emit this->widgetChanged();
}

void PropertyEditWidget::handleEntryChanged(void)
{
    _entryTimer->start(UPDATE_TIMER_MS);
    this->updateInternals();
    emit this->entryChanged();
}

void PropertyEditWidget::handleCommitRequested(void)
{
    this->flushEvents();
    this->updateInternals();
    emit this->commitRequested();
}

void PropertyEditWidget::handleModeButtonClicked(void)
{
    _forceLineWidget = not _forceLineWidget;
    this->reloadParamDesc(_lastParamDesc);
}

void PropertyEditWidget::cancelEvents(void)
{
    _entryTimer->stop();
}

void PropertyEditWidget::flushEvents(void)
{
    if (not _entryTimer->isActive()) return;
    _entryTimer->stop();
    this->handleEntryChanged();
}
