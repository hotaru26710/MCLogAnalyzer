#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("MInecraft日志分析器");

    ui->analyzeBtn->setEnabled(false);
    ui->progressBar->setValue(0);
    ui->logPreview->setPlaceholderText("导入日志文件后，这里会显示文件预览...");



}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_openBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择MineCraft日志文件",
        QDir::homePath(),
        "日志文件(*.log);;所有文件(*.*)");

    if(filePath.isEmpty()){
        return;
    }

    m_currentFilePath=filePath;
    ui->fileLabel->setText(filePath);
    ui->analyzeBtn->setEnabled(true);

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

    ui->progressBar->setValue(100);
    ui->analyzeBtn->setEnabled(true);

}

void MainWindow::showResult(const QString &errorType,const QString &suggestion,const QStringList &stack){

    ui->resultTree->clear();

    //错误类型
    QTreeWidgetItem *typeItem = new QTreeWidgetItem(ui->resultTree);
    typeItem->setText(0,"错误类型");
    typeItem->setText(1,errorType.isEmpty()?"未检测到崩溃":errorType);
    if(errorType.isEmpty()){
        typeItem->setForeground(1,Qt::green);
    }
    else{
        typeItem->setForeground(1,Qt::red);
    }

    //解决方案
    QTreeWidgetItem *suggestionItem = new QTreeWidgetItem(ui->resultTree);
    suggestionItem->setText(0,"解决方案");
    suggestionItem->setText(1,suggestion.isEmpty()?"无建议":suggestion);
    suggestionItem->setForeground(1,Qt::blue);

    //堆栈跟踪
    QTreeWidgetItem *stackItem = new QTreeWidgetItem(ui->resultTree);
    stackItem->setText(0,"堆栈跟踪");
    stackItem->setText(1,stack.isEmpty()?"无":QString("共 %1 条").arg(stack.size()));
    stackItem->setExpanded(true);

    int count = 0;
    for (const QString &line : stack) {
        if (count++ >= 20) break;
        QTreeWidgetItem *lineItem = new QTreeWidgetItem(stackItem);
        lineItem->setText(0, "  └─");
        lineItem->setText(1, line);
        lineItem->setForeground(1, Qt::darkGray);
    }

    ui->resultTree->expandAll();

}


































