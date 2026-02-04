#ifndef APPWINDOW_H
#define APPWINDOW_H

#include <QtGui>

#include "ui_appwindow.h"

#include <radardata/nradarabstractplot.h>
#include <radarview/nradarscene.h>
#include <radarview/nradaritem.h>

#include "ianalyser.h"

class DataPack;
class GuiFilter;
class QGraphicsLineItem;
class QGraphicsProxyWidget;
class PlotLabel;

struct PlotPopupData
{
    QGraphicsProxyWidget *proxy;
    QGraphicsLineItem *line;
    PlotLabel *label;
    NRadarItem *radarItem;
};

class AppWindow:public QMainWindow,public IAnalyser,private Ui_AppWindow
{
    Q_OBJECT

public:
    AppWindow();
    ~AppWindow();

public:
    void setTaskProgress(int value,bool on=true);
    const QList<NRadarAbstractPlot*>& getAllData() const;
    const NRadarMap* getActiveMap() const;
    NRadarScene* getRadarScene() const;

    //set plot color
    void setPlotColor(const NRadarAbstractPlot* plot,const QColor& color);

protected slots:
    void loadMap();
    bool importData(bool confirm=true);

    void initFilters(const DataPack *data);
    void applyFilter(bool fullFilter=true);
    void applyFilterToLayers();

	void updateViewMode(int m);

    QTreeWidgetItem* addItemToDetails(QTreeWidgetItem *parent,const QString& name,const QString& value=QString::null);
    void onPlotSelected(NRadarItem*);

    void executeTask(QAction *act);

protected:
    void initGUI();
    void initActions();
    bool eventFilter(QObject *obj,QEvent *event) override;

    void fillComboBox(QComboBox *cmb,const QMap<QString,bool>& list);

private:
    DataPack *dataPack;

    QVector<AnalyserTask*> tasks;

    NRadarScene *radarScene;
    QMap<int,NRadarSceneLayer*> layers;

    QList<GuiFilter*> filters;
    GuiFilter *areaFilter;

    QProgressBar *barWorking;
    QTime lastWorkingUpdate;

    QColor m_notAssociated, m_allCall, m_ssr, m_modeS, m_psr, m_combined;
    bool m_blackWhiteMode;

    void showPlotPopup(NRadarItem *radarItem,const QPointF& scenePos);
    void closeAllPlotPopups();
    void closePlotPopup(int index);
    void updatePlotPopupLines();
    void populatePlotPopup(PlotPopupData &popupData, NRadarItem *radarItem);
    int findPopupAtPos(const QPoint& viewPos);

    QList<PlotPopupData> m_plotPopups;

    bool m_draggingPopup;
    int m_draggingPopupIndex;
    QPointF m_popupDragOffset;
};

#endif // APPWINDOW_H
