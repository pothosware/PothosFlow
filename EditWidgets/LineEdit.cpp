// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>

/***********************************************************************
 * General purpose single line entry widget
 **********************************************************************/
class LineEdit : public QLineEdit
{
    Q_OBJECT
public:
    LineEdit(QWidget *parent):
        QLineEdit(parent)
    {
        connect(this, &QLineEdit::textEdited, [=](const QString &){emit this->entryChanged();});
        connect(this, &QLineEdit::returnPressed, this, &LineEdit::commitRequested);
    }

public slots:
    QString value(void) const
    {
        return QLineEdit::text();
    }

    void setValue(const QString &s)
    {
        QLineEdit::setText(s);
    }

signals:
    void commitRequested(void);
    void widgetChanged(void);
    void entryChanged(void);
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeLineEdit(const QJsonArray &, const QJsonObject &, QWidget *parent)
{
    return new LineEdit(parent);
}

pothos_static_block(registerLineEdit)
{
    Pothos::PluginRegistry::add("/flow/EntryWidgets/LineEdit", Pothos::Callable(&makeLineEdit));
}

#include "LineEdit.moc"
