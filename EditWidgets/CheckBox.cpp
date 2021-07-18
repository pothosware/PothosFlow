// Copyright (c) 2017-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QCheckBox>

/***********************************************************************
 * Check box with labels
 **********************************************************************/
class CheckBox : public QCheckBox
{
    Q_OBJECT
public:
    CheckBox(QWidget *parent, const QString &onText, const QString &offText):
        QCheckBox(parent),
        _onText(onText),
        _offText(offText)
    {
        connect(this, &QCheckBox::toggled, this, &CheckBox::handleToggled);
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
static QWidget *makeCheckBox(const QJsonArray &, const QJsonObject &kwargs, QWidget *parent)
{
    return new CheckBox(parent, kwargs["on"].toString(), kwargs["off"].toString());
}

pothos_static_block(registerCheckBox)
{
    Pothos::PluginRegistry::add("/flow/EntryWidgets/CheckBox", Pothos::Callable(&makeCheckBox));
}

#include "CheckBox.moc"
