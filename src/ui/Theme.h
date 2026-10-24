#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>

namespace Theme {

inline const QColor Background(11, 14, 20);
inline const QColor Sidebar(14, 18, 25);
inline const QColor Surface(18, 23, 34);
inline const QColor SurfaceAlt(15, 20, 29);
inline const QColor Border(32, 40, 56);
inline const QColor BorderLight(42, 53, 73);
inline const QColor Text(231, 236, 245);
inline const QColor Muted(138, 149, 169);
inline const QColor Dim(108, 120, 139);
inline const QColor Accent(108, 140, 255);
inline const QColor AccentDeep(91, 108, 240);
inline const QColor Success(61, 220, 151);
inline const QColor Warning(245, 184, 74);
inline const QColor Danger(255, 107, 133);
inline const QColor Info(90, 200, 250);

QString styleSheet();
QPalette palette();
QFont applicationFont();

} // namespace Theme
