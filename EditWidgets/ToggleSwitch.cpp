// Copyright (c) 2017-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QPushButton>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPropertyAnimation>
#include <QStaticText>
#include <algorithm> //min/max
#include <utility> //swap

/***********************************************************************
 * Toggle switch is a custom slider-like toggle widget
 *
 * Reference for painting style and animation techniques
 * https://stackoverflow.com/questions/14780517/toggle-switch-in-qt
 **********************************************************************/
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(int offset READ offset WRITE setOffset)
public:
    ToggleSwitch(QWidget *parent, const QString &onText, const QString &offText):
        QAbstractButton(parent),
        _onText(onText),
        _offText(offText),
        _m(3), //margin
        _x(this->startIndex()),
        _anim(new QPropertyAnimation(this, "offset", this))
    {
        this->setCheckable(true);
        this->setFocusPolicy(Qt::StrongFocus);
        this->setCursor(Qt::PointingHandCursor);
        connect(this, SIGNAL(toggled(bool)), this, SLOT(handleToggled(bool)));
    }

    int offset(void) const
    {
        return _x;
    }

    void setOffset(int off)
    {
        _x = off;
        this->update();
    }

    QSize sizeHint(void) const
    {
        const int minHeight(16);
        const QSize trackMinSize(2*minHeight, minHeight);
        const QSize textMaxSize(
            std::max(_onText.size().width(), _offText.size().width()),
            std::max(_onText.size().height(), _offText.size().height()));
        return QSize(2*_m, 2*_m) + QSize(
            std::max(textMaxSize.width(), trackMinSize.width()),
            std::max(textMaxSize.height(), trackMinSize.height()));
    }

public slots:
    QString value(void) const
    {
        return this->isChecked()?"true":"false";
    }

    void setValue(const QString &s)
    {
        this->setChecked(s=="true");
        this->updatePos(s=="true");
    }

signals:
    void commitRequested(void);
    void widgetChanged(void);
    void entryChanged(void);

private slots:
    void handleToggled(const bool checked)
    {
        int start = this->startIndex();
        int end = this->endIndex();
        if (not checked) std::swap(start, end);

        _anim->setStartValue(start);
        _anim->setEndValue(end);
        _anim->setDuration(120);
        _anim->start();

        emit this->entryChanged();
    }

protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        const int w = this->width();
        const int h = this->height();
        const auto &palette = this->palette();
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        //the border color to show selection
        const qreal penW(1.0);
        p.setPen(QPen(palette.color(this->hasFocus()?QPalette::Highlight:QPalette::Dark), penW));

        //draw the track
        p.setBrush(palette.brush(this->isChecked()?QPalette::Window:QPalette::Dark));
        p.drawRoundedRect(QRect(_m, _m, w - (2*_m), h - (2*_m)), 8.0, 8.0);

        //draw the thumb given the offset
        p.setBrush(palette.brush(this->isChecked()?QPalette::Midlight:QPalette::Mid));
        p.drawEllipse(QRectF(_x - (h/2)+penW, penW, h-penW*2, h-penW*2));

        //draw the text centered
        p.setPen(palette.color(this->isChecked()?QPalette::WindowText:QPalette::BrightText));
        const auto &text = this->isChecked()?_onText:_offText;
        p.drawStaticText((w-text.size().width())/2, (h-text.size().height())/2, text);
    }

    void resizeEvent(QResizeEvent *e)
    {
        this->updatePos(this->isChecked());
        QAbstractButton::resizeEvent(e);
    }

    void wheelEvent(QWheelEvent *e)
    {
        const bool checked = e->angleDelta().y() > 0;
        if (e->angleDelta().y() != 0 and checked != this->isChecked())
        {
            this->setChecked(checked);
            this->handleToggled(checked);
            return e->accept();
        }
        QAbstractButton::wheelEvent(e);
    }

private:
    void updatePos(const bool checked)
    {
        const auto finalPos = checked?this->endIndex():this->startIndex();

        //if an animation is active, correct to the new bounds
        if (_anim->state() == QAbstractAnimation::Running)
        {
            _anim->setEndValue(finalPos);
        }

        //otherwise just update the resting position
        else this->setOffset(finalPos);
    }

    int startIndex(void) const
    {
        return this->height()/2;
    }

    int endIndex(void) const
    {
        return this->width() - this->height()/2;
    }

    const QStaticText _onText;
    const QStaticText _offText;
    int _m, _x;
    QPropertyAnimation *_anim;
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeToggleSwitch(const QJsonArray &, const QJsonObject &kwargs, QWidget *parent)
{
    return new ToggleSwitch(parent, kwargs["on"].toString(), kwargs["off"].toString());
}

pothos_static_block(registerToggleSwitch)
{
    Pothos::PluginRegistry::add("/flow/EntryWidgets/ToggleSwitch", Pothos::Callable(&makeToggleSwitch));
}

#include "ToggleSwitch.moc"
