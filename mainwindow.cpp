#include "mainwindow.h"
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QSystemTrayIcon>
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QSharedMemory *sharedMemory, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , sharedMemory(sharedMemory)
    , local_data(new Shortcuts(Shortcuts::NONE))
    , react_cmd(new ReactCmd())
{
    ui->setupUi(this);
    ui->groupBox_selected_region->hide();
    connect(react_cmd, &ReactCmd::memoryChanged, this, &MainWindow::receive_shortcut);
    sharedMemory->attach();
    react_cmd->start();
    // screen_list = QGuiApplication::screens();
    ui->comboBox_screens->addItem("Auto", "auto");
    ui->comboBox_screens->setCurrentIndex(0);
    get_all_outputs();
    for (auto &&var : output_list) {
        ui->comboBox_screens->addItem(QString("%1 (%2 × %3)")
                                          .arg(var.label,
                                               QString::number(var.geometry.width()),
                                               QString::number(var.geometry.height())),
                                      var.label);
    }
    connect(ui->pushButton_record, &QPushButton::clicked, this, &MainWindow::prepare_to_record);
    connect(ui->pushButton_browse,
            &QPushButton::clicked,
            this,
            &MainWindow::pushButton_browse_clicked);
    connect(ui->pushButton_set_region,
            &QPushButton::clicked,
            this,
            &MainWindow::pushButton_set_region_clicked);
    connect(ui->pushButton_set_fullscreen,
            &QPushButton::clicked,
            this,
            &MainWindow::pushButton_set_fullscreen_clicked);
    connect(&region_process, &QProcess::finished, this, &MainWindow::region_select_finished);
    connect(&killer, &QTimer::timeout, this, &MainWindow::process_time_out);
    record_process.setProgram("wl-screenrec");
    ui->comboBox_GPUs->addItem("Auto");
    QDir GPUs(GPU_path);
    ui->comboBox_GPUs->addItems(GPUs.entryList(QDir::System));
    connect(&record_process, &QProcess::started, this, &MainWindow::record_started);
    connect(&record_process, &QProcess::finished, this, &MainWindow::record_finished);
    for (auto &&var : codecs) {
        ui->comboBox_codec->addItem(var.first, var.second);
    }
    connect(ui->comboBox_codec,
            &QComboBox::currentTextChanged,
            this,
            &MainWindow::update_format_for_codec);
    connect(&update_timer, &QTimer::timeout, this, &MainWindow::update_time_label);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::about_to_quit);
    // initial settings
    ui->lineEdit_path->setText(config.value("storage_path", QDir::homePath()).toString());
    ui->checkBox_hide->setChecked(config.value("hide_window_when_recording", false).toBool());
    ui->checkBox_use_GPU->setChecked(config.value("use_GPU", true).toBool());

    if (int index = ui->comboBox_screens->findData(config.value("last_screen", "auto"));
        index != -1) {
        ui->comboBox_screens->setCurrentIndex(index);
    }
    if (int index = ui->comboBox_GPUs->findText(config.value("last_GPU", "Auto").toString());
        index != -1) {
        ui->comboBox_GPUs->setCurrentIndex(index);
    }
    if (int index = ui->comboBox_codec->findData(config.value("last_codec", "auto")); index != -1) {
        ui->comboBox_codec->setCurrentIndex(index);
    }
    update_format_for_codec();
    if (int index = ui->comboBox_format->findData(config.value("last_format", "mp4")); index != -1) {
        ui->comboBox_format->setCurrentIndex(index);
    }

    createActionsAndTray();
}

MainWindow::~MainWindow()
{
    delete ui;
    react_cmd->deleteLater();
}

void MainWindow::receive_shortcut()
{
    auto share_data = static_cast<Shortcuts *>(sharedMemory->data());
    *local_data = Shortcuts::RUNNING;
    memcpy(local_data, share_data, sizeof(Shortcuts));
    if (*local_data != Shortcuts::RUNNING) {
        if (*local_data == Shortcuts::RECORD) {
            // qDebug() << "start";
            prepare_to_record();
        } else if (*local_data == Shortcuts::STOP) {
            qDebug() << "stop";
        }
        *local_data = Shortcuts::RUNNING;
        sharedMemory->lock();
        memcpy(share_data, local_data, sizeof(Shortcuts));
        sharedMemory->unlock();
    }
}

void MainWindow::record_finished(int exitCode)
{
    update_record_button_text(false);
    update_timer.stop();
    if (killer.isActive()) {
        killer.stop();
    }
    if (is_force_ended) {
        QString error = record_process.readAllStandardError();
        QMessageBox::warning(this,
                             "Warning",
                             QString("wl-screenrec has been killed forcely, "
                                     "please check the output and the command\n\n"
                                     "Output: \n%1\n"
                                     "Exit Code: %2\n\n"
                                     "Command: \n%3 %4")
                                 .arg(error,
                                      QString::number(exitCode),
                                      record_process.program(),
                                      record_process.arguments().join(' ')));
        is_force_ended = false;
    } else if (!is_expected_to_terminate) {
        if (exitCode) {
            QString error = record_process.readAllStandardError();
            QMessageBox::critical(this,
                                  "Error",
                                  QString("There is some error occured when running wl-screenrec "
                                          "and it quit unexpectedly, "
                                          "please check the output and the command\n\n"
                                          "Output: \n%1\n"
                                          "Exit Code: %2\n\n"
                                          "Command: \n%3 %4")
                                      .arg(error,
                                           QString::number(exitCode),
                                           record_process.program(),
                                           record_process.arguments().join(' ')));
        } else {
            QString error = record_process.readAllStandardError();
            QMessageBox::information(
                this,
                "Error",
                QString("wl-screenrec quit unexpectedly, but luckily, there is no error occured."
                        "Please check the output and the command\n\n"
                        "Output: \n%1\n"
                        "Exit Code: %2\n\n"
                        "Command: \n%3 %4")
                    .arg(error,
                         QString::number(exitCode),
                         record_process.program(),
                         record_process.arguments().join(' ')));
        }
    } else if (exitCode) {
        QString error = record_process.readAllStandardError();
        QMessageBox::critical(this,
                              "Error",
                              QString("There is some error occured when ending wl-screenrec and it "
                                      "quit with error exit code, "
                                      "please check the output and the command\n\n"
                                      "Output: \n%1\n"
                                      "Exit Code: %2\n\n"
                                      "Command: \n%3 %4")
                                  .arg(error,
                                       QString::number(exitCode),
                                       record_process.program(),
                                       record_process.arguments().join(' ')));
    }
    is_expected_to_terminate = false;
    if (loop.isRunning()) {
        loop.quit();
    }
}

void MainWindow::region_select_finished()
{
    qDebug() << region_process.readAllStandardOutput();
}

void MainWindow::process_time_out()
{
    qDebug() << "time out!";
    QMessageBox::StandardButton to_kill
        = QMessageBox::question(this, "Error", "It seems like wl-screenrec is stunned, kill it?");
    if (to_kill == QMessageBox::Yes) {
        killer.stop();
        record_process.kill();
        is_force_ended = true;
    }
}

void MainWindow::update_record_button_text(bool is_checked)
{
    ui->pushButton_record->setEnabled(true);
    is_recording = is_checked;
    if (is_checked) {
        ui->pushButton_record->setText("Stop Recording");
        ui->pushButton_record->setIcon(stop_button_icon);
        trayIcon->setIcon(recording_icon);
    } else {
        ui->pushButton_record->setText("Start Recording");
        ui->pushButton_record->setIcon(record_button_icon);
        ui->label_time->setText("00:00:00");
        trayIcon->setIcon(normal_icon);
    }
}

void MainWindow::update_format_for_codec()
{
    ui->comboBox_format->clear();
    for (auto &&var : format_for_codec[ui->comboBox_codec->currentData().toByteArray()]) {
        ui->comboBox_format->addItem(var.first, var.second);
    }
}

void MainWindow::pushButton_browse_clicked()
{
    QFileDialog::Options options;
    options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString directory = QFileDialog::getExistingDirectory(this,
                                                          "Select storage path",
                                                          ui->lineEdit_path->text(),
                                                          options);
    if (!directory.isEmpty()) {
        ui->lineEdit_path->setText(directory);
        // myset.setValue("default_path", directory);
        // myset.sync();
    }
}

void MainWindow::prepare_to_record()
{
    config.setValue("hide_window_when_recording", ui->checkBox_hide->isChecked());
    if (!is_recording && this->isVisible() && ui->checkBox_hide->isChecked()) {
        this->close();
        QTimer::singleShot(500, this, &MainWindow::record);
    } else {
        record();
    }
}

void MainWindow::record()
{
    if (!is_recording) {
        QStringList record_arguments;
        if (ui->pushButton_set_region->isChecked() && selected_region.is_Empty()) {
            QMessageBox::warning(this, "Warning", "Selected region is empty!");
            return;
        }
        if (ui->pushButton_set_region->isChecked()) {
            record_arguments << "-g"
                             << QString("%1,%2 %3x%4")
                                    .arg(QString::number(selected_region.geometry.x()),
                                         QString::number(selected_region.geometry.y()),
                                         QString::number(selected_region.geometry.width()),
                                         QString::number(selected_region.geometry.height()));
        } else if (ui->comboBox_screens->currentData().toByteArray() != "auto") {
            QString output = ui->comboBox_screens->currentData().toString();
            config.setValue("last_screen", output);
            record_arguments << "-o" << output;
        }
        if (ui->comboBox_GPUs->currentText() != "Auto") {
            QString GPU = ui->comboBox_GPUs->currentText();
            config.setValue("last_GPU", GPU);
            record_arguments << "--dri-device" << GPU_path + GPU;
        }
        if (!ui->checkBox_use_GPU->isChecked()) {
            config.setValue("use_GPU", ui->checkBox_use_GPU->isChecked());
            record_arguments << "--no-hw";
        }
        if (ui->comboBox_codec->currentData() != "auto") {
            config.setValue("last_codec", ui->comboBox_codec->currentData());
            record_arguments << "--codec" << ui->comboBox_codec->currentData().toString();
        }
        config.setValue("last_format", ui->comboBox_format->currentData());
        config.setValue("storage_path", ui->lineEdit_path->text());
        record_arguments << "-f"
                         << QString("%1/wl-screenrec_%2.%3")
                                .arg(ui->lineEdit_path->text(),
                                     QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"),
                                     ui->comboBox_format->currentData().toString());
        record_process.setArguments(record_arguments);
        record_process.start();
        ui->pushButton_record->setEnabled(false);
        qDebug() << record_arguments;
    } else {
        record_process.terminate();
        is_expected_to_terminate = true;
        ui->pushButton_record->setEnabled(false);
        killer.start(5000);
    }
}

void MainWindow::record_started()
{
    recording_time.restart();
    update_timer.start(1000);
    update_record_button_text(true);
}

void MainWindow::createActionsAndTray()
{
    auto showWindowAction = new QAction("Show", this);
    connect(showWindowAction, &QAction::triggered, this, &MainWindow::showNormal);
    auto quitAction = new QAction("Exit", this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    auto record = new QAction("Start/Stop Recording");
    connect(record, &QAction::triggered, this, &MainWindow::prepare_to_record);
    auto trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(showWindowAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(record);
    trayIconMenu->addAction(quitAction);
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(QIcon(":/icon/simplescreenrecorder.svg"));
    trayIcon->show();
}

void MainWindow::update_time_label()
{
    qint64 elapsed = recording_time.elapsed();
    // int mseconds = elapsed % 1000;
    int seconds = (elapsed / 1000) % 60;
    int minutes = (elapsed / (1000 * 60)) % 60;
    int hours = (elapsed / (1000 * 60 * 60)) % 24;
    ui->label_time->setText(QString("%1:%2:%3")
                                .arg(hours, 2, 10, QChar('0'))
                                .arg(minutes, 2, 10, QChar('0'))
                                .arg(seconds, 2, 10, QChar('0')));
}

void MainWindow::about_to_quit()
{
    if (update_timer.isActive()) {
        record();
        loop.exec();
    }
}

void MainWindow::get_all_outputs()
{
    char **strr = 0;
    struct screen_output *screens = (struct screen_output *) malloc(sizeof(struct screen_output));
    screens->next = NULL;
    struct seletion_box *selection = NULL;
    get_region(0, strr, selection, screens);
    output_list.clear();
    struct screen_output *screen_temp = NULL;
    while (screens->next != NULL) {
        screen_temp = screens->next;
        output_list[QString(screen_temp->label)] = LScreen(screen_temp->geometry,
                                                           screen_temp->logical_geometry,
                                                           screen_temp->label);
        free(screen_temp->label);
        screens->next = screens->next->next;
        free(screen_temp);
    }
    free(screens);
    for (auto &&var : output_list) {
        qDebug() << var.label;
        qDebug() << "logical_geo:" << var.logical_geometry;
        qDebug() << "geo:" << var.geometry;
    }
}

void MainWindow::pushButton_set_region_clicked()
{
    // region_process.setStandardOutputFile(QDir::currentPath() + "/output.txt");
    ui->label_screens->hide();
    ui->comboBox_screens->hide();
    char **strr = 0;
    struct screen_output *screens = (struct screen_output *) malloc(sizeof(struct screen_output));
    screens->next = NULL;
    struct seletion_box *selection = (struct seletion_box *) malloc(sizeof(struct seletion_box));
    selection->label = NULL;
    get_region(0, strr, selection, screens);
    if (selection->geometry.height != 0 || selection->geometry.width != 0) {
        selected_region = LSelection(selection->geometry, selection->label);
        qDebug() << selected_region.label << selected_region.geometry;
    }
    // qDebug() << selection->height;
    // qDebug() << selection->x << "," << selection->y;
    // qDebug() << selection->label;
    free(selection->label);
    free(selection);
    output_list.clear();
    struct screen_output *screen_temp = NULL;
    // screens = screens->next;
    while (screens->next != NULL) {
        screen_temp = screens->next;
        output_list[QString(screen_temp->label)] = LScreen(screen_temp->geometry,
                                                           screen_temp->logical_geometry,
                                                           screen_temp->label);
        free(screen_temp->label);
        screens->next = screens->next->next;
        free(screen_temp);
    }
    free(screens);
    if (selected_region.is_Empty()) {
        ui->pushButton_set_region->setChecked(false);
        ui->pushButton_set_fullscreen->setChecked(true);
        pushButton_set_fullscreen_clicked();
        return;
    }
    if (output_list[selected_region.label].logical_geometry.contains(selected_region.geometry)) {
        // qDebug() << "valid";
    } else {
        // qDebug() << "invalid";
        QRect biggest;
        for (auto &&screen : output_list) {
            QRect temp = screen.logical_geometry.intersected(selected_region.geometry);
            if (temp.width() * temp.height() > biggest.width() * biggest.height())
                biggest = temp;
        }
        if (config.value("warn_when_selection_too_big", true).toBool()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Warning");
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("wl-screenrec currently supports recording on only one screen. "
                           "It seems that your selection includes two or more screens. "
                           "However, wl-screenrec_GUI can attempt to use the largest intersection "
                           "automatically.");
            msgBox.setStandardButtons(QMessageBox::Ok);
            QCheckBox checkBox("Don't warn me again");
            msgBox.setCheckBox(&checkBox);
            msgBox.exec();
            if (checkBox.isChecked()) {
                config.setValue("warn_when_selection_too_big", false);
            }
        }
        selected_region.geometry = biggest;
    }
    ui->label_X->setText(QString::number(selected_region.geometry.x()));
    ui->label_Y->setText(QString::number(selected_region.geometry.y()));
    ui->label_width->setText(QString::number(selected_region.geometry.width()));
    ui->label_height->setText(QString::number(selected_region.geometry.height()));
    ui->groupBox_selected_region->show();

    // for (auto&& var : output_list) {
    //     qDebug() << var.label;
    //     qDebug() << "logical_geo:" << var.logical_geometry;
    //     qDebug() << "geo:" << var.geometry;
    // }
    // qDebug() << screens->label;
    // qDebug() << screens->geometry.x << screens->geometry.y << screens->geometry.height << screens->geometry.width;
    // qDebug() << screens->logical_geometry.x << screens->logical_geometry.y << screens->logical_geometry.height << screens->logical_geometry.width;
}

void MainWindow::pushButton_set_fullscreen_clicked()
{
    ui->label_screens->show();
    ui->comboBox_screens->show();
    ui->groupBox_selected_region->hide();
}
