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
class NRadarTrackPlot;
class PlotLabel;
class TrackPathsOverlayItem;

struct PlotPopupData
{
    QGraphicsProxyWidget *proxy;
    QGraphicsLineItem *line;
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
    SourceSelection getSelectedSource() const;
    ModeSSelection getSelectedModeS() const;
    QDateTime getSelectedBeginTime() const;
    QDateTime getSelectedEndTime() const;
    bool getShowPredictedTrackPoints() const;
    quint64 getTrackInstanceId(const NRadarTrackPlot *track) const;

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
    void populatePlotPopup(PlotLabel *label, NRadarItem *radarItem);
    int findPopupAtPos(const QPoint& viewPos);

    void clearTrackPaths();
    void rebuildTrackInstances();
    void rebuildTrackPaths();
    void ensureTrackPathOverlays();
    void addTrackPolyline(const QPolygonF& polyline,quint8 radarId,uint trackId,
                          quint64 trackInstance,bool adsb);
    void setTrackPathsVisible(bool visible);
    void updateTrackPathStyles();
    void setHighlightedTrack(const NRadarTrackPlot *track);

    QList<PlotPopupData> m_plotPopups;
    TrackPathsOverlayItem *m_trackOverlay;
    TrackPathsOverlayItem *m_adsbTrackOverlay;
    QHash<const NRadarTrackPlot*,quint64> m_trackInstances;

    int m_draggingPopupIndex;
    QPointF m_popupDragOffset;
    int m_altitudeMultiplier;
    bool m_hasHighlightedTrack;
    quint64 m_highlightedTrackInstance;
};

#endif // APPWINDOW_H
