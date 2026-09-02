#ifndef CRASHPARSER_H
#define CRASHPARSER_H

#include <QString>
#include <QStringList>
#include <QMap>

// 一个已加载模组的简要信息
struct ModEntry {
    QString id;          // modid，如 "tconstruct"
    QString displayName; // 显示名，如 "Tinkers' Construct"
    QString version;     // 版本号
};

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

    // ---- 主要肇事模组相关 ----
    bool hasMainMod() const { return m_hasMainMod; }
    // 肇事模组的识别结果（无则空串）
    QString getMainModId() const { return m_mainModId; }
    QString getMainModName() const { return m_mainModName; }
    QString getMainModVersion() const { return m_mainModVersion; }
    // 命中证据：原始堆栈帧文本
    QString getMainModEvidence() const { return m_mainModEvidence; }
    // 已加载模组列表
    QList<ModEntry> getLoadedMods() const { return m_loadedMods; }

private:
    void parseCrashReport(const QString &section);

    void parseStackTrace(const QString &section);

    void parseSystemDetails(const QString &section);

    // 从 "Mod List" / "Mods:" 段提取所有已加载模组
    void parseModList(const QString &section);

    // 核心：从堆栈来源 jar 反查肇事模组
    void identifyMainMod(const QString &section);

private:
    QString m_errorType;//错误类型
    QString m_suggestion;//建议
    QStringList m_stackTrace;//堆栈跟踪列表;
    bool m_hasCrash;//是否崩溃

    QMap<QString,QString> m_systemDetails;

    // 肇事模组识别结果
    bool m_hasMainMod = false;
    QString m_mainModId;
    QString m_mainModName;
    QString m_mainModVersion;
    QString m_mainModEvidence;
    QList<ModEntry> m_loadedMods;
};

#endif // CRASHPARSER_H
