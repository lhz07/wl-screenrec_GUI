#include "mainwindow.h"
#include <QAudioDevice>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QMediaDevices>
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
    // system("pw-loopback --capture-props='node.name=VirtualMic media.class=Audio/Source'");
    connect(react_cmd, &ReactCmd::memoryChanged, this, &MainWindow::receive_shortcut);
    sharedMemory->attach();
    react_cmd->start();
    // this->create();
    // screen_list = QGuiApplication::screens();
    get_audio_device();
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
    connect(ui->pushButton_open_path,
            &QPushButton::clicked,
            this,
            &MainWindow::pushButton_open_path_clicked);
    connect(&region_process, &QProcess::finished, this, &MainWindow::region_select_finished);
    connect(ui->checkBox_launch_silently,
            &QCheckBox::clicked,
            this,
            &MainWindow::checkBox_launch_window_stateChanged);
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
    if (int index = ui->comboBox_audio_device->findData(config.value("last_audio_device", "none"));
        index != -1) {
        ui->comboBox_audio_device->setCurrentIndex(index);
    }
    ui->checkBox_launch_silently->setChecked(config.value("launch_silently", false).toBool());
    ui->textBrowser_about->setHtml(R"(
        <h2>wl-screenrec_GUI</h2>
        <p>Version 1.0.0</p>
        <p>Record screen conveniently with wl-screenrec</p>
        <p><i>Copyright © 2025  lhz</i></p>
        <p>This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.</p>
    
        <p>This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.</p>
    
        <p>You should have received a copy of the GNU General Public License
        along with this program.  If not, see <a href='https://www.gnu.org/licenses/'>https://www.gnu.org/licenses/</a>.</p>
        <p>Github: <a href='https://github.com/lhz07/wl-screenrec_GUI'>
        https://github.com/lhz07/wl-screenrec_GUI</a></p>
        <p>If you encounter some problem, please check that whther it is a bug of wl-screenrec. 
        If so, please 
        report issue with the command directly to its repository.</p>
        wl-screenrec: <a 
        href='https://github.com/russelltg/wl-screenrec'>https://github.com/russelltg/
        wl-screenrec</a></p>)");
    ui->textBrowser_about->setOpenExternalLinks(true);
    ui->textBrowser_commands->setMarkdown(R"(
### Record screen
```sh
wl-screenrec_GUI record  # start/stop record
```


### Select region
```sh
wl-screenrec_GUI select
```
)");
    // ui->textBrowser_about->setFixedHeight(100);
    createActionsAndTray();
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::iconActivated);
    if (!ui->checkBox_launch_silently->isChecked()) {
        this->show();
    }
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
        } else if (*local_data == Shortcuts::SELECT) {
            // qDebug() << "select";
            ui->pushButton_set_region->setChecked(true);
            pushButton_set_region_clicked();
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
    if (frame.isRunning()) {
        frame.change_status("Ready to record");
    }
    if (killer.isActive()) {
        killer.stop();
    }
    if (is_force_ended) {
        QString error = record_process.readAllStandardError();
        QMessageBox::warning(
            this,
            "Warning",
            QString("wl-screenrec has been killed forcely, please check the output and the "
                    "command\n\nOutput: \n%1\nExit Code: %2\n\nCommand: \n%3 %4")
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
        QDir dir_check(ui->lineEdit_path->text());
        if (dir_check.exists() && dir_check.isReadable()) {
            config.setValue("storage_path", ui->lineEdit_path->text());
        } else {
            QMessageBox::warning(this, "Warning", "Storage path is invalid!");
            return;
        }
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

            record_arguments << "-o" << output;
        }
        if (ui->comboBox_GPUs->currentText() != "Auto") {
            QString GPU = ui->comboBox_GPUs->currentText();

            record_arguments << "--dri-device" << GPU_path + GPU;
        }
        if (!ui->checkBox_use_GPU->isChecked()) {
            record_arguments << "--no-hw";
        }
        if (ui->comboBox_codec->currentData() != "auto") {
            record_arguments << "--codec" << ui->comboBox_codec->currentData().toString();
        }
        if (ui->comboBox_audio_device->currentData() != "none") {
            record_arguments << "--audio" << "--audio-device"
                             << ui->comboBox_audio_device->currentData().toString();
        }
        if (!ui->checkBox_variable_frame->isChecked()) {
            record_arguments << "--no-damage";
        }
        config.setValue("last_screen", ui->comboBox_screens->currentData().toString());
        config.setValue("last_GPU", ui->comboBox_GPUs->currentText());
        config.setValue("use_GPU", ui->checkBox_use_GPU->isChecked());
        config.setValue("last_codec", ui->comboBox_codec->currentData());
        config.setValue("last_audio_device", ui->comboBox_audio_device->currentData());
        config.setValue("use_variable_framerate", ui->checkBox_variable_frame->isChecked());
        config.setValue("last_format", ui->comboBox_format->currentData());

        record_arguments << "-f"
                         << QString("%1/wl-screenrec_%2.%3")
                                .arg(ui->lineEdit_path->text(),
                                     QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"),
                                     ui->comboBox_format->currentData().toString());
        qDebug() << record_arguments;
        record_process.setArguments(record_arguments);
        record_process.start();
        ui->pushButton_record->setEnabled(false);

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
    if (frame.isRunning()) {
        frame.change_status("Recording");
    }
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
    if (frame.isRunning()) {
        frame.stop();
        frame.wait();
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

void MainWindow::get_audio_device()
{
    ui->comboBox_audio_device->clear();
    ui->comboBox_audio_device->addItem("None", "none");
    const auto audio_outputs = QMediaDevices::audioOutputs();
    for (const auto &var : audio_outputs) {
        ui->comboBox_audio_device->addItem(var.description(), var.id() + ".monitor");
    }
    const auto audio_inputs = QMediaDevices::audioInputs();
    for (auto &&var : audio_inputs) {
        ui->comboBox_audio_device->addItem(var.description(), var.id());
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
    if (frame.isRunning()) {
        frame.stop();
        frame.wait();
    }
    frame.set_geometry(selected_region.geometry);
    frame.start();
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
    if (frame.isRunning()) {
        frame.stop();
        frame.wait();
    }
}

void MainWindow::pushButton_open_path_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(ui->lineEdit_path->text()));
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        qDebug() << "clicked";
        this->show();
    }
}

void MainWindow::checkBox_launch_window_stateChanged(bool checked)
{
    // qDebug() << checked;
    config.setValue("launch_silently", checked);
}
