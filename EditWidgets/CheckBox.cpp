// Copyright (c) 2017-2017 Josh Blum
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
    CheckBox(QWidget *parent, const QString &checkedText, const QString &uncheckedText):
        QCheckBox(parent),
        _checkedText(checkedText),
        _uncheckedText(uncheckedText)
    {
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
        this->setText(checked?_checkedText:_uncheckedText);
    }

    const QString _checkedText;
    const QString _uncheckedText;
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
    Pothos::PluginRegistry::add("/gui/EntryWidgets/CheckBox", Pothos::Callable(&makeCheckBox));
}

#include "CheckBox.moc"
