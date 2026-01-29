
//header for tracks class
#include <radardata/nradartrackplot.h>

#include "ampscattertask.h"
#include <qwt_plot.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_histogram.h>
#include <qwt_symbol.h>
#include <ampscatterdlg.h>

ampScatterTask::ampScatterTask(IAnalyser *analyser):AnalyserTask(analyser)
{
}

QString ampScatterTask::getName(bool firstStage) const
{
    return tr("Amplitude/range scatter");
}

bool ampScatterTask::execute(bool firstStage)
{
    QVector<double> amplitudesAssoc, rangesAssoc, amplitudesNAssoc, rangesNAssoc;

    foreach(NRadarAbstractPlot* data,analyser->getAllData())
    {
        if(data->getType()!=NRadarAbstractPlot::TypePlot)
            continue;

        if(!analyser->isPlotVisible(data))
            continue;

        NRadarPlot* plot=static_cast<NRadarPlot*>(data);

        if(plot->getSource()!=NRadarPlot::SSR) continue;

        int ampl=plot->getOption(NRadarPlot::RAW_SSRAmplitude).toInt();
        if(ampl>0)
        {
            if(plot->getPlotAssociation()==NRadarPlot::AssociatedWithTrack)
            {
                amplitudesAssoc.append(20*log10(ampl/18.0)-102);
                rangesAssoc.append(plot->getADCoord().y());
            }
            else
            {
                amplitudesNAssoc.append(20*log10(ampl/18.0)-102);
                rangesNAssoc.append(plot->getADCoord().y());
            }
        }
    }

    if (amplitudesAssoc.isEmpty() && amplitudesNAssoc.isEmpty())
    {
        QMessageBox::critical(0, "Amplitude/range scatter", "Error: too small number of plots");
        return false;
    }

    QwtPlotCurve *curveAssoc = new QwtPlotCurve;
    QwtPlotCurve *curveNAssoc = new QwtPlotCurve;

    QwtPlot *plot = new QwtPlot;
    QwtPlotMagnifier* pMagnifier = new QwtPlotMagnifier(plot->canvas());
    QwtPlotPanner* pPanner = new QwtPlotPanner(plot->canvas());

 //   QwtPlotZoomer* pZoomer = new QwtPlotZoomer(ui.p_Plot->canvas());

//    pZoomer->setKeyPattern( QwtEventPattern::KeyRedo, Qt::Key_I, Qt::ShiftModifier );
//    pZoomer->setKeyPattern( QwtEventPattern::KeyUndo, Qt::Key_O, Qt::ShiftModifier );
//    pZoomer->setKeyPattern( QwtEventPattern::KeyHome, Qt::Key_Home );

    curveAssoc->setSamples(rangesAssoc.data(), amplitudesAssoc.data(), amplitudesAssoc.size());
    QwtSymbol *symbolAssoc = new QwtSymbol(QwtSymbol::Ellipse,QBrush(Qt::green),QPen(Qt::green),QSize(9,9));
    curveAssoc->setSymbol(symbolAssoc);
    curveAssoc->setStyle(QwtPlotCurve::NoCurve);
    curveAssoc->setRenderHint(QwtPlotItem::RenderAntialiased);
    curveAssoc->attach(plot);

    curveNAssoc->setSamples(rangesNAssoc.data(), amplitudesNAssoc.data(), amplitudesNAssoc.size());
    QwtSymbol *symbolNAssoc = new QwtSymbol(QwtSymbol::Ellipse,QBrush(Qt::red),QPen(Qt::red),QSize(9,9));
    curveNAssoc->setSymbol(symbolNAssoc);
    curveNAssoc->setStyle(QwtPlotCurve::NoCurve);
    curveNAssoc->setRenderHint(QwtPlotItem::RenderAntialiased);
    curveNAssoc->attach(plot);

    // ***

    ampScatterTaskDlg *ampform = new ampScatterTaskDlg(plot);
    ampform->show();
    ampform->showGraph();

    plot->replot();
    plot->show();

    // ---
//    QVBoxLayout *vLayout = new QVBoxLayout;
//    vLayout->addWidget(plot);

    emit finished(true);
    return true;
}
