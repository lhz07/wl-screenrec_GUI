#ifndef REACTCMD_H
#define REACTCMD_H

#include <QThread>

class ReactCmd : public QThread
{
    Q_OBJECT
public:
    explicit ReactCmd(QObject *parent = nullptr);
protected:
    void run() override;
signals:
    void memoryChanged();
};

#endif // REACTCMD_H
