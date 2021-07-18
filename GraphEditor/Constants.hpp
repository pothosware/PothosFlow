// Copyright (c) 2013-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QSizeF>
#include <QString>

//! selection flags used for getting object lists
static const int GRAPH_BLOCK = (1 << 0);
static const int GRAPH_BREAKER = (1 << 1);
static const int GRAPH_CONNECTION = (1 << 2);
static const int GRAPH_WIDGET = (1 << 3);

//! Default background color
const QString &defaultPaletteBackground(void);

//! Default window text color
const QString &defaultPaletteForeground(void);

/*!
 * Select between color based on system dark mode
 */
QString darkColorSupport(const QString &light, const QString &dark);

static const QString GraphObjectHighlightPenColor = "#33FFFF";
#define GraphObjectDefaultPenColor darkColorSupport("black", "white")
static const QString GraphObjectDefaultFillColor = "#E8E8E8";
static const qreal GraphObjectBorderWidth = 0.5;

static const QString ConnectModeHighlightPenColor = "#00ff00";
static const qreal ConnectModeLineWidth = 1.5;
static const qreal ConnectModeHighlightWidth = 2.0;

static const QString GraphObjectConnPointColor = "#FF85C2";
#define GraphObjectConnLineColor darkColorSupport("black", "white")
static const qreal GraphObjectConnPointRadius = 3;
static const qreal GraphObjectConnLineLength = 10;

static const qreal GraphDrawScrollFudge = 20;
#define GraphDrawBackgroundColor darkColorSupport("#FCFFFF", defaultPaletteBackground())
static const qreal GraphDrawZoomStep = 0.1;
static const qreal GraphDrawZoomMax = 1.5;
static const qreal GraphDrawZoomMin = 0.5;

static const qreal GraphBlockPortTextHPad = 1.5;
static const qreal GraphBlockPortTextVPad = 1.5;
static const qreal GraphBlockPropTextHPad = 5;
static const qreal GraphBlockPropTextVPad = 3;
static const qreal GraphBlockPortVGap = 10;
static const qreal GraphBlockPortVOutterPad = 10;
static const qreal GraphBlockTitleVPad = 5;
static const qreal GraphBlockTitleHPad = 5;
static const qreal GraphBlockPortArc = 3;
static const qreal GraphBlockMainArc = 5;
static const qreal GraphBlockSignalPortSpan = 15;
static const qreal GraphBlockSignalPortLength = GraphBlockSignalPortSpan/1.618;
static const int GraphBlockPropMaxWidthPx = 130;
static const int GraphBlockPropPointWidth = 8;
static const QString GraphBlockTitleFontSize = "9pt";
static const QString GraphBlockPropFontSize = "8pt";
static const QString GraphBlockPortFontSize = "8pt";
static const QString GraphBlockDisabledColor = "#A0A0A0";
static const qreal GraphBlockDisabledAlphaBlend = 0.5;

static const qreal GraphBreakerTitleVPad = 1.5;
static const qreal GraphBreakerTitleHPad = 1.5;
static const qreal GraphBreakerEdgeJut = 2.5;
static const QString GraphBreakerTitleFontSize = "8pt";

static const qreal GraphConnectionMinPling = 10;
static const qreal GraphConnectionGirth = 1.5;
static const qreal GraphConnectionArrowLen = 12;
static const qreal GraphConnectionArrowAngle = 30;
static const qreal GraphConnectionMaxCurve = 10;
static const qreal GraphConnectionSelectPad = 5;
static const qreal GraphConnectionDisabledXLen = 8;
static const QString GraphConnectionLineTextColor = "#848482";
static const QString GraphConnectionLineTextFontSize = "8pt";
#define GraphConnectionDefaultColor darkColorSupport("black", "white")
static const QString GraphConnectionHighlightColor = "#0040FF";
static const QString GraphConnectionDisabledColor = "#A0A0A0";

static const QString GraphWidgetGripLabelFontSize = "6pt";
#define GraphWidgetGripLabelColor darkColorSupport("#484848", "white")
#define GraphWidgetBackgroundColor darkColorSupport("beige", "gray")
