#pragma once

//main interface header, read it first!
#include "ianalyser.h"

//inherits AnalyserTask interface
class FalsePSRTask:public AnalyserTask
{
public:
    //IAnalyser holds all the data and provides QGraphicsScene for drawing
    FalsePSRTask(IAnalyser *analyser);

    //should return type of the task
    TaskType getType() const {return SingleStage; }
    //name for adding to the main menu
    QString getName(bool firstStage) const;

    //run it!
    bool execute(bool firstStage);
};

