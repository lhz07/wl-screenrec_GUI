#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QElapsedTimer>
#include <QEventLoop>
#include <QIcon>
#include <QMainWindow>
#include <QProcess>
#include <QSettings>
#include <QSharedMemory>
#include <QSystemTrayIcon>
#include <QTimer>
#include "reactcmd.h"
#include "showframe.h"
#include "slurp_tool.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct LSelection {
    QRect geometry;
    QString label;
    LSelection():empty(true){}
    LSelection(struct screen_box geo, char* label)
        : geometry(geo.x, geo.y, geo.width, geo.height)
        , label(label)
        , empty(false)
    {}
    void clear(){
        this->empty = true;
    }
    bool is_Empty(){
        return this->empty;
    }
protected:
    bool empty;
};

struct LScreen : public LSelection{
    QRect logical_geometry;
    LScreen():LSelection(){}
    LScreen(struct screen_box geo, struct screen_box logical_geo, char* label)
        : LSelection(geo, label)
        , logical_geometry(logical_geo.x, logical_geo.y, logical_geo.width, logical_geo.height)
    {}
};
enum class Shortcuts { NONE, RUNNING, RECORD, SELECT };
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QSharedMemory* sharedMemory, QWidget *parent = nullptr);
    ~MainWindow();
public slots:
    void receive_shortcut();
private slots:
    void pushButton_browse_clicked();

    void pushButton_set_region_clicked();

    void pushButton_set_fullscreen_clicked();

    void pushButton_open_path_clicked();

    void checkBox_launch_window_stateChanged(bool checked);

private:
    Ui::MainWindow *ui;
    // QList<QScreen*> screen_list;
    QMap<QString, LScreen> output_list;
    LSelection selected_region;
    bool is_recording;
    bool is_force_ended = false;
    bool is_expected_to_terminate = false;
    QProcess record_process;
    QProcess region_process;
    QSystemTrayIcon* trayIcon;
    QSharedMemory* sharedMemory;
    ReactCmd* react_cmd;
    Shortcuts* local_data;
    QString region_geometry;
    QSettings config = QSettings("wl-screenrec_GUI", "Config");
    const QString GPU_path = "/dev/dri/";
    const QIcon recording_icon = QIcon(":/icon/green-recorder.svg");
    const QIcon normal_icon = QIcon(":/icon/simplescreenrecorder.svg");
    const QIcon record_button_icon = QIcon(":/icon/record.svg");
    const QIcon stop_button_icon = QIcon(":/icon/stop-fill.svg");
    void prepare_to_record();
    void record();
    void record_started();
    void record_finished(int exitCode);
    void region_select_finished();
    void process_time_out();
    void update_record_button_text(bool is_checked);
    void update_format_for_codec();
    void createActionsAndTray();
    void update_time_label();
    void about_to_quit();
    void get_all_outputs();
    void get_audio_device();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    // void createLoopback();
    ShowFrame frame;
    QTimer killer;
    QTimer update_timer;
    QElapsedTimer recording_time;
    QEventLoop loop;
    const QPair<QString, QString> MP4 = {"MP4 (.mp4)", "mp4"};
    const QPair<QString, QString> MKV = {"MKV (.mkv)", "mkv"};
    const QPair<QString, QString> MOV = {"MOV (.mov)", "mov"};
    const QPair<QString, QString> AVI = {"AVI (.avi)", "avi"};
    const QPair<QString, QString> FLV = {"FLV (.flv)", "flv"};
    const QPair<QString, QString> WEBM = {"WebM (.webm)", "webm"};
    const QPair<QString, QString> TS = {"MPEG-TS (.ts)", "ts"};
    const QPair<QString, QString> $3GP = {"3GP (.3gp)", "3gp"};
    const QMap<QString, QList<QPair<QString, QString>>> format_for_codec
        = {{"auto", {MP4, MKV, MOV, AVI, FLV, WEBM, TS}},
           {"avc", {MP4, MKV, MOV, AVI, FLV, TS}},
           {"hevc", {MP4, MKV, MOV, TS}},
           {"vp8", {WEBM, MKV, AVI}},
           {"vp9", {WEBM, MP4, MKV, AVI}},
           {"av1", {WEBM, MP4, MKV, MOV, TS, AVI}}};
    const QList<QPair<QString, QString>> codecs = {{"Auto", "auto"},
                                                   {"AVC (H.264)", "avc"},
                                                   {"HEVC (H.265)", "hevc"},
                                                   {"VP8", "vp8"},
                                                   {"VP9", "vp9"},
                                                   {"AV1", "av1"}};
};
#endif // MAINWINDOW_H
