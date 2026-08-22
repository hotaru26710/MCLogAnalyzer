#ifndef CRASHPARSER_H
#define CRASHPARSER_H

#include <QString>
#include <QStringList>
#include <QMap>

class CrashParser{
public:
    CrashParser();

    //核心分析方法
    void parse(const QString &logContent);

    //getter
    QString getErrorType() const{
        return m_errorType;
    }
    QString getSuggestion() const{
        return m_suggestion;
    }
    QStringList getStackTrace() const{
        return m_stackTrace;
    }
    bool hasCrash() const{
        return m_hasCrash;
    }
    QMap<QString, QString> getSystemDetails() const {
        return m_systemDetails;
    }

private:
    void parseCrashReport(const QString &section);

    void parseStackTrace(const QString &section);

    void parseSystemDetails(const QString &section);

private:
    QString m_errorType;//错误类型
    QString m_suggestion;//建议
    QStringList m_stackTrace;//堆栈跟踪列表;
    bool m_hasCrash;//是否崩溃

    QMap<QString,QString> m_systemDetails;
};

#endif // CRASHPARSER_H
