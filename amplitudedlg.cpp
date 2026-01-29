#include "amplitudedlg.h"
#include "ui_amplitudedlg.h"

AmplitudeDlg::AmplitudeDlg(int ratio, int level):QDialog(0),ui(new Ui::AmplitudeDlg)
{
    ui->setupUi(this);
    ui->spinIncrease->setValue(ratio);
    ui->spinLevel->setValue(level);
}

AmplitudeDlg::~AmplitudeDlg()
{
    delete ui;
}

int AmplitudeDlg::getIncreaseRatio() const
{ return ui->spinIncrease->value(); }

int AmplitudeDlg::getMinLevel() const
{ return ui->spinLevel->value(); }
