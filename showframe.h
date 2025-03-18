#ifndef SHOWFRAME_H
#define SHOWFRAME_H

#include <QRect>
#include <QThread>

class ShowFrame : public QThread
{
    Q_OBJECT
public:
    explicit ShowFrame(QObject *parent = nullptr);
    void set_geometry(QRect geometry);
    void stop();
    void change_status(const QString text);

protected:
    void run() override;

private:
    QRect geo;
};

#endif // SHOWFRAME_H
