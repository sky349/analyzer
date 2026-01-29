#ifndef REFLECTORSTASK_H
#define REFLECTORSTASK_H

#include <radarview/nradarobjectitem.h>

#include "ianalyser.h"

class NRadarTrackPlot;

class ReflectorsTask:public AnalyserTask
{
    Q_OBJECT

public:
    ReflectorsTask(IAnalyser *analyser);

    TaskType getType() const {return TwoStages; }
    QString getName(bool) const;
    bool execute(bool);

    void clear();

protected:
    QPointF predict(const NRadarTrackPlot *tplot, int msecs);
    int hasReflector(double azimuth);
    void findReflector(const NRadarTrackPlot *tplot);
    void addReflector(const QPointF &pos, int heigth, const QPointF &other, int boardNumber);

protected slots:
    void removeReflector();

protected:
    struct Track
    {
        int id;
        QList<NRadarTrackPlot*> history;
    };

    struct Reflector
    {
        QPointF pos;
        NRadarObjectItem* mark;
    };

protected:
    QTime lastWorkingUpdate;

    QVector<Reflector> reflectors;
    QMap<int,Track> tracks;

    double azim_err;
    double rev_time;
};


class ReflectorItem:public NRadarObjectItem
{
Q_OBJECT

public:
    ReflectorItem(const QPointF& pos,const QString& text,NRadarScene* scene);
    ~ReflectorItem();

    void addHintLines(const QPointF bug,const QPointF good);

    void paint(QPainter *painter,const NRadarItemOption *option);
    QRect boundingRect() const;

signals:
    void removeMe();

protected:
    void selectionEvent(bool on);

protected slots:
    void hideLabel();

protected:
    QPainterPath path;
    QVector<QGraphicsLineItem*> lines;
    QGraphicsTextItem *label;
    QPen markPen;
};

#endif // REFLECTORSTASK_H
