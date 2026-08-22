#include "CrashParser.h"
#include <QDebug>
#include <QRegularExpression>

CrashParser::CrashParser()
    : m_hasCrash(false){
}

void CrashParser::parse(const QString &logContent){
    // 每次解析前清空旧数据
    m_errorType.clear();
    m_suggestion.clear();
    m_stackTrace.clear();
    m_systemDetails.clear();
    m_hasCrash = false;

    //检测崩溃标记
    QRegularExpression crashMarker("---- Minecraft Crash Report ----");
    QRegularExpressionMatch match = crashMarker.match(logContent);

    if (!match.hasMatch()) {
        qDebug() << "未检测到崩溃报告，日志运行正常";
        return;
    }

    m_hasCrash = true;
    qDebug() << "检测到崩溃报告，开始分析...";

    //截取崩溃报告段落（从标记开始到文件末尾）
    int crashStart = match.capturedStart();
    QString crashSection = logContent.mid(crashStart);

    //解析
    parseCrashReport(crashSection);
    parseStackTrace(crashSection);
    parseSystemDetails(crashSection);
}

void CrashParser::parseCrashReport(const QString &section){
    // 提取错误类型 - 找 "Description:" 行
    QRegularExpression descRe("Description:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch descMatch = descRe.match(section);

    if (descMatch.hasMatch()) {
        m_errorType = descMatch.captured(1).trimmed();
        qDebug() << "错误类型:" << m_errorType;
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

void CrashParser::parseStackTrace(const QString &section){
    // 提取堆栈 - 匹配 "at 类名(文件:行号)" 或 "at 类名(源文件)"
    QRegularExpression stackRe("(\\n|^)\\s+at\\s+([^(]+)\\(([^)]+)\\)");
    QRegularExpressionMatchIterator it = stackRe.globalMatch(section);

    int count = 0;
    while (it.hasNext() && count < 20) {
        QRegularExpressionMatch match = it.next();
        QString className = match.captured(2).trimmed();
        QString location = match.captured(3).trimmed();
        m_stackTrace.append(className + " (" + location + ")");
        count++;
    }

    if (count > 0) {
        qDebug() << "提取到" << count << "条堆栈信息";
    }
}

void CrashParser::parseSystemDetails(const QString &section)
{
    m_systemDetails.clear();

    //找到 Details 部分
    int detailsStart = section.lastIndexOf("Details:");
    if (detailsStart == -1) {
        qDebug() << "未找到 Details 部分";
        return;
    }

    //截取 Details 后的内容
    QString detailsSection = section.mid(detailsStart);


    //用正则提取关键信息（只在 Details 部分查找）

    //操作系统
    QRegularExpression osRe("^\\s*Operating System:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch osMatch = osRe.match(detailsSection);
    if (osMatch.hasMatch()) {
        m_systemDetails["Operating System"] = osMatch.captured(1).trimmed();
    }

    //Java 版本
    QRegularExpression javaRe("^\\s*Java Version:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch javaMatch = javaRe.match(detailsSection);
    if (javaMatch.hasMatch()) {
        m_systemDetails["Java Version"] = javaMatch.captured(1).trimmed();
    }

    //CPU
    QRegularExpression cpuRe("^\\s*CPU:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch cpuMatch = cpuRe.match(detailsSection);
    if (cpuMatch.hasMatch()) {
        m_systemDetails["CPU"] = cpuMatch.captured(1).trimmed();
    }

    //CPU核心数
    QRegularExpression cpusRe("^\\s*CPUs:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch cpusMatch = cpusRe.match(detailsSection);
    if (cpusMatch.hasMatch()) {
        m_systemDetails["CPUs"] = cpusMatch.captured(1).trimmed();
    }

    //内存
    QRegularExpression memRe("^\\s*Memory:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch memMatch = memRe.match(detailsSection);
    if (memMatch.hasMatch()) {
        m_systemDetails["Memory"] = memMatch.captured(1).trimmed();
    }

    //JVM参数
    QRegularExpression jvmRe("^\\s*JVM Flags:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch jvmMatch = jvmRe.match(detailsSection);
    if (jvmMatch.hasMatch()) {
        m_systemDetails["JVM Flags"] = jvmMatch.captured(1).trimmed();
    }

    //Minecraft 版本
    QRegularExpression mcRe("^\\s*Minecraft Version:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch mcMatch = mcRe.match(detailsSection);
    if (mcMatch.hasMatch()) {
        m_systemDetails["Minecraft Version"] = mcMatch.captured(1).trimmed();
    }

    //启动版本
    QRegularExpression launchRe("^\\s*Launched Version:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch launchMatch = launchRe.match(detailsSection);
    if (launchMatch.hasMatch()) {
        m_systemDetails["Launched Version"] = launchMatch.captured(1).trimmed();
    }

    //渲染 API
    QRegularExpression apiRe("^\\s*Backend API:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch apiMatch = apiRe.match(detailsSection);
    if (apiMatch.hasMatch()) {
        m_systemDetails["Backend API"] = apiMatch.captured(1).trimmed();
    }

    //显卡
    QRegularExpression gpuRe("^\\s*Graphics card #\\d+ name:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator gpuIt = gpuRe.globalMatch(detailsSection);
    while (gpuIt.hasNext()) {
        QRegularExpressionMatch gpuMatch = gpuIt.next();
        QString gpuName = gpuMatch.captured(1).trimmed();
        // 过滤掉虚拟显卡
        if (!gpuName.contains("Oray", Qt::CaseInsensitive)) {
            m_systemDetails["Graphics Card"] = gpuName;
            break;//只取第一个真实显卡
        }
    }

    //模组加载器
    QRegularExpression moddedRe("^\\s*Is Modded:\\s*(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch moddedMatch = moddedRe.match(detailsSection);
    if (moddedMatch.hasMatch()) {
        QString moddedInfo = moddedMatch.captured(1).trimmed();
        if (moddedInfo.contains("forge", Qt::CaseInsensitive)) {
            m_systemDetails["Mod Loader"] = "Forge";
        } else if (moddedInfo.contains("fabric", Qt::CaseInsensitive)) {
            m_systemDetails["Mod Loader"] = "Fabric";
        } else if (moddedInfo.contains("Definitely")) {
            m_systemDetails["Mod Loader"] = "Forge/Fabric (已检测到)";
        } else {
            m_systemDetails["Mod Loader"] = moddedInfo;
        }
    }

    qDebug() << "提取到" << m_systemDetails.size() << "条系统信息";
}