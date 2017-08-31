// Copyright (c) 2017-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QPushButton>

/***********************************************************************
 * Toggle button is a checkable push button + labels
 **********************************************************************/
class ToggleButton : public QPushButton
{
    Q_OBJECT
public:
    ToggleButton(QWidget *parent, const QString &onText, const QString &offText):
        QPushButton(parent),
        _onText(onText),
        _offText(offText)
    {
        this->setCheckable(true);
        connect(this, SIGNAL(toggled(bool)), this, SLOT(handleToggled(bool)));
    }

public slots:
    QString value(void) const
    {
        return this->isChecked()?"true":"false";
    }

    void setValue(const QString &s)
    {
        this->setChecked(s=="true");
        this->updateText(s=="true");
    }

signals:
    void commitRequested(void);
    void widgetChanged(void);
    void entryChanged(void);

private slots:
    void handleToggled(const bool checked)
    {
        this->updateText(checked);
        emit this->entryChanged();
    }

private:
    void updateText(const bool checked)
    {
        this->setText(checked?_onText:_offText);
    }

    const QString _onText;
    const QString _offText;
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeToggleButton(const QJsonArray &, const QJsonObject &kwargs, QWidget *parent)
{
    return new ToggleButton(parent, kwargs["on"].toString(), kwargs["off"].toString());
}

pothos_static_block(registerToggleButton)
{
    Pothos::PluginRegistry::add("/flow/EntryWidgets/ToggleButton", Pothos::Callable(&makeToggleButton));
}

#include "ToggleButton.moc"
