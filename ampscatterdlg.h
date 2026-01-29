#ifndef AMPSCATTERDLG_H
#define AMPSCATTERDLG_H

#include <QWidget>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <cmath>
#include <spline.h>

namespace Ui {
class ampscatter;
}

class ampScatterTaskDlg : public QWidget
{
    Q_OBJECT

public:
    explicit ampScatterTaskDlg(QwtPlot *plot, QWidget *parent = nullptr);
    ~ampScatterTaskDlg();
    void showGraph();
    QwtPlotCurve *replyPowerCurve;
    QwtPlot *m_plot;
    double m_FL, m_padding, m_Z1_range, m_Z1_att, m_Z2_range, m_Z2_att;

private slots:
    void on_Z1range_edit_editingFinished();

    void on_FL_edit_editingFinished();

    void on_Padding_edit_editingFinished();

    void on_Z1att_edit_editingFinished();

    void on_Z2range_edit_editingFinished();

    void on_Z2att_edit_editingFinished();

private:
    Ui::ampscatter *ui;
};

#endif // AMPSCATTERDLG_H
