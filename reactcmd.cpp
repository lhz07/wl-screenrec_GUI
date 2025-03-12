#include "reactcmd.h"
#include <QSystemSemaphore>

ReactCmd::ReactCmd(QObject *parent)
    : QThread{parent}
{}

void ReactCmd::run()
{
    QSystemSemaphore* sema = new QSystemSemaphore("wl-screenrec_GUI_sema", 0);
    while (1){
        sema->acquire();
        emit memoryChanged();
    }
}
