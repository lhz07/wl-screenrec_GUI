#include "showframe.h"
#include "slurp_tool.h"
#include <qdebug.h>

ShowFrame::ShowFrame(QObject *parent) {}

void ShowFrame::set_geometry(QRect geometry)
{
    this->geo = geometry;
}

void ShowFrame::stop()
{
    change_running();
}

void ShowFrame::change_status(const QString text)
{
    change_text(text.toUtf8().data());
}

void ShowFrame::run()
{
    char **strr = 0;
    struct screen_box area = {.x = geo.x() - 2,
                              .y = geo.y() - 2,
                              .width = geo.width() + 4,
                              .height = geo.height() + 4};
    show_selected_area(0, strr, area);
}
