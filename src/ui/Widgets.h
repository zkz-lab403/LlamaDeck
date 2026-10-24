#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QPropertyAnimation>

class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    qreal position() const { return m_position; }
    void setPosition(qreal position);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_position = 0.0;
    QPropertyAnimation *m_animation = nullptr;
};

class StatCard : public QFrame {
    Q_OBJECT

public:
    explicit StatCard(const QString &title, QWidget *parent = nullptr);

    void setValue(const QString &value, const QString &unit = QString());
    void setCaption(const QString &caption);
    void setAccent(const QColor &color);

private:
    QLabel *m_title = nullptr;
    QLabel *m_value = nullptr;
    QLabel *m_unit = nullptr;
    QLabel *m_caption = nullptr;
    QLabel *m_dot = nullptr;
};

class Badge : public QLabel {
    Q_OBJECT

public:
    explicit Badge(const QString &text, const QString &variant = QStringLiteral("accent"),
                   QWidget *parent = nullptr);

    void setVariant(const QString &variant);
};

QFrame *makeCard(QWidget *parent = nullptr);
QFrame *makeDivider(QWidget *parent = nullptr);
QLabel *makeLabel(const QString &text, const QString &objectName, QWidget *parent = nullptr);
void repolish(QWidget *widget);
