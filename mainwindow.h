#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "CrashParser.h"

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>
#include <QMap>

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

    void on_themeToggle_clicked();

private:
    Ui::MainWindow *ui;
    QString m_currentFilePath;
    CrashParser m_parser;

    // 外观：深浅主题（不影响任何分析逻辑）
    bool m_dark = false;
    void applyTheme();

    void showResult(const QString &errorType, const QString &suggestion, const QStringList &stack);
    void showSystemInfo(const QMap<QString,QString> &sysInfo);
};
#endif // MAINWINDOW_H
