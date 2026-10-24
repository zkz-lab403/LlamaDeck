#include "Widgets.h"

#include "Theme.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(38, 22);

    m_animation = new QPropertyAnimation(this, "position", this);
    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        m_animation->stop();
        m_animation->setStartValue(m_position);
        m_animation->setEndValue(checked ? 1.0 : 0.0);
        m_animation->start();
    });
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(38, 22);
}

void ToggleSwitch::setPosition(qreal position)
{
    m_position = qBound(0.0, position, 1.0);
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF track(0.5, 2.5, width() - 1.0, height() - 5.0);
    const qreal radius = track.height() / 2.0;

    QColor trackColor = isChecked() ? Theme::Accent : QColor(39, 49, 67);
    if (!isEnabled())
        trackColor.setAlpha(90);

    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawRoundedRect(track, radius, radius);

    const qreal knobDiameter = track.height() - 4.0;
    const qreal travel = track.width() - knobDiameter - 4.0;
    const QRectF knob(track.left() + 2.0 + m_position * travel,
                      track.top() + 2.0, knobDiameter, knobDiameter);

    p.setBrush(isEnabled() ? QColor(255, 255, 255) : QColor(150, 160, 180));
    p.drawEllipse(knob);
}

StatCard::StatCard(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Card"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 13, 16, 12);
    layout->setSpacing(4);

    auto *header = new QHBoxLayout();
    header->setSpacing(7);
    m_dot = new QLabel(this);
    m_dot->setFixedSize(7, 7);
    m_dot->setStyleSheet(QStringLiteral("background-color:#6C8CFF; border-radius:3px;"));
    m_title = makeLabel(title, QStringLiteral("StatTitle"), this);
    header->addWidget(m_dot);
    header->addWidget(m_title);
    header->addStretch();
    layout->addLayout(header);

    auto *valueRow = new QHBoxLayout();
    valueRow->setSpacing(6);
    m_value = makeLabel(QStringLiteral("—"), QStringLiteral("StatValue"), this);
    m_unit = makeLabel(QString(), QStringLiteral("StatUnit"), this);
    valueRow->addWidget(m_value);
    valueRow->addWidget(m_unit, 0, Qt::AlignBottom);
    valueRow->addStretch();
    layout->addLayout(valueRow);

    m_caption = makeLabel(QString(), QStringLiteral("StatCaption"), this);
    layout->addWidget(m_caption);
    layout->addStretch();
}

void StatCard::setValue(const QString &value, const QString &unit)
{
    m_value->setText(value);
    m_unit->setText(unit);
    m_unit->setVisible(!unit.isEmpty());
}

void StatCard::setCaption(const QString &caption)
{
    m_caption->setText(caption);
}

void StatCard::setAccent(const QColor &color)
{
    m_dot->setStyleSheet(QStringLiteral("background-color:%1; border-radius:3px;")
                             .arg(color.name()));
}

Badge::Badge(const QString &text, const QString &variant, QWidget *parent)
    : QLabel(text, parent)
{
    setObjectName(QStringLiteral("Badge"));
    setAlignment(Qt::AlignCenter);
    setProperty("variant", variant);
}

void Badge::setVariant(const QString &variant)
{
    setProperty("variant", variant);
    repolish(this);
}

QFrame *makeCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("Card"));
    return card;
}

QFrame *makeDivider(QWidget *parent)
{
    auto *divider = new QFrame(parent);
    divider->setObjectName(QStringLiteral("Divider"));
    divider->setFrameShape(QFrame::NoFrame);
    return divider;
}

QLabel *makeLabel(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    if (!objectName.isEmpty())
        label->setObjectName(objectName);
    return label;
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
