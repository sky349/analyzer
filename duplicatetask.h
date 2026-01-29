#pragma once

//main interface header, read it first!
#include "ianalyser.h"
#include <radarview/nradarobjectitem.h>

//inherits AnalyserTask interface
class DuplicateTask:public AnalyserTask
{
public:
    //IAnalyser holds all the data and provides QGraphicsScene for drawing
    DuplicateTask(IAnalyser *analyser);

    //should return type of the task
    TaskType getType() const {return SingleStage; }
    //name for adding to the main menu
    QString getName(bool firstStage) const;

    bool execute(bool firstStage);
private:
    QElapsedTimer lastWorkingUpdate;

    struct localTrack
    {
        QDateTime time;
        uint trackId;
        uint boardNumber;
        bool isModeS;
        uint icaoAddr;
        QPointF ADCoord;
    };
};

class dLine : public NRadarObjectItem
{
    Q_OBJECT

public:
    dLine(const QPointF& from, const QPointF& to, NRadarScene* scene, QColor color);
    ~dLine();
    void paint(QPainter *painter,const NRadarItemOption *option);
    QRect boundingRect() const;

private:
    QGraphicsLineItem *m_line;
};
