#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QFileInfo>
#include <QColor>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Minecraft 日志分析器");

    ui->analyzeBtn->setEnabled(false);
    ui->progressBar->setValue(0);
    ui->logPreview->setPlaceholderText("导入日志文件后，这里会显示文件预览...");

    ui->tabWidget->setTabText(0,"分析结果");
    ui->tabWidget->setTabText(1,"系统信息");

    setMinimumWidth(720);
    setMinimumHeight(560);

    //默认浅色主题，并让“深色模式”按钮显示当前会切入的目标
    m_dark = false;
    ui->themeToggle->setText("🌙 深色模式");
    ui->themeToggle->setToolTip("在浅色 / 深色主题之间切换");
    applyTheme();

    statusBar()->showMessage("就绪：打开一个 latest.log 或 crash-report 即可自动诊断");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::applyTheme()
{
    //读取对应主题的 QSS，只影响外观，不影响任何功能
    QFile qss(m_dark ? ":/theme-dark.qss" : ":/theme-light.qss");
    QString style;
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        style = QString::fromUtf8(qss.readAll());
        qss.close();
    }
    setStyleSheet(style);
}

void MainWindow::on_themeToggle_clicked()
{
    m_dark = !m_dark;
    ui->themeToggle->setText(m_dark ? "☀️ 浅色模式" : "🌙 深色模式");
    applyTheme();
    // 结果信息颜色随主题刷新
    if (!m_currentFilePath.isEmpty()) {
        showResult(m_parser.getErrorType(), m_parser.getSuggestion(), m_parser.getStackTrace());
        showSystemInfo(m_parser.getSystemDetails());
    }
    statusBar()->showMessage(m_dark ? "已切换到深色主题" : "已切换到浅色主题", 2000);
}

void MainWindow::on_openBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择MineCraft日志文件",
        QDir::homePath(),
        "日志文件(*.log);;文本文件(*.txt);;所有文件(*.*)");

    if(filePath.isEmpty()){
        return;
    }

    m_currentFilePath=filePath;
    ui->fileLabel->setText(QFileInfo(filePath).fileName());
    ui->fileLabel->setToolTip(filePath);
    ui->analyzeBtn->setEnabled(true);
    ui->detailsTree->clear();

    //预览文件前200行
    QFile file(filePath);
    if(file.open(QIODevice::ReadOnly|QIODevice::Text)){
        QTextStream stream(&file);
        QString preview;
        int lineCount = 0;
        while(!stream.atEnd()&&lineCount<200){
            preview+=stream.readLine()+"\n";
            lineCount++;
        }
        file.close();
        ui->logPreview->setPlainText(preview+(lineCount==200?"\n...(仅显示前200行)":""));

    }

    ui->progressBar->setValue(0);
    ui->resultTree->clear();
}


void MainWindow::on_analyzeBtn_clicked()
{
    if(m_currentFilePath.isEmpty()){
        QMessageBox::warning(this,"提示","请先选择一个日志");
        return;
    }

    ui->analyzeBtn->setEnabled(false);
    ui->progressBar->setValue(20);

    //读取文件
    QFile file(m_currentFilePath);
    if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){
        QMessageBox::critical(this,"错误","无法读取文件");
        ui->analyzeBtn->setEnabled(true);
        return;
    }

    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();

    ui->progressBar->setValue(50);

    //开始解析
    m_parser.parse(content);

    ui->progressBar->setValue(80);

    //显示结果
    showResult(
        m_parser.getErrorType(),
        m_parser.getSuggestion(),
        m_parser.getStackTrace());

    showSystemInfo(m_parser.getSystemDetails());

    if (m_parser.hasCrash()) {
        statusBar()->showMessage("⚠️ 检测到崩溃，已定位根因并给出建议", 4000);
    } else {
        statusBar()->showMessage("✅ 该日志未发现崩溃报告（运行正常）", 4000);
    }

    ui->tabWidget->setCurrentIndex(0);
    ui->progressBar->setValue(100);
    ui->analyzeBtn->setEnabled(true);

}

void MainWindow::showResult(const QString &errorType,const QString &suggestion,const QStringList &stack){

    ui->resultTree->clear();

    //语义颜色（随深浅主题微调，保证可读性）——仅影响外观
    const QColor okColor   = m_dark ? QColor("#7fd98f") : QColor("#2f9e44");
    const QColor errColor  = m_dark ? QColor("#ff8f8a") : QColor("#d64541");
    const QColor sugColor  = m_dark ? QColor("#8db6ff") : QColor("#3366d6");
    const QColor codeColor = m_dark ? QColor("#6fd0c0") : QColor("#0f8c7e");

    //错误类型
    QTreeWidgetItem *typeItem = new QTreeWidgetItem(ui->resultTree);
    typeItem->setText(0,"错误类型");
    typeItem->setForeground(0, m_dark ? QColor("#c9d4e5") : QColor("#3c4a63"));
    typeItem->setText(1,errorType.isEmpty()?"未检测到崩溃":errorType);
    typeItem->setForeground(1,errorType.isEmpty()?okColor:errColor);

    //主要责任模组
    if (m_parser.hasMainMod()) {
        const QColor modColor = m_dark ? QColor("#ffc46b") : QColor("#c07a00");
        QString modText = m_parser.getMainModName();
        if (!m_parser.getMainModVersion().isEmpty())
            modText += "  " + m_parser.getMainModVersion();
        if (!m_parser.getMainModId().isEmpty()
                && m_parser.getMainModId().compare(m_parser.getMainModName(), Qt::CaseInsensitive) != 0)
            modText += "  (" + m_parser.getMainModId() + ")";

        QTreeWidgetItem *modItem = new QTreeWidgetItem(ui->resultTree);
        modItem->setText(0,"🎯 主要责任模组");
        modItem->setForeground(0, m_dark ? QColor("#c9d4e5") : QColor("#3c4a63"));
        modItem->setText(1,modText);
        modItem->setForeground(1,modColor);
        modItem->setExpanded(false);

        if (!m_parser.getMainModEvidence().isEmpty()) {
            QTreeWidgetItem *evItem = new QTreeWidgetItem(modItem);
            evItem->setText(0,"  └─ 命中证据");
            evItem->setText(1,m_parser.getMainModEvidence());
            evItem->setForeground(1, m_dark ? QColor("#8a9bb3") : QColor("#7b8494"));
        }
    } else if (m_parser.hasCrash()) {
        QTreeWidgetItem *noModItem = new QTreeWidgetItem(ui->resultTree);
        noModItem->setText(0,"🎯 主要责任模组");
        noModItem->setForeground(0, m_dark ? QColor("#c9d4e5") : QColor("#3c4a63"));
        noModItem->setText(1,"未定位到模组（可能是原版或引擎自身问题）");
        noModItem->setForeground(1, m_dark ? QColor("#9aa6ba") : QColor("#6a7487"));
    }

    //解决方案
    QTreeWidgetItem *suggestionItem = new QTreeWidgetItem(ui->resultTree);
    suggestionItem->setText(0,"💡 解决方案");
    suggestionItem->setForeground(0, m_dark ? QColor("#c9d4e5") : QColor("#3c4a63"));
    suggestionItem->setText(1,suggestion.isEmpty()?"无建议":suggestion);
    suggestionItem->setForeground(1,sugColor);

    //堆栈跟踪
    QTreeWidgetItem *stackItem = new QTreeWidgetItem(ui->resultTree);
    stackItem->setText(0,"📍 堆栈跟踪");
    stackItem->setForeground(0, m_dark ? QColor("#c9d4e5") : QColor("#3c4a63"));
    stackItem->setText(1,stack.isEmpty()?"无":QString("共 %1 条").arg(stack.size()));
    stackItem->setExpanded(true);

    int count = 0;
    for (const QString &line : stack) {
        if (count++ >= 20) break;
        QTreeWidgetItem *lineItem = new QTreeWidgetItem(stackItem);
        lineItem->setText(0, "  └─");
        lineItem->setText(1, line);
        lineItem->setForeground(1, codeColor);
    }

    ui->resultTree->expandAll();

}

void MainWindow::showSystemInfo(const QMap<QString,QString> &sysInfo){

    ui->detailsTree->clear();

    ui->detailsTree->setColumnWidth(0, 170);
    ui->detailsTree->setColumnWidth(1, 460);

    //主题色——只影响外观
    const QColor labelColor = m_dark ? QColor("#9db1cf") : QColor("#4a5468");
    const QColor valueColor = m_dark ? QColor("#e6edf8") : QColor("#22303f");
    const QColor errColor   = m_dark ? QColor("#ff8f8a") : QColor("#d64541");
    const QColor extraColor = m_dark ? QColor("#8a9bb3") : QColor("#7b8494");

    if(sysInfo.isEmpty()){
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->detailsTree);
        item->setText(0,"未识别到系统信息");
        item->setForeground(1,errColor);
        return;
    }

    //已加载模组概览
    if (!m_parser.getLoadedMods().isEmpty()) {
        const QList<ModEntry> &mods = m_parser.getLoadedMods();
        QTreeWidgetItem *modSummary = new QTreeWidgetItem(ui->detailsTree);
        modSummary->setText(0, "📦 已加载模组");
        modSummary->setForeground(0, labelColor);
        modSummary->setText(1, QString("共 %1 个").arg(mods.size()));
        modSummary->setForeground(1, valueColor);
        modSummary->setExpanded(false);

        int shown = 0;
        for (const ModEntry &me : mods) {
            if (me.id.compare("minecraft", Qt::CaseInsensitive) == 0
                    || me.id.compare("forge", Qt::CaseInsensitive) == 0
                    || me.id.compare("fabric", Qt::CaseInsensitive) == 0
                    || me.id.compare("quilt", Qt::CaseInsensitive) == 0)
                continue;
            if (shown++ >= 12)
                break;
            QTreeWidgetItem *row = new QTreeWidgetItem(modSummary);
            row->setText(0, "  └─ " + me.id);
            row->setText(1, me.displayName.isEmpty() ? me.version
                                                      : me.displayName + (me.version.isEmpty() ? "" : "  " + me.version));
            row->setForeground(1, extraColor);
        }
    }

    struct KeyInfo{
        QString key;
        QString displayName;
    };

    QList<KeyInfo> importantKeys = {
        {"Operating System","操作系统"},
        {"CPU","CPU"},
        {"CPUs","CPU核心数"},
        {"Memory","内存"},
        {"Graphics Card","显卡"},
        {"Java Version","Java版本"},
        {"JVM Flags","JVM参数"},
        {"Minecraft Version","Minecraft版本"},
        {"Launched Version","启动版本"},
        {"Mod Loader","模组加载器"},
        {"Backend API","渲染API"}
    };

    for(const KeyInfo &info : importantKeys){
        if(sysInfo.contains(info.key)){
            QTreeWidgetItem *item = new QTreeWidgetItem(ui->detailsTree);
            item->setText(0,info.displayName);
            item->setForeground(0,labelColor);
            item->setText(1,sysInfo[info.key]);
            item->setForeground(1,valueColor);
        }
    }

    // 如果有遗漏的关键信息，补充显示
    QStringList extraKeys = sysInfo.keys();
    for (const QString &key : extraKeys) {
        // 检查是否已经在重要列表中显示过了
        bool alreadyShown = false;
        for (const KeyInfo &info : importantKeys) {
            if (info.key == key) {
                alreadyShown = true;
                break;
            }
        }
        // 如果是额外的关键信息，显示出来
        if (!alreadyShown &&
            !key.endsWith(".dll") &&
            !key.endsWith(".DLL") &&
            !key.contains(" #")) {
            QTreeWidgetItem *item = new QTreeWidgetItem(ui->detailsTree);
            item->setText(0, key);
            item->setForeground(0,extraColor);
            item->setText(1, sysInfo[key]);
            item->setForeground(1, valueColor);
        }
    }

    ui->detailsTree->expandAll();
}


































