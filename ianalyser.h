#ifndef IANALYSER_H
#define IANALYSER_H

/***********************************************
    Interfaces for additional data analysis tasks
**************************************************/

#include <QtCore>

//we need NRadarAbstractPlot interface
#include <radardata/nradarabstractplot.h>
//coordinate conversion routines
#include <nradarmap.h>
//plot area
#include <radarview/nradarscene.h>

//the host interface represents "Analyser" application with all data and UI
class IAnalyser
{
public:
    IAnalyser() {}

    //set progress bar value in case the task is takes a long time
    virtual void setTaskProgress(int value,bool on=true) =0;
    //get all the data available
    virtual const QList<NRadarAbstractPlot*>& getAllData() const =0;
    //get the map, may be needed for coordiantes conversion
    virtual const NRadarMap* getActiveMap() const =0;
    //the scene
    virtual NRadarScene* getRadarScene() const =0;
    //if the plot is visible with current active filters
    bool isPlotVisible(const NRadarAbstractPlot* plot) const
    { return plot->getUserData()!=0; }
    //set plot color
    virtual void setPlotColor(const NRadarAbstractPlot* plot,const QColor& color) =0;
};

//base class for all tasks
class AnalyserTask:public QObject
{
Q_OBJECT

public:
    //type of task
    enum TaskType
    {
        SingleStage,//one-click-only task
        TwoStages   //first click is for actual work and the second for saving the results
    };

public:
    AnalyserTask(IAnalyser *a):QObject(0) {analyser=a;}

    //implement it to define the type
    virtual TaskType getType() const =0;
    //return two names used in the main menu -
    //one for 1st stage and other for 2nd stage, if applicable
    virtual QString getName(bool on) const =0;
    //run it, on = true - 1st stage, on = false - 2nd stage
    virtual bool execute(bool on) =0;

signals:
    //why it's here? o_O
    void finished(bool);

protected:
    IAnalyser *analyser;
};

#endif // IANALYSER_H
