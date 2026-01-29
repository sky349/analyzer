#pragma once

/****************************
 The simpliest possible example of Analyser task
****************************/

//main interface header, read it first!
#include "ianalyser.h"

//inherits AnalyserTask interface
class AmplitudesTask:public AnalyserTask
{
public:
    //IAnalyser holds all the data and provides QGraphicsScene for drawing
    AmplitudesTask(IAnalyser *analyser);

    //should return type of the task
    TaskType getType() const {return SingleStage; }
    //name for adding to the main menu
    QString getName(bool firstStage) const;

    //run it!
    bool execute(bool firstStage);

protected:
    QPropertyAnimation gradient;

    double increaseRatio;
    int minLevel;
};

