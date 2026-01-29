#ifndef AMPLITUDEDLG_H
#define AMPLITUDEDLG_H

#include <QDialog>

namespace Ui {
class AmplitudeDlg;
}

class AmplitudeDlg : public QDialog
{
    Q_OBJECT

public:
    explicit AmplitudeDlg(int ratio,int level);
    ~AmplitudeDlg();

    int getIncreaseRatio() const;
    int getMinLevel() const;

private:
    Ui::AmplitudeDlg *ui;
};

#endif // AMPLITUDEDLG_H
