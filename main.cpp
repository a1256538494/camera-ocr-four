#include "modbuscamera.h"
#include "main_interface.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(true);
    main_interface m;
    //modbusCamera w;
    m.show();
    //w.show();
    return a.exec();
}
