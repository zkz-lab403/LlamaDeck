#include "Icons.h"

#include "Theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace Icons {

namespace {

void addNavStates(QIcon &icon, Glyph glyph, const QColor &off, const QColor &on, int size)
{
    icon.addPixmap(pixmap(glyph, off, size), QIcon::Normal, QIcon::Off);
    icon.addPixmap(pixmap(glyph, on, size), QIcon::Normal, QIcon::On);
    icon.addPixmap(pixmap(glyph, on, size), QIcon::Active, QIcon::Off);
    icon.addPixmap(pixmap(glyph, on, size), QIcon::Active, QIcon::On);
    icon.addPixmap(pixmap(glyph, off, size), QIcon::Selected, QIcon::Off);
    icon.addPixmap(pixmap(glyph, on, size), QIcon::Selected, QIcon::On);
}

} // namespace

QPixmap pixmap(Glyph glyph, const QColor &color, int size)
{
    constexpr int dpr = 2;
    QPixmap pm(size * dpr, size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(double(size) / 24.0, double(size) / 24.0);

    QPen pen(color, 1.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    auto rounded = [&p](qreal x, qreal y, qreal w, qreal h, qreal r) {
        p.drawRoundedRect(QRectF(x, y, w, h), r, r);
    };

    switch (glyph) {
    case Glyph::Models:
        rounded(3.0, 3.0, 8.0, 8.0, 2.2);
        rounded(13.0, 3.0, 8.0, 8.0, 2.2);
        rounded(3.0, 13.0, 8.0, 8.0, 2.2);
        rounded(13.0, 13.0, 8.0, 8.0, 2.2);
        break;
    case Glyph::Monitor: {
        rounded(2.2, 4.5, 19.6, 15.0, 3.0);
        QPainterPath path;
        path.moveTo(6.0, 12.0);
        path.lineTo(8.6, 12.0);
        path.lineTo(10.5, 8.4);
        path.lineTo(12.6, 15.2);
        path.lineTo(14.6, 11.0);
        path.lineTo(18.2, 11.0);
        p.drawPath(path);
        break;
    }
    case Glyph::History:
        p.drawEllipse(QRectF(4.0, 4.0, 16.0, 16.0));
        p.drawLine(QPointF(12.0, 8.0), QPointF(12.0, 12.2));
        p.drawLine(QPointF(12.0, 12.2), QPointF(15.4, 14.2));
        break;
    case Glyph::Book:
        p.drawLine(QPointF(4.0, 5.5), QPointF(4.0, 19.0));
        p.drawLine(QPointF(20.0, 5.5), QPointF(20.0, 19.0));
        p.drawLine(QPointF(4.0, 5.5), QPointF(12.0, 7.0));
        p.drawLine(QPointF(12.0, 7.0), QPointF(20.0, 5.5));
        p.drawLine(QPointF(12.0, 7.0), QPointF(12.0, 20.2));
        p.drawLine(QPointF(4.0, 19.0), QPointF(12.0, 20.2));
        p.drawLine(QPointF(12.0, 20.2), QPointF(20.0, 19.0));
        break;
    case Glyph::Api: {
        QPainterPath left;
        left.moveTo(10.0, 4.0);
        left.lineTo(8.0, 4.0);
        left.lineTo(8.0, 9.4);
        left.lineTo(6.0, 12.0);
        left.lineTo(8.0, 14.6);
        left.lineTo(8.0, 20.0);
        left.lineTo(10.0, 20.0);
        p.drawPath(left);
        QPainterPath right;
        right.moveTo(14.0, 4.0);
        right.lineTo(16.0, 4.0);
        right.lineTo(16.0, 9.4);
        right.lineTo(18.0, 12.0);
        right.lineTo(16.0, 14.6);
        right.lineTo(16.0, 20.0);
        right.lineTo(14.0, 20.0);
        p.drawPath(right);
        break;
    }
    case Glyph::Settings:
        p.drawLine(QPointF(3.0, 6.5), QPointF(21.0, 6.5));
        p.drawLine(QPointF(3.0, 12.0), QPointF(21.0, 12.0));
        p.drawLine(QPointF(3.0, 17.5), QPointF(21.0, 17.5));
        p.drawEllipse(QPointF(15.0, 6.5), 2.2, 2.2);
        p.drawEllipse(QPointF(8.0, 12.0), 2.2, 2.2);
        p.drawEllipse(QPointF(17.0, 17.5), 2.2, 2.2);
        break;
    case Glyph::Play: {
        QPainterPath path;
        path.moveTo(8.0, 4.8);
        path.lineTo(19.0, 12.0);
        path.lineTo(8.0, 19.2);
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(path);
        break;
    }
    case Glyph::Stop:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        rounded(7.0, 7.0, 10.0, 10.0, 2.2);
        break;
    case Glyph::Refresh: {
        p.drawArc(QRectF(4.0, 4.0, 16.0, 16.0), 55 * 16, 285 * 16);
        QPainterPath arrow;
        arrow.moveTo(16.4, 9.4);
        arrow.lineTo(20.6, 11.8);
        arrow.lineTo(16.4, 14.2);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(arrow);
        break;
    }
    case Glyph::Folder: {
        QPainterPath path;
        path.moveTo(3.0, 8.0);
        path.lineTo(9.0, 8.0);
        path.lineTo(11.0, 10.2);
        path.lineTo(21.0, 10.2);
        path.lineTo(21.0, 19.0);
        path.lineTo(3.0, 19.0);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Glyph::Copy:
        rounded(3.5, 3.5, 11.0, 11.0, 2.4);
        rounded(9.5, 9.5, 11.0, 11.0, 2.4);
        break;
    case Glyph::Send: {
        QPainterPath path;
        path.moveTo(3.5, 11.8);
        path.lineTo(20.5, 4.0);
        path.lineTo(15.5, 20.0);
        path.lineTo(11.2, 13.8);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(11.2, 13.8), QPointF(20.5, 4.0));
        break;
    }
    case Glyph::Trash:
        p.drawLine(QPointF(4.0, 7.0), QPointF(20.0, 7.0));
        p.drawLine(QPointF(9.0, 7.0), QPointF(9.0, 4.6));
        p.drawLine(QPointF(9.0, 4.6), QPointF(15.0, 4.6));
        p.drawLine(QPointF(15.0, 4.6), QPointF(15.0, 7.0));
        p.drawLine(QPointF(6.0, 7.0), QPointF(7.0, 20.0));
        p.drawLine(QPointF(7.0, 20.0), QPointF(17.0, 20.0));
        p.drawLine(QPointF(17.0, 20.0), QPointF(18.0, 7.0));
        p.drawLine(QPointF(10.0, 10.5), QPointF(10.5, 16.5));
        p.drawLine(QPointF(14.0, 10.5), QPointF(13.5, 16.5));
        break;
    case Glyph::Plus:
        p.drawLine(QPointF(12.0, 5.0), QPointF(12.0, 19.0));
        p.drawLine(QPointF(5.0, 12.0), QPointF(19.0, 12.0));
        break;
    case Glyph::Globe:
        p.drawEllipse(QRectF(4.0, 4.0, 16.0, 16.0));
        p.drawEllipse(QRectF(8.0, 4.0, 8.0, 16.0));
        p.drawLine(QPointF(4.0, 12.0), QPointF(20.0, 12.0));
        break;
    case Glyph::Key:
        p.drawEllipse(QRectF(4.2, 4.2, 7.2, 7.2));
        p.drawLine(QPointF(10.0, 10.5), QPointF(19.5, 19.5));
        p.drawLine(QPointF(16.0, 16.0), QPointF(18.8, 13.2));
        p.drawLine(QPointF(13.4, 13.4), QPointF(16.0, 10.8));
        break;
    case Glyph::ChevronDown:
        p.drawLine(QPointF(8.0, 10.0), QPointF(12.0, 14.0));
        p.drawLine(QPointF(12.0, 14.0), QPointF(16.0, 10.0));
        break;
    case Glyph::Search:
        p.drawEllipse(QRectF(4.5, 4.5, 11.0, 11.0));
        p.drawLine(QPointF(14.4, 14.4), QPointF(20.0, 20.0));
        break;
    case Glyph::Export:
        p.drawLine(QPointF(12.0, 4.0), QPointF(12.0, 15.0));
        p.drawLine(QPointF(8.0, 11.0), QPointF(12.0, 15.0));
        p.drawLine(QPointF(12.0, 15.0), QPointF(16.0, 11.0));
        p.drawLine(QPointF(4.0, 18.5), QPointF(20.0, 18.5));
        break;
    case Glyph::Close:
        p.drawEllipse(QRectF(4.0, 4.0, 16.0, 16.0));
        p.drawLine(QPointF(9.0, 9.0), QPointF(15.0, 15.0));
        p.drawLine(QPointF(15.0, 9.0), QPointF(9.0, 15.0));
        break;
    case Glyph::Restart:
        p.drawArc(QRectF(4.0, 4.0, 16.0, 16.0), 55 * 16, 285 * 16);
        p.drawLine(QPointF(16.4, 9.4), QPointF(20.6, 11.8));
        p.drawLine(QPointF(20.6, 11.8), QPointF(16.4, 14.2));
        break;
    case Glyph::Logo:
        break;
    }

    p.end();
    return pm;
}

QIcon icon(Glyph glyph, const QColor &color, int size)
{
    return QIcon(pixmap(glyph, color, size));
}

QIcon navIcon(Glyph glyph, const QColor &offColor, const QColor &onColor, int size)
{
    QIcon result;
    addNavStates(result, glyph, offColor, onColor, size);
    return result;
}

QPixmap logo(int size)
{
    constexpr int dpr = 2;
    QPixmap pm(size * dpr, size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(double(size) / 48.0, double(size) / 48.0);

    QLinearGradient gradient(4.0, 4.0, 44.0, 44.0);
    gradient.setColorAt(0.0, QColor(122, 152, 255));
    gradient.setColorAt(0.55, QColor(102, 126, 255));
    gradient.setColorAt(1.0, QColor(91, 103, 235));

    QPainterPath background;
    background.addRoundedRect(QRectF(2.0, 2.0, 44.0, 44.0), 12.0, 12.0);
    p.setPen(Qt::NoPen);
    p.setBrush(gradient);
    p.drawPath(background);

    QPen shine(QColor(255, 255, 255, 40), 1.2);
    p.setPen(shine);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(3.0, 3.0, 42.0, 42.0), 11.0, 11.0);

    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(31);
    font.setWeight(QFont::Bold);
    p.setFont(font);
    p.setPen(QColor(255, 255, 255, 240));
    p.drawText(QRectF(2.0, 1.0, 44.0, 44.0), Qt::AlignCenter, QStringLiteral("λ"));

    p.end();
    return pm;
}

QIcon appIcon()
{
    QIcon result;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        result.addPixmap(logo(size));
    return result;
}

} // namespace Icons
