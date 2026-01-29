#include "ampscatterdlg.h"
#include "ui_ampscatter.h"
#include <QDebug>

ampScatterTaskDlg::ampScatterTaskDlg(QwtPlot *plot, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ampscatter)
{
    ui->setupUi(this);
    m_plot = plot;
    ui->verticalLayout_2->addWidget(m_plot);


    replyPowerCurve = new QwtPlotCurve();
    replyPowerCurve->setPen(QPen(Qt::black, 2));
    replyPowerCurve->attach(m_plot);

    m_FL = 140;
    m_padding = 0;
    m_Z1_range = 12;
    m_Z1_att = 24;
    m_Z2_range = 30;
    m_Z2_att = 16;

    ui->Z1range_edit->setText(QString::number(round(m_Z1_range)));
    ui->Z1att_edit->setText(QString::number(round(m_Z1_att)));
    ui->Z2range_edit->setText(QString::number(round(m_Z2_range)));
    ui->Z2att_edit->setText(QString::number(round(m_Z2_att)));
    ui->FL_edit->setText(QString::number(round(m_FL)));
    ui->Padding_edit->setText(QString::number(round(m_padding)));
}

ampScatterTaskDlg::~ampScatterTaskDlg()
{
    delete ui;
    delete replyPowerCurve;
}

void ampScatterTaskDlg::showGraph()
{
    replyPowerCurve->detach();

    double mssrAntHeight = 14;   // [m] Radar antenna height,
    double coneOfSilenceReq = 45;   // [deg] Cone of silence required

    double targetAltitude_fl = m_FL;  // [Flight Level]. 1 unit is 100 feet

    double antennaTilt = 0; // [degrees] Antenna tilt,

        // Transponder parameters
    double transponderTxPower = 54;   // [dBm]
    double transponderLosses = 3;   // [dB]

    double transponderMaxInpPowr = -21; // dBm  maximal signal level
    double transponderMinInpPowr = -71; //
    double transponderAntGain    = 0;

    // MSSR parameters
    double mssrTxPower = 64;  // [dBm]
    double mssrFeederLosses = 3; // [dB]
    double antGainOffFromCenter = 3; // [dB] Antenna gain at beam edge

    // MSSR Receiver dynamic range
    double mssrMaxInpPowr = -30; // dBm  maximal signal level
    double mssrMinInpPowr = -83; // -83 (decoder level) + 6(at beam edge)

    double uplinkAtmosphereLosses  = 0.0065; // [dB/NM]
    double dowlinkAtmosphereLosses = 0.009; // [dB/NM]

    double downlinkEqConst  = -98.550; // [dB] Radar equation constant for range in NM for Downlink
    double uplinkEqConst    = -98.058; // [dB] Radar equation constant for range in NM for Uplink

    double Re = (4.0/3.0)* 6.371*1e6;  // [m] Earth radius

    double metersInFlightLevel = 100.0*0.3048; // [m/metersInFlightLevel]; Annex10 v4 pp2.1.3.2.2
    double metersInNauticalMile = 1852;

    double maxGain = 27; // dBi Maximal Antenna Gain -- in the center of the SUM beam

    std::vector<double> ant_pattern_deg {-10, -5, -1, 0, 5, 7.5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60};
    std::vector<double> ant_pattern_dB {-22, -25, -9.5, -6, -1, 0, -1, -6, -14, -14, -14, -14, -16, -18, -22, -28, -34};

    tk::spline s(ant_pattern_deg, ant_pattern_dB);

    QVector<double> antennaPatternAngle;
    QVector<double> ant_gain_src;

    for (double i=*std::min_element(ant_pattern_deg.begin(), ant_pattern_deg.end());
         i<=*std::max_element(ant_pattern_deg.begin(), ant_pattern_deg.end()); i+=0.2)
    {
        antennaPatternAngle.append(i);
        ant_gain_src.append(s(i) + maxGain);
    }

    double targetAltitude = targetAltitude_fl * metersInFlightLevel;
    double targetMinRange = targetAltitude -  mssrAntHeight;

    QVector<double> targetRangeLim;
    QVector<double> calcElevAngles;

    for (double targetRange = 0; targetRange<256; targetRange+=0.1)
    {
        double rangeInMeters = targetRange*metersInNauticalMile;

        if (rangeInMeters > targetMinRange)
        {
            targetRangeLim.append(rangeInMeters);
            double ratio = (pow(Re + mssrAntHeight, 2) + pow(rangeInMeters,2)
                            - pow(Re + targetAltitude, 2)) / (2 * rangeInMeters * (Re + mssrAntHeight));
            calcElevAngles.append(acos(ratio) * 180.0 / M_PI - 90.0);
        }
    }

    QVector<double> mssrAntennaGain(targetRangeLim.size(), ant_gain_src.last());

    for (int i=antennaPatternAngle.size()-2; i>=0; i--)
        for (int j=0; j<mssrAntennaGain.size(); j++)
            if (calcElevAngles[j] > antennaPatternAngle[i] && calcElevAngles[j] <= antennaPatternAngle[i+1])
                mssrAntennaGain[j] = ant_gain_src[i];

    QVector<double> interrogPower;
    QVector<double> replyPower;

    for (int i=0; i<mssrAntennaGain.size(); i++)
    {
        interrogPower.append(
            mssrTxPower
            - mssrFeederLosses
            + transponderAntGain
            - antGainOffFromCenter
            - transponderLosses
            - uplinkAtmosphereLosses*(targetRangeLim[i]/metersInNauticalMile)
            + uplinkEqConst
            + mssrAntennaGain[i]
            - 20*log10(targetRangeLim[i]/metersInNauticalMile));

        replyPower.append(
            transponderTxPower
            - mssrFeederLosses
            + transponderAntGain
            - antGainOffFromCenter
            - transponderLosses
            - dowlinkAtmosphereLosses*(targetRangeLim[i]/metersInNauticalMile)
            + uplinkEqConst
            + mssrAntennaGain[i]
            - 20*log10(targetRangeLim[i]/metersInNauticalMile)
            +m_padding);
    }

    for (int i=0; i<targetRangeLim.size(); i++)
    {
        if (targetRangeLim[i]<=m_Z1_range*1000)
            replyPower[i] -= m_Z1_att;

        if (targetRangeLim[i]>m_Z1_range*1000 && i<=m_Z2_range*1000)
            replyPower[i] -= m_Z2_att;
    }

    replyPowerCurve->setSamples(targetRangeLim.data(), replyPower.data(), replyPower.size());
    replyPowerCurve->setRenderHint(QwtPlotItem::RenderAntialiased);
    replyPowerCurve->attach(m_plot);

    m_plot->replot();
}

void ampScatterTaskDlg::on_FL_edit_editingFinished()
{
    m_FL = ui->FL_edit->text().toDouble();
    showGraph();
}

void ampScatterTaskDlg::on_Padding_edit_editingFinished()
{
    m_padding = ui->Padding_edit->text().toDouble();
    showGraph();
}

void ampScatterTaskDlg::on_Z1range_edit_editingFinished()
{
    m_Z1_range = ui->Z1range_edit->text().toDouble();
    showGraph();
}

void ampScatterTaskDlg::on_Z1att_edit_editingFinished()
{
    m_Z1_att = ui->Z1att_edit->text().toDouble();
    showGraph();
}

void ampScatterTaskDlg::on_Z2range_edit_editingFinished()
{
    m_Z2_range = ui->Z2range_edit->text().toDouble();
    showGraph();
}


void ampScatterTaskDlg::on_Z2att_edit_editingFinished()
{
    m_Z2_att = ui->Z2att_edit->text().toDouble();
    showGraph();
}

