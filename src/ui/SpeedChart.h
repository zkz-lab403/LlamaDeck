#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

class SpeedChart : public QWidget {
    Q_OBJECT

public:
    explicit SpeedChart(QWidget *parent = nullptr);

    void addSample(double tokensPerSecond);
    void clear();
    void setCapacity(int capacity);
    void setAccent(const QColor &accent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_data;
    int m_capacity = 120;
    QColor m_accent;
};
