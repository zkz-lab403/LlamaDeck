#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace Icons {

enum class Glyph {
    Models,
    Monitor,
    History,
    Api,
    Settings,
    Play,
    Stop,
    Refresh,
    Folder,
    Copy,
    Send,
    Trash,
    Plus,
    Globe,
    Key,
    ChevronDown,
    Search,
    Export,
    Close,
    Restart,
    Logo
};

QPixmap pixmap(Glyph glyph, const QColor &color, int size = 18);
QIcon icon(Glyph glyph, const QColor &color, int size = 18);
QIcon navIcon(Glyph glyph, const QColor &offColor, const QColor &onColor, int size = 18);
QPixmap logo(int size);
QIcon appIcon();

} // namespace Icons
