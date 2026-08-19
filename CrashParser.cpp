#include "CrashParser.h"
#include <QDebug>
#include <QRegularExpression>

CrashParser::CrashParser()
    : m_hasCrash(false)
{
}

void CrashParser::parse(const QString &logContent)
{
    // 每次解析前清空旧数据
    m_errorType.clear();
    m_suggestion.clear();
    m_stackTrace.clear();
    m_hasCrash = false;

    // 1. 检测崩溃标记
    QRegularExpression crashMarker("---- Minecraft Crash Report ----");
    QRegularExpressionMatch match = crashMarker.match(logContent);

    if (!match.hasMatch()) {
        qDebug() << "未检测到崩溃报告，日志运行正常";
        return;
    }

    m_hasCrash = true;
    qDebug() << "✅ 检测到崩溃报告，开始分析...";

    // 2. 截取崩溃报告段落（从标记开始到文件末尾）
    int crashStart = match.capturedStart();
    QString crashSection = logContent.mid(crashStart);

    // 3. 解析
    parseCrashReport(crashSection);
    parseStackTrace(crashSection);
}

void CrashParser::parseCrashReport(const QString &section)
{
    // 提取错误类型 - 找 "Description:" 行
    QRegularExpression descRe("Description:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch descMatch = descRe.match(section);

    if (descMatch.hasMatch()) {
        m_errorType = descMatch.captured(1).trimmed();
        qDebug() << "📌 错误类型:" << m_errorType;
    }

    // ========================================
    // 规则匹配 - 这里可以不断扩充
    // ========================================
    if (section.contains("OutOfMemoryError", Qt::CaseInsensitive)) {
        m_suggestion = "内存不足！建议分配更多内存给Minecraft，添加JVM参数 -Xmx4G（或更高）";
    }
    else if (section.contains("ModResolutionException", Qt::CaseInsensitive)) {
        m_suggestion = "模组加载冲突！检查模组版本是否兼容，或移除最近添加的模组";
    }
    else if (section.contains("java.lang.NullPointerException")) {
        m_suggestion = "空指针异常！可能是模组调用了不存在的对象，尝试更新模组或联系作者";
    }
    else if (section.contains("EXCEPTION_ACCESS_VIOLATION")) {
        m_suggestion = "JVM崩溃！尝试更新显卡驱动或Java版本，检查是否分配了过多内存";
    }
    else if (section.contains("ClassNotFoundException", Qt::CaseInsensitive)) {
        m_suggestion = "类找不到！某个模组依赖的库缺失，尝试重新安装模组或补全依赖";
    }
    else if (section.contains("Forge", Qt::CaseInsensitive) && section.contains("FML", Qt::CaseInsensitive)) {
        m_suggestion = "Forge模组加载失败！尝试更新Forge版本或检查模组兼容性";
    }
    else if (section.contains("Fabric", Qt::CaseInsensitive)) {
        m_suggestion = "Fabric模组加载失败！检查模组是否支持当前Fabric版本";
    }
    else if (section.contains("OpenGL", Qt::CaseInsensitive) || section.contains("GLFW", Qt::CaseInsensitive)) {
        m_suggestion = "渲染错误！更新显卡驱动，或关闭光影/材质包后再试";
    }
    else if (m_hasCrash) {
        m_suggestion = "未知错误类型。建议将完整日志粘贴到MCBBS或百度搜索解决方案";
    }
}

void CrashParser::parseStackTrace(const QString &section)
{
    // 提取堆栈 - 匹配 "at 类名(文件:行号)" 或 "at 类名(源文件)"
    QRegularExpression stackRe("\\s+at\\s+([^(]+)\\(([^)]+)\\)");
    QRegularExpressionMatchIterator it = stackRe.globalMatch(section);

    int count = 0;
    while (it.hasNext() && count < 20) {
        QRegularExpressionMatch match = it.next();
        QString className = match.captured(1).trimmed();
        QString location = match.captured(2).trimmed();
        m_stackTrace.append(className + " (" + location + ")");
        count++;
    }

    if (count > 0) {
        qDebug() << "提取到" << count << "条堆栈信息";
    }
}