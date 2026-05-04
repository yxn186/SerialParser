#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("SerialParser");
    QApplication::setOrganizationName("SerialParser");
    QApplication::setWindowIcon(QIcon(":/icons/app_icon.png"));

    MainWindow window;
    window.setWindowIcon(QIcon(":/icons/app_icon.png"));
    window.resize(1500, 930);
    window.show();

    return app.exec();
}
