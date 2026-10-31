#include <QApplication>
#include <QCoreApplication>
#include "GcsGUI.h" 
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <QPropertyAnimation>

int main(int argc, char* argv[])
{   
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    GcsGUI window;

    window.showFullScreen();

    return app.exec();
}



