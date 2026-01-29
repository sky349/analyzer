
#include <QtGui>

#include "appwindow.h"

#include <QtNetwork>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion"));

    AppWindow w;
    if(!w.property("valid").toBool())
        return 0;

    w.show();
    return a.exec();
}
