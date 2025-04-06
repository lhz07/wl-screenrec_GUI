#include "mainwindow.h"

#include <QApplication>
#include <QGuiApplication>
#include <QSystemSemaphore>

int main(int argc, char *argv[])
{
    QString cmd;
    if (argc >= 2)
    {
        cmd = argv[1];
    }
    QString mem_name = "wl-screenrec_GUI_sharedmem";
    QSharedMemory* sharedMemory = new QSharedMemory(mem_name);
    QSystemSemaphore sema("wl-screenrec_GUI_sema", 0, QSystemSemaphore::Create);
    sharedMemory->attach();
    delete sharedMemory;
    sharedMemory = new QSharedMemory(mem_name);
    if (!sharedMemory->create(sizeof(Shortcuts))) {
        if (sharedMemory->error() == QSharedMemory::AlreadyExists) {
            sharedMemory->attach();
            if (sharedMemory->data() == nullptr){
                qDebug() << "It seems like you have run wl-screenrec_GUI in a new minimal isolated environment "
                            "that doesn't carry on some \"excessive\" variables, such as \"sudo su\" \"sudo -i\", "
                            "which led to a crash of wl-screenrec_GUI, "
                            "please reboot your computer now.";
                return 1;
            }
            sharedMemory->lock();
            auto share_data = static_cast<Shortcuts*>(sharedMemory->data());
            auto local_data = new Shortcuts(Shortcuts::NONE);
            memcpy(local_data, share_data, sizeof(Shortcuts));
            qDebug() << "load sharedMemory";
            if (*local_data == Shortcuts::RUNNING){
                if (cmd == "record"){
                    *local_data = Shortcuts::RECORD;
                } else if (cmd == "select") {
                    *local_data = Shortcuts::SELECT;
                }
                memcpy(share_data, local_data, sizeof(Shortcuts));
                sema.release(1);
            }
            sharedMemory->unlock();
            sharedMemory->detach();
            delete sharedMemory;
            delete local_data;
            return 0;
        } else {
            return 0;
        }
    }else{
        sharedMemory->lock();
        auto share_data = static_cast<Shortcuts*>(sharedMemory->data());
        auto local_data = new Shortcuts(Shortcuts::RUNNING);
        memcpy(share_data, local_data, sizeof(Shortcuts));
        sharedMemory->unlock();
        qDebug() << "create sharedMemory";
        delete local_data;
    }
    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow w(sharedMemory);
    return a.exec();
}
