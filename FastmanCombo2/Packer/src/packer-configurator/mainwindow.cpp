#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSettings>
#include <QFileDialog>
#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QProcess>
#include <QDomDocument>
#include <QTextStream>
#include <QTextCodec>
#include <QMessageBox>
#include <QDebug>

static bool copyRecursively(const QString &srcFilePath, const QString &tgtFilePath) {
    QFileInfo srcFileInfo(srcFilePath);
    if (srcFileInfo.isDir()) {
        QDir targetDir(tgtFilePath);
        targetDir.cdUp();
        if (!targetDir.mkpath(QFileInfo(tgtFilePath).filePath())) {
            qDebug() << "mkpath error" << srcFilePath << tgtFilePath;
            return false;
        }
        QDir sourceDir(srcFilePath);
        QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        foreach (const QString &fileName, fileNames) {
            const QString newSrcFilePath
                    = srcFilePath + QLatin1Char('/') + fileName;
            const QString newTgtFilePath
                    = tgtFilePath + QLatin1Char('/') + fileName;
            if (!copyRecursively(newSrcFilePath, newTgtFilePath))
                return false;
        }
    } else {
        if (!QFile::copy(srcFilePath, tgtFilePath)) {
            qDebug() << "copy file error:" << srcFilePath << tgtFilePath;
            return false;
        }
    }
    return true;
}

static bool rmRecursively(const QString &dirPath) {
#if 1
    QProcess process;
#ifdef _WIN32
    QString path = dirPath;
    QStringList args;
    args << "/c" << "rd" << "/s" << "/q" << path.replace("/", "\\");
    process.start("cmd", args);
    process.waitForFinished(-1);
#else
    process.start(QString("rm -rf %1").arg(dirPath));
    process.waitForFinished(-1);
#endif
    QDir dir(dirPath);
    return !dir.exists();

#else

    QDir dir(dirPath);
    if (!dir.exists())
        return true;
    foreach(const QFileInfo &info, dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
        if (info.isDir()) {
            if (!rmRecursively(info.filePath()))
                return false;
        } else {
            if (!QFile::remove(info.absoluteFilePath())) {
                qDebug() << "xx" << info.absoluteFilePath();
                return false;
            }
        }
    }
    QDir parentDir(QFileInfo(dirPath).path());
    return parentDir.rmdir(QFileInfo(dirPath).fileName());
#endif
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->lineEdit_program, SIGNAL(textChanged(QString)), SLOT(slot_programChanged(QString)));
    connect(ui->btn_chooseTargetDir, SIGNAL(clicked()), SLOT(slot_chooseTargetDir()));
    connect(ui->btn_chooseIFWDir, SIGNAL(clicked()), SLOT(slot_chooseIFWDir()));
    connect(ui->btn_apply, SIGNAL(clicked()), SLOT(slot_apply()));

    connect(ui->btn_chooseTargetDirDual, SIGNAL(clicked()), SLOT(slot_chooseTargetDirDual()));
    connect(ui->btn_chooseNSISDir, SIGNAL(clicked()), SLOT(slot_chooseNSISDir()));
    connect(ui->lineEdit_versionDual, SIGNAL(textChanged(QString)), SLOT(slot_versionChangedDual(QString)));
    connect(ui->btn_applyDual, SIGNAL(clicked()), SLOT(slot_applyDual()));

    init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slot_programChanged(const QString &program)
{
    m_program = program;
}

void MainWindow::init()
{
    ui->tabWidget_outer->setCurrentIndex(0);
    ui->tabWidget_inner->setCurrentIndex(0);

    loadCfg();
    loadConfigXML();
    loadRootXML();
    loadReadmeXML();
    loadIncrementalXML();
}

void MainWindow::loadConfigXML()
{
    QDomDocument doc("mydoc");
    QFile file(QString("%1/config/config.xml").arg(m_targetDir));
    if (!doc.setContent(&file)) {
        return;
    }

    QDomNodeList nodes = doc.documentElement().childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QString tagName = nodes.at(i).toElement().tagName();
        QString data = nodes.at(i).toElement().text();
        if (tagName == "Name") {
            ui->lineEdit_program->setText(data);
        } else if (tagName == "Version") {
            ui->lineEdit_version->setText(data);
        } else if (tagName == "ProductUrl") {
            ui->lineEdit_productUrl->setText(data);
        } else if (tagName == "RunProgram") {
            ui->lineEdit_runProgram->setText(data);
        } else if (tagName == "RemoteRepositories") {
            QDomNodeList repositories = nodes.at(i).childNodes();
            if (repositories.count() != 1) {
                continue;
            }
            QDomNodeList nodes = repositories.at(0).childNodes();
            for (int i = 0; i < nodes.count(); ++i) {
                QString tagName = nodes.at(i).toElement().tagName();
                QString data = nodes.at(i).toElement().text();
                if (tagName == "Url") {
                    ui->lineEdit_repositoryUrl->setText(data);
                } else if (tagName == "Username") {
                    ui->lineEdit_repositoryUsername->setText(data);
                } else if (tagName == "Password") {
                    ui->lineEdit_repositoryPassword->setText(data);
                }
            }  // for
        }
    }
}

void MainWindow::loadRootXML()
{
    QDomDocument doc("mydoc");
    QFile file(QString("%1/packages/com.dengxian.mmqt2.root/meta/package.xml").arg(m_targetDir));
    if (!doc.setContent(&file)) {
        return;
    }

    QDomNodeList nodes = doc.documentElement().childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QString tagName = nodes.at(i).toElement().tagName();
        QString data = nodes.at(i).toElement().text();
        if (tagName == "Version") {
            ui->lineEdit_version_root->setText(data);
        }
    }

    file.close();
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    QTextStream stream(&file);
    doc.save(stream, 4);
    file.close();
}

void MainWindow::loadReadmeXML()
{
    QDomDocument doc("mydoc");
    QFile file(QString("%1/packages/com.dengxian.mmqt2.readme/meta/package.xml").arg(m_targetDir));
    if (!doc.setContent(&file)) {
        return;
    }

    QDomNodeList nodes = doc.documentElement().childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QString tagName = nodes.at(i).toElement().tagName();
        QString data = nodes.at(i).toElement().text();
        if (tagName == "Version") {
            ui->lineEdit_version_readme->setText(data);
        }
    }

    file.close();
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    QTextStream stream(&file);
    doc.save(stream, 4);
    file.close();
}

void MainWindow::loadIncrementalXML()
{
    QDomDocument doc("mydoc");
    QFile file(QString("%1/packages/com.dengxian.mmqt2.incremental/meta/package.xml").arg(m_targetDir));
    if (!doc.setContent(&file)) {
        return;
    }

    QDomNodeList nodes = doc.documentElement().childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QString tagName = nodes.at(i).toElement().tagName();
        QString data = nodes.at(i).toElement().text();
        if (tagName == "Version") {
            ui->lineEdit_version_incremental->setText(data);
        }
    }

    file.close();
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    QTextStream stream(&file);
    doc.save(stream, 4);
    file.close();
}

void MainWindow::setConfigXML()
{
    QDomDocument doc("mydoc");
    QFile file(QString("%1/config/config.xml").arg(m_targetDir));
    if (!doc.setContent(&file)) {
        return;
    }

    QDomNodeList nodes = doc.documentElement().childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QString tagName = nodes.at(i).toElement().tagName();
        if (tagName == "Name" || tagName == "Title" || tagName == "StartMenuDir") {
            QDomText newNode = doc.createTextNode(m_program);
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "RunProgramDescription") {
            QDomText newNode = doc.createTextNode(QString::fromUtf8("启动%1").arg(m_program));
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "Version") {
            QDomText newNode = doc.createTextNode(ui->lineEdit_version->text());
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "ProductUrl") {
            QDomText newNode = doc.createTextNode(ui->lineEdit_productUrl->text());
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "RunProgram") {
            QDomText newNode = doc.createTextNode(ui->lineEdit_runProgram->text());
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "ReleaseDate") {
            QDomText newNode = doc.createTextNode(QDate::currentDate().toString("yyyy-MM-dd"));
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "RemoteRepositories") {
            QDomNodeList repositories = nodes.at(i).childNodes();
            if (repositories.count() != 1) {
                continue;
            }
            QDomNodeList nodes = repositories.at(0).childNodes();
            for (int i = 0; i < nodes.count(); ++i) {
                QString tagName = nodes.at(i).toElement().tagName();
                if (tagName == "Url") {
                    QDomText newNode = doc.createTextNode(ui->lineEdit_repositoryUrl->text());
                    nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
                } else if (tagName == "Username") {
                    QDomText newNode = doc.createTextNode(ui->lineEdit_repositoryUsername->text());
                    nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
                } else if (tagName == "Password") {
                    QDomText newNode = doc.createTextNode(ui->lineEdit_repositoryPassword->text());
                    nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
                }
            }  // for
        }
    }

    file.close();
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    QTextStream stream(&file);
    doc.save(stream, 4);
    file.close();
}

void MainWindow::setRootXML()
{
    QDomDocument doc("mydoc");
    QFile file(QString("%1/packages/com.dengxian.mmqt2.root/meta/package.xml").arg(m_targetDir));
    if (!doc.setContent(&file)) {
        return;
    }

    QDomNodeList nodes = doc.documentElement().childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QString tagName = nodes.at(i).toElement().tagName();
        if (tagName == "DisplayName") {
            QDomText newNode = doc.createTextNode(m_program);
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "Version") {
            QDomText newNode = doc.createTextNode(ui->lineEdit_version_root->text());
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "ReleaseDate") {
            QDomText newNode = doc.createTextNode(QDate::currentDate().toString("yyyy-MM-dd"));
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        }
    }

    file.close();
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    QTextStream stream(&file);
    doc.save(stream, 4);
    file.close();
}

void MainWindow::setReadmeXML()
{
    if (ui->textEdit->toPlainText().isEmpty()) {
        slot_addLog("Update is empty, do not update README component.");
        return;
    }

    QDomDocument doc("mydoc");
    QFile file(QString("%1/packages/com.dengxian.mmqt2.readme/meta/package.xml").arg(m_targetDir));
    if (!doc.setContent(&file)) {
        return;
    }

    QDomNodeList nodes = doc.documentElement().childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QString tagName = nodes.at(i).toElement().tagName();
        if (tagName == "Version") {
            QDomText newNode = doc.createTextNode(ui->lineEdit_version_readme->text());
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "ReleaseDate") {
            QDomText newNode = doc.createTextNode(QDate::currentDate().toString("yyyy-MM-dd"));
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        }
    }

    file.close();
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    QTextStream stream(&file);
    doc.save(stream, 4);
    file.close();

    QFile readme(QString("%1/packages/com.dengxian.mmqt2.readme/data/README.txt").arg(m_targetDir));
    if (readme.open(QIODevice::ReadWrite)) {
        // NOTE: 处理中文
        QTextCodec *codec = QTextCodec::codecForName("UTF-8");
        QTextCodec::setCodecForTr(codec);
        QTextCodec::setCodecForLocale(codec);
        QTextCodec::setCodecForCStrings(codec);
        stream.setCodec(codec);

        QByteArray data = readme.readAll();
        QString updateLog;
        updateLog.append(QString("***************************%1 Release Note at %2\n").arg(ui->lineEdit_version->text()).arg(QDateTime::currentDateTime().toString()));
        updateLog.append(QString::fromLocal8Bit(ui->textEdit->toPlainText().toUtf8()));
        updateLog.append("\r\n");
        updateLog.append(data);
        readme.seek(0);
        readme.write(updateLog.toLocal8Bit());
        readme.flush();
        readme.close();
    } else {
        slot_addLog("Open README.txt failed, " + readme.errorString());
    }
}

void MainWindow::setIncrementalXML()
{
    QDomDocument doc("mydoc");
    QFile file(QString("%1/packages/com.dengxian.mmqt2.incremental/meta/package.xml").arg(m_targetDir));
    if (!doc.setContent(&file)) {
        return;
    }

    QDomNodeList nodes = doc.documentElement().childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        QString tagName = nodes.at(i).toElement().tagName();
        if (tagName == "Version") {
            QDomText newNode = doc.createTextNode(ui->lineEdit_version_incremental->text());
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        } else if (tagName == "ReleaseDate") {
            QDomText newNode = doc.createTextNode(QDate::currentDate().toString("yyyy-MM-dd"));
            nodes.at(i).replaceChild(newNode, nodes.at(i).firstChild());
        }
    }

    file.close();
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    QTextStream stream(&file);
    doc.save(stream, 4);
    file.close();
}

void MainWindow::loadCfg()
{
    QSettings settings(QString("%1.ini").arg(qApp->applicationName()), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    m_program = settings.value("Programe").toString();
    ui->lineEdit_program->setText(m_program);
    m_targetDir = settings.value("TargetDir").toString();
    ui->lineEdit_packerDir->setText(m_targetDir);
    m_IFWDir = settings.value("IFWDir").toString();
    ui->lineEdit_IFWDir->setText(m_IFWDir);

    m_NSISDir = settings.value("Dual/NSISDir").toString();
    ui->lineEdit_NSISDir->setText(m_NSISDir);
    m_targetDirDual = settings.value("Dual/TargetDir").toString();
    ui->lineEdit_packerDirDual->setText(m_targetDirDual);
    m_versionDual = settings.value("Dual/Version").toString();
    ui->lineEdit_versionDual->setText(m_versionDual);
}

void MainWindow::saveCfg()
{
    // NOTE: QSettings的key会对'0'-'9', 'a'-'z', 'A'-'Z', '_', '-', '.'之外的字符都会进行转义处理
    QSettings settings(QString("%1.ini").arg(qApp->applicationName()), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.setValue("Programe", m_program);
    settings.setValue("TargetDir", m_targetDir);
    settings.setValue("IFWDir", m_IFWDir);

    settings.setValue("Dual/NSISDir", m_NSISDir);
    settings.setValue("Dual/TargetDir", m_targetDirDual);
    settings.setValue("Dual/Version", m_versionDual);
    settings.sync();
}

void MainWindow::slot_chooseTargetDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                    qApp->applicationDirPath(),
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        ui->lineEdit_packerDir->setText(dir);
        m_targetDir = dir;

        loadConfigXML();
        loadRootXML();
        loadReadmeXML();
        loadIncrementalXML();
    }
}

void MainWindow::slot_chooseIFWDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                    qApp->applicationDirPath(),
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        ui->lineEdit_IFWDir->setText(dir);
        m_IFWDir = dir;
    }
}

void MainWindow::slot_apply()
{
    slot_addLog("Apply begin...", LOG_STEP);

    ui->btn_apply->setEnabled(false);

    bool applyResult = false;

    do {
        if (!slot_applyCheck()) {
            slot_addLog("Appy check failed, please try again!");
            break;
        }

        if (!ui->checkBox_offlineInstaller->isChecked()
                && !ui->checkBox_onlineInstaller->isChecked()
                && !ui->checkBox_repository->isChecked()) {
            slot_addLog("No Installer or Repository need applying, please choose one at least!!");
            break;
        }

        setConfigXML();
        setRootXML();
        setReadmeXML();
        setIncrementalXML();

        // clean .svn dir
        m_svnFilesMap.clear();
        QString hint;
        bool rmResult = slot_removeSvnFile(m_targetDir, hint);
        if (!rmResult) {
            slot_addLog("Rm .svn dir failed!");
            break;
        }

        QProcess builder;
        builder.setProcessChannelMode(QProcess::MergedChannels);
        if (ui->checkBox_offlineInstaller->isChecked()) {
            slot_addLog("Creating offline installer...", LOG_STEP);
            QString tools;
            QString targetName;
#ifdef _WIN32
            tools = QString("%1/binarycreator.exe").arg(m_IFWDir);
            targetName = QString("%1_%2_%3_Windows_Offline").arg(m_program).arg(ui->lineEdit_version->text()).arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));
#else
            tools = QString("%1/binarycreator").arg(m_IFWDir);
            targetName = QString("%1_%2_%3_Linux_Offline").arg(m_program).arg(ui->lineEdit_version->text()).arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));
#endif
            QString cmd = QString("\"%1\" --offline-only -c \"%2/config/config.xml\" -p \"%3/packages\" %4")
                    .arg(tools).arg(m_targetDir).arg(m_targetDir).arg(targetName);
            builder.start(cmd);
            if (!builder.waitForFinished(-1)) {
                slot_addLog(QString("Build failed:cmd=<%1>,err=<%2>").arg(cmd).arg(builder.errorString()), LOG_ERROR);
                break;
            } else {
                slot_addLog("Build succeed:" + builder.readAll().trimmed());
            }
        }

        if (ui->checkBox_onlineInstaller->isChecked()) {
            slot_addLog("Creating online installer...", LOG_STEP);
            QString tools;
            QString targetName;
#ifdef _WIN32
            tools = QString("%1/binarycreator.exe").arg(m_IFWDir);
            targetName = QString("%1_%2_%3_Windows_Online").arg(m_program).arg(ui->lineEdit_version->text()).arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));
#else
            tools = QString("%1/binarycreator").arg(m_IFWDir);
            targetName = QString("%1_%2_%3_Linux_Online").arg(m_program).arg(ui->lineEdit_version->text()).arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));
#endif

            QString cmd = QString("\"%1\" --online-only -c \"%2/config/config.xml\" -p \"%3/packages\" %4")
                    .arg(tools).arg(m_targetDir).arg(m_targetDir).arg(targetName);
            builder.start(cmd);
            if (!builder.waitForFinished(-1)) {
                slot_addLog(QString("Build failed:cmd=<%1>,err=<%2>").arg(cmd).arg(builder.errorString()), LOG_ERROR);
                break;
            } else {
                slot_addLog("Build succeed:" + builder.readAll().trimmed());
            }
        }

        if (ui->checkBox_repository->isChecked()) {
            slot_addLog("Creating repository ...", LOG_STEP);
            // clear first
            rmRecursively("repository");

            QString tools;
#ifdef _WIN32
            tools = QString("%1/repogen.exe").arg(m_IFWDir);
#else
            tools = QString("%1/repogen").arg(m_IFWDir);
#endif
            QString cmd = QString("\"%1\" -p \"%2/packages\" repository")
                    .arg(tools).arg(m_targetDir);
            builder.start(cmd);
            if (!builder.waitForFinished(-1)) {
                slot_addLog(QString("Build failed:cmd=<%1>,err=<%2>").arg(cmd).arg(builder.errorString()), LOG_ERROR);
                break;
            } else {
                slot_addLog("Build succeed:" + builder.readAll().trimmed());
            }
        }

        saveCfg();

        applyResult = true;

    } while (0);

    slot_restoreSvnFile();

    if (applyResult) {
        slot_addLog("Apply success", LOG_FINISHED);
    } else {
        slot_addLog("Apply failed", LOG_FINISHED);
    }

    ui->btn_apply->setEnabled(true);
}

bool MainWindow::slot_applyCheck()
{
    QString program = ui->lineEdit_program->text();
    QString packerDir = ui->lineEdit_packerDir->text();
    QString version = ui->lineEdit_version->text();
    QString runProgram = ui->lineEdit_runProgram->text();
    QString repositoryUrl = ui->lineEdit_repositoryUrl->text();

    slot_addLog("Check Program:" + program);
    if (program.isEmpty()) {
        QMessageBox::critical(this, tr("Apply error"), tr("Program is empty!"));
        return false;
    }

    slot_addLog("Check Packer Dir:" + packerDir);
    if (packerDir.isEmpty()) {
        QMessageBox::critical(this, tr("Apply error"), tr("Packer Dir is empty!"));
        return false;
    }

    slot_addLog("Check Version:" + version);
    if (version.isEmpty()) {
        QMessageBox::critical(this, tr("Apply error"), tr("Version is empty!"));
        return false;
    }

    slot_addLog("Check RunProgram:" + runProgram);
    if (runProgram.isEmpty()) {
        QMessageBox::critical(this, tr("Apply error"), tr("RunProgram is empty!"));
        return false;
    }

    slot_addLog("Check RepositoryUrl:" + repositoryUrl);
    if (repositoryUrl.isEmpty()) {
        QMessageBox::critical(this, tr("Apply error"), tr("RepositoryUrl is empty!"));
        return false;
    }

    return true;
}

void MainWindow::slot_chooseTargetDirDual()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                    qApp->applicationDirPath(),
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        ui->lineEdit_packerDirDual->setText(dir);
        m_targetDirDual = dir;
    }
}

void MainWindow::slot_chooseNSISDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                    qApp->applicationDirPath(),
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        ui->lineEdit_NSISDir->setText(dir);
        m_NSISDir = dir;
    }
}

void MainWindow::slot_versionChangedDual(const QString &version)
{
    m_versionDual = version;
}

void MainWindow::slot_applyDual()
{
    slot_addLog("Apply begin...", LOG_STEP);
    slot_addLog("Creating Dual installer...", LOG_STEP);

    ui->btn_applyDual->setEnabled(false);

    bool applyResult = false;
    do {
        if (!slot_applyCheckDual()) {
            slot_addLog("Appy check failed, please try again!");
            break;
        }

        // for components.xml
        QDomDocument doc("mydoc");
        QFile file(QString("%1/components.xml").arg(m_targetDirDual));
        if (!doc.setContent(&file)) {
            break;
        }

        QDomNodeList nodes = doc.documentElement().childNodes();
        for (int i = 0; i < nodes.count(); ++i) {
            QString tagName = nodes.at(i).toElement().tagName();
            if (tagName == "Package") {
                QDomNodeList nodesPackage = nodes.at(i).toElement().childNodes();
                for (int i = 0; i < nodesPackage.count(); ++i) {
                    QString tagName = nodesPackage.at(i).toElement().tagName();
                    if (tagName == "Version") {
                        QDomText newNode = doc.createTextNode(ui->lineEdit_versionDual->text());
                        nodesPackage.at(i).replaceChild(newNode, nodesPackage.at(i).firstChild());
                    } else if (tagName == "InstallDate") {
                        QDomText newNode = doc.createTextNode(QDate::currentDate().toString("yyyy-MM-dd"));
                        nodesPackage.at(i).replaceChild(newNode, nodesPackage.at(i).firstChild());
                    }
                }
            }  // for
        }

        file.close();
        if (!file.open(QFile::WriteOnly)) {
            break;
        }
        QTextStream stream(&file);
        doc.save(stream, 4);
        file.close();

        // for NSIS
        QFile NSISScript(QString("%1/packer_dual.nsi").arg(m_targetDirDual));
        if (NSISScript.open(QIODevice::ReadWrite)) {
            // NOTE: NSIS对编码的能力较弱
            QTextCodec *codec = QTextCodec::codecForName("unicode");
            QTextCodec::setCodecForTr(codec);
            QTextCodec::setCodecForLocale(codec);
            QTextCodec::setCodecForCStrings(codec);

            QTextStream in(&NSISScript);
            in.setCodec(codec);

            QString newData;
            QString line = in.readLine();
            bool found = false;
            while (!line.isNull()) {
                if (!found && line.contains("_Dual_Offline.exe")) {
                    line = QString("OutFile \"%1/${APP_NAME}_%2_%3_Dual_Offline.exe\"")
                            .arg(qApp->applicationDirPath())
                            .arg(ui->lineEdit_versionDual->text())
                            .arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));
                    found = true;
                }

                newData.append(line);
                newData.append("\r\n");

                line = in.readLine();
            }

            NSISScript.close();

            QFile::remove(QString("%1/packer_dual.nsi").arg(m_targetDirDual));
            QFile file(QString("%1/packer_dual.nsi").arg(m_targetDirDual));
            if (file.open(QIODevice::ReadWrite)) {
                file.seek(0);
                file.write(newData.toLocal8Bit());
                file.flush();
                file.close();
            } else {
                slot_addLog("Open packer_dual.nsi failed, " + file.errorString());
                break;
            }
        } else {
            slot_addLog("Open packer_dual.nsi failed, " + NSISScript.errorString());
            break;
        }

        // clean .svn dir
        m_svnFilesMap.clear();
        QString hint;
        bool rmResult = slot_removeSvnFile(m_targetDirDual, hint);
        if (!rmResult) {
            slot_addLog("Rm .svn dir failed!");
            break;
        }

        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);

        QString cmd = QString("\"%1/makensis.exe\" \"%2/packer_dual.nsi\"").arg(m_NSISDir).arg(m_targetDirDual);
        process.start(cmd);
        slot_addLog(cmd);
        if (!process.waitForFinished(-1)) {
            slot_addLog(QString("Build failed:cmd=<%1>,err=<%2>").arg(cmd).arg(process.errorString()), LOG_ERROR);
            break;
        } else {
            QByteArray out = process.readAll().trimmed();
            QTextStream in(&out);
            QString line = in.readLine();
            while (!in.atEnd()) {
                line = in.readLine();
            }

            if (line.startsWith("Total size:")) {
                slot_addLog("Build succeed:" + process.readAll().trimmed());
            } else {
                slot_addLog(QString("Build failed:cmd=<%1>,err=<%2>").arg(cmd).arg(process.errorString()), LOG_ERROR);
                break;
            }
        }

        saveCfg();

        applyResult = true;
    } while (0);

    slot_restoreSvnFile();

    if (applyResult) {
        slot_addLog("Apply success", LOG_FINISHED);
    } else {
        slot_addLog("Apply failed", LOG_FINISHED);
    }

    ui->btn_applyDual->setEnabled(true);
}

bool MainWindow::slot_applyCheckDual()
{
    QString targetDir = ui->lineEdit_packerDirDual->text();
    QString version = ui->lineEdit_versionDual->text();

    slot_addLog("Check TargetDir:" + targetDir);
    if (targetDir.isEmpty()) {
        QMessageBox::critical(this, tr("Apply error"), tr("TargetDir is empty!"));
        return false;
    }

    slot_addLog("Check Version:" + version);
    if (version.isEmpty()) {
        QMessageBox::critical(this, tr("Apply error"), tr("Version is empty!"));
        return false;
    }

    return true;
}

bool MainWindow::slot_removeSvnFile(const QString &targetDir, QString &hint)
{
    slot_addLog("Clean svn files like '.svn'");

    do {
        QDir dir(targetDir);
        if (!dir.exists()) {
            hint = "TargetDir not exists";
            break;
        }

        QDirIterator iter(targetDir,
                          QStringList() << ".svn",
                          QDir::Dirs | QDir::Hidden, QDirIterator::Subdirectories);
        while (iter.hasNext()) {
            iter.next();
            QString dirname = QCryptographicHash::hash(iter.filePath().toUtf8(), QCryptographicHash::Md5).toHex().toUpper();
            QString destPath = QString("%1/%2").arg(qApp->applicationDirPath()).arg(dirname);
            m_svnFilesMap.insert(iter.filePath(), destPath);
        }

        bool failed = false;
        QMap<QString, QString>::const_iterator i = m_svnFilesMap.constBegin();
        while (i != m_svnFilesMap.constEnd()) {
            QString srcDir = i.key();
            QString desDir = i.value();
            bool result = copyRecursively(srcDir, desDir);
            if (!result) {
                hint = QString("Copy failed, %1 %2").arg(srcDir).arg(desDir);
                failed = true;
                break;
            }
            result = rmRecursively(srcDir);
            if (!result) {
                hint = QString("Remove failed, %1").arg(srcDir);
                failed = true;
                break;
            }
            ++i;
        }
        if (failed) {
            break;
        }

        return true;

    } while (0);

    return false;
}

bool MainWindow::slot_restoreSvnFile()
{
    slot_addLog("Restore svn files like '.svn'");

    bool result;
    QMap<QString, QString>::const_iterator i = m_svnFilesMap.constBegin();
    while (i != m_svnFilesMap.constEnd()) {
        QString srcDir = i.value();
        QString desDir = i.key();
        bool result = copyRecursively(srcDir, desDir);
        if (!result) {
            result = false;
            break;
        }
        result = rmRecursively(srcDir);
        if (!result) {
            result = false;
            break;
        }
        ++i;
    }

    return result;
}

void MainWindow::slot_addLog(const QString &msg, int type)
{
    // save
    int fw = ui->textBrowser->fontWeight();
    QColor tc = ui->textBrowser->textColor();

    // append
    switch (type) {
    case LOG_NORMAL:
        ui->textBrowser->setFontWeight(QFont::Normal);
        ui->textBrowser->setTextColor(QColor("#000000"));
        break;
    case LOG_STEP:
        ui->textBrowser->setFontWeight(QFont::DemiBold);
        ui->textBrowser->setTextColor(QColor("#00ABEB"));
        break;
    case LOG_WARNING:
        ui->textBrowser->setFontWeight(QFont::DemiBold);
        ui->textBrowser->setTextColor(QColor("#FFC90E"));
        break;
    case LOG_ERROR:
        ui->textBrowser->setFontWeight(QFont::DemiBold);
        ui->textBrowser->setTextColor(QColor("#ED1C24"));
        break;
    case LOG_FINISHED:
        ui->textBrowser->setFontWeight(QFont::DemiBold);
        ui->textBrowser->setTextColor(QColor("#22B14C"));
        break;
    }

    QString now = QDateTime::currentDateTime().toString("[hh:mm:ss:zzz] ");
    ui->textBrowser->append(now + msg);

    // restore
    ui->textBrowser->setFontWeight(fw);
    ui->textBrowser->setTextColor(tc);
    ui->textBrowser->repaint();
}
