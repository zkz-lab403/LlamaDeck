#include "SpeedChart.h"

#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

SpeedChart::SpeedChart(QWidget *parent)
    : QWidget(parent)
    , m_accent(Theme::Accent)
{
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void SpeedChart::addSample(double tokensPerSecond)
{
    m_data.append(qMax(0.0, tokensPerSecond));
    while (m_data.size() > m_capacity)
        m_data.removeFirst();
    update();
}

void SpeedChart::clear()
{
    m_data.clear();
    update();
}

void SpeedChart::setCapacity(int capacity)
{
    m_capacity = qBound(10, capacity, 600);
    while (m_data.size() > m_capacity)
        m_data.removeFirst();
    update();
}

void SpeedChart::setAccent(const QColor &accent)
{
    m_accent = accent;
    update();
}

void SpeedChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = QRectF(rect()).adjusted(44.0, 14.0, -16.0, -28.0);
    if (area.width() < 40.0 || area.height() < 30.0)
        return;

    QFont small = font();
    small.setPixelSize(10);
    p.setFont(small);

    double maximum = 0.0;
    for (double value : m_data)
        maximum = qMax(maximum, value);
    const double scaleMax = qMax(5.0, std::ceil(maximum * 1.15 / 5.0) * 5.0);

    const QFontMetrics fm(small);
    for (int i = 0; i <= 4; ++i) {
        const qreal y = area.bottom() - area.height() * double(i) / 4.0;
        p.setPen(QPen(QColor(30, 39, 53), 1.0, Qt::DashLine));
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));

        p.setPen(QColor(103, 115, 135));
        const QString label = QString::number(scaleMax * double(i) / 4.0, 'f',
                                              scaleMax < 10.0 ? 1 : 0);
        p.drawText(QRectF(0.0, y - 8.0, area.left() - 8.0, 16.0),
                   Qt::AlignRight | Qt::AlignVCenter, label);
    }

    const QStringList timeLabels{QStringLiteral("4 分钟前"), QStringLiteral("3 分钟前"),
                                 QStringLiteral("2 分钟前"), QStringLiteral("1 分钟前"),
                                 QStringLiteral("现在")};
    p.setPen(QColor(103, 115, 135));
    for (int i = 0; i < timeLabels.size(); ++i) {
        const qreal x = area.left() + area.width() * double(i) / 4.0;
        QRectF labelRect(x - 30.0, area.bottom() + 6.0, 60.0, 16.0);
        if (i == 0)
            labelRect.moveLeft(area.left());
        if (i == timeLabels.size() - 1)
            labelRect.moveRight(area.right());
        p.drawText(labelRect, Qt::AlignCenter, timeLabels.at(i));
    }

    if (m_data.isEmpty()) {
        p.setPen(QColor(94, 106, 128));
        p.drawText(area, Qt::AlignCenter, QStringLiteral("等待服务产生吞吐数据"));
        return;
    }

    const int count = m_data.size();
    const qreal step = area.width() / qMax(1, m_capacity - 1);
    const qreal x0 = area.right() - qMax(0, count - 1) * step;

    QPainterPath linePath;
    QVector<QPointF> points;
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        const qreal x = x0 + i * step;
        const qreal y = area.bottom()
            - qBound(0.0, m_data.at(i) / scaleMax, 1.0) * area.height();
        points.append(QPointF(x, y));
        if (i == 0)
            linePath.moveTo(x, y);
        else
            linePath.lineTo(x, y);
    }

    if (count == 1) {
        const QPointF only = points.first();
        QLinearGradient fill(only.x(), area.top(), only.x(), area.bottom());
        fill.setColorAt(0.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 60));
        fill.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 0));
        p.fillRect(QRectF(only.x() - step * 0.5, area.top(), step, area.height()), fill);
    } else {
        QPainterPath areaPath = linePath;
        areaPath.lineTo(points.last().x(), area.bottom());
        areaPath.lineTo(points.first().x(), area.bottom());
        areaPath.closeSubpath();

        QLinearGradient fill(0.0, area.top(), 0.0, area.bottom());
        fill.setColorAt(0.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 70));
        fill.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 2));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(areaPath);

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(m_accent, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(linePath);
    }

    const QPointF latest = points.last();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 45));
    p.drawEllipse(latest, 7.0, 7.0);
    p.setBrush(m_accent);
    p.drawEllipse(latest, 3.2, 3.2);

    p.setPen(QColor(210, 220, 238));
    QFont valueFont = font();
    valueFont.setPixelSize(11);
    valueFont.setWeight(QFont::DemiBold);
    p.setFont(valueFont);
    p.drawText(QRectF(area.right() - 130.0, area.top() - 10.0, 130.0, 16.0),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("最新 %1 t/s").arg(m_data.last(), 0, 'f', 1));
}
