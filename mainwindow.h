#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "CrashParser.h"

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_openBtn_clicked();

    void on_analyzeBtn_clicked();

private:
    Ui::MainWindow *ui;
    QString m_currentFilePath;
    CrashParser m_parser;

    void showResult(const QString &errorType, const QString &suggestion, const QStringList &stack);

};
#endif // MAINWINDOW_H
