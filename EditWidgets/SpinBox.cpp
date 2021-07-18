// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QSpinBox>
#include <limits>

/***********************************************************************
 * SpinBox for integer entry
 **********************************************************************/
class SpinBox : public QSpinBox
{
    Q_OBJECT
public:
    SpinBox(QWidget *parent):
        QSpinBox(parent)
    {
        connect(this, &SpinBox::editingFinished, this, &SpinBox::widgetChanged);
        connect(this, QOverload<int>::of(&SpinBox::valueChanged), [=](int){this->handleWidgetChanged();});
    }

public slots:
    QString value(void) const
    {
        return _value;
    }

    void setValue(const QString &value)
    {
        QSpinBox::setValue(value.toInt());
        _value = value;
    }

signals:
    void commitRequested(void);
    void widgetChanged(void);
    void entryChanged(void);

private slots:
    void handleWidgetChanged(void)
    {
        _value = QSpinBox::text();
        emit this->widgetChanged();
    }

private:
    QString _value;
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeSpinBox(const QJsonArray &, const QJsonObject &kwargs, QWidget *parent)
{
    auto spinBox = new SpinBox(parent);
    spinBox->setMinimum(kwargs["minimum"].toInt(std::numeric_limits<int>::min()));
    spinBox->setMaximum(kwargs["maximum"].toInt(std::numeric_limits<int>::max()));
    spinBox->setSingleStep(kwargs["step"].toInt(1));
    spinBox->setDisplayIntegerBase(kwargs["base"].toInt(10));
    return spinBox;
}

pothos_static_block(registerSpinBox)
{
    Pothos::PluginRegistry::add("/flow/EntryWidgets/SpinBox", Pothos::Callable(&makeSpinBox));
}

#include "SpinBox.moc"
