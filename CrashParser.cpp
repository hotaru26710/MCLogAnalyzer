#include "CrashParser.h"
#include <QDebug>
#include <QRegularExpression>
#include <QSet>

CrashParser::CrashParser()
    : m_hasCrash(false){
}

void CrashParser::parse(const QString &logContent){
    // 每次解析前清空旧数据
    m_errorType.clear();
    m_suggestion.clear();
    m_stackTrace.clear();
    m_systemDetails.clear();
    m_hasMainMod = false;
    m_mainModId.clear();
    m_mainModName.clear();
    m_mainModVersion.clear();
    m_mainModEvidence.clear();
    m_loadedMods.clear();
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
    parseModList(crashSection);
    identifyMainMod(crashSection);
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
    else if (section.contains("ClassCastException", Qt::CaseInsensitive)) {
        m_suggestion = "类转换异常！模组试图将物品实体转换为生物实体，可能是模组版本不兼容或调用方式错误。尝试更新 or 移除责任模组";
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
    else if (section.contains("ticking block entity", Qt::CaseInsensitive)
             || section.contains("Exception ticking block", Qt::CaseInsensitive)) {
        m_suggestion = "区块实体刻(Block Entity)循环异常！多半由某个模组的方块/实体逻辑引发，可尝试重进世界移除问题方块，"
                       "或更新/移除上面识别出的责任模组";
    }
    else if (section.contains("ticking entity", Qt::CaseInsensitive)) {
        m_suggestion = "实体刻(Tick)异常！通常是模组实体的 Update 抛错，尝试更新/移除上面识别出的责任模组，或重进世界";
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

// ---------- 模组列表 ----------
void CrashParser::parseModList(const QString &section)
{
    m_loadedMods.clear();

    // 取 "Mod List:" 之后的段落
    int listStart = section.indexOf("Mod List:");
    if (listStart == -1) {
        // 旧版格式可能是 "Mods:" 单独小节或纯文本行，跳过
        qDebug() << "未找到 Mod List 段";
        return;
    }
    QString listPart = section.mid(listStart + 9); // 跳过 "Mod List:"

    // 截到下一个 "--" 区块标题为止
    int nextSection = listPart.indexOf("\n-- ");
    if (nextSection != -1)
        listPart = listPart.left(nextSection);

    // 现代 Forge 行形如:
    //   +---|tconstruct |Tinkers' Construct |3.1.1.31 |DONE |Manifest: NONE |...
    //   或 |minecraft |Minecraft |1.16.5 |DONE |...
    QRegularExpression entryRe("^\\s*(?:\\+---)?\\|([^|]+)\\|([^|]+)\\|([^|]+)\\|", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = entryRe.globalMatch(listPart);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        ModEntry e;
        e.id = m.captured(1).trimmed();
        e.displayName = m.captured(2).trimmed();
        e.version = m.captured(3).trimmed();
        if (e.id.isEmpty() || e.displayName.isEmpty())
            continue;
        m_loadedMods.append(e);
    }

    // 若上面没匹配到，尝试旧版 Forge 行:
    //   mod_tconstruct : Tinkers' Construct (1.12.2-2.13.0.171)
    if (m_loadedMods.isEmpty()) {
        QRegularExpression oldRe("^\\s*mod_([A-Za-z0-9_]+)\\s*:\\s*(.+?)\\s*\\(([^)]+)\\)", QRegularExpression::MultilineOption);
        QRegularExpressionMatchIterator oldIt = oldRe.globalMatch(listPart);
        while (oldIt.hasNext()) {
            QRegularExpressionMatch m = oldIt.next();
            ModEntry e;
            e.id = m.captured(1).trimmed();
            e.displayName = m.captured(2).trimmed();
            e.version = m.captured(3).trimmed();
            m_loadedMods.append(e);
        }
    }

    qDebug() << "识别到" << m_loadedMods.size() << "个模组";
}

// ---------- 肇事模组识别 ----------
namespace {
// 属于平台/基础设施的类名前缀（这些栈帧绝不“属于”某个第三方模组）
bool isInfraClass(const QString &cls) {
    static const QSet<QString> prefixes = {
        "java.", "javax.", "sun.", "jdk.",
        "net.minecraft.", "net.minecraftforge.", "net.neoforged.",
        "cpw.mods.", "com.mojang.", "org.spongepowered.asm.",
        "org.lwjgl.", "io.netty.", "com.google.",
        "org.apache.", "it.unimi.dsi.fastutil.",
        "com.ibm.", "org.slf4j.", "org.apache.logging.",
    };
    for (const QString &p : prefixes)
        if (cls.startsWith(p))
            return true;
    return false;
}

// 平台 jar 关键字（文件名小写后包含它们即视为平台/引擎自身）
bool isPlatformJarName(const QString &jarLower) {
    static const QSet<QString> kws = {
        "minecraft", "forge", "fml", "neoforge", "fabric",
        "quilt", "launchwrapper", "mixin", "bundler",
        "guava", "gson", "netty", "commons-", "oshi", "jna",
        "authlib", "slf4j", "log4j", "lwjgl", "fastutil",
        "datafixer", "brigadier", "icu4j", "apache-",
        "okhttp", "okio", "jackson", "snakeyaml", "night-config",
        "paulscode", "asm-", "asm.", "caffeine", "gson",
    };
    for (const QString &k : kws)
        if (jarLower.contains(k))
            return true;
    return false;
}

// “路过型”事件监听/回调类 —— 它们常因订阅了事件而出现在堆栈里，
// 但不一定是真正发起异常行为的元凶（如 CreativeEvents、ForgeEventListener、XxxEventHandler）
bool isEventHandlerClass(const QString &cls) {
    static const QSet<QString> needles = {
        "eventhandler", "events", "listener", "listeners",
        "eventbus",
    };
    const QString lower = cls.toLower();
    for (const QString &n : needles)
        if (lower.contains(n))
            return true;
    return false;
}

// lambda$… 风格的匿名方法，常见于在事件回调里点起的一串帧，
// 自身通常也不是责任现场
bool isLambdaMethod(const QString &method) {
    return method.contains("lambda$");
}

// 从 "Entity Type: 命名空间:id (…) " 行取命名空间（即 modid）。
// 只在存在实体区块时才可能返回非空。
bool extractEntityModId(const QString &section, QString &outModid) {
    QRegularExpression etRe("Entity\\s+Type:\\s*([A-Za-z0-9_\\-]+):[A-Za-z0-9_.$]+",
                            QRegularExpression::MultilineOption);
    QRegularExpressionMatch m = etRe.match(section);
    if (m.hasMatch()) {
        outModid = m.captured(1).trimmed().toLower();
        return true;
    }
    return false;
}
}

void CrashParser::identifyMainMod(const QString &section)
{
    m_hasMainMod = false;
    m_mainModId.clear();
    m_mainModName.clear();
    m_mainModVersion.clear();
    m_mainModEvidence.clear();

    // 1) 错误描述本身点名的模组
    // 1a) Forge 1.12 加载崩溃标志性句式：
    //     net.minecraftforge.fml.common.LoaderExceptionModCrash: Caught exception from Tinkers Construct (tconstruct)
    QRegularExpression caughtRe("Caught exception from\\s+(.+?)\\s*\\(([A-Za-z0-9_\\-]+)\\)");
    QRegularExpressionMatch caughtMatch = caughtRe.match(section.left(6000));
    if (caughtMatch.hasMatch()) {
        m_mainModName = caughtMatch.captured(1).trimmed();
        m_mainModId = caughtMatch.captured(2).trimmed();
        m_hasMainMod = true;
        m_mainModEvidence = "Forge 加载异常信息直接点名了该模组";
        qDebug() << "通过 LoaderExceptionModCrash 定位模组:" << m_mainModName;
        return;
    }

    // 1b) 现代 Forge 模组加载失败句式：
    //     The mod X has failed to load / Mod resolution exception ...
    QRegularExpression explicitMod(
        "(?:The mod|mod)\\s+([A-Za-z0-9_\\-]+)\\s+(?:has|is|was|failed|caused|requires)",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch em = explicitMod.match(section.left(4000));
    if (em.hasMatch()) {
        m_mainModId = em.captured(1).trimmed();
        // 在列表里找显示名/版本
        for (const ModEntry &e : m_loadedMods) {
            if (e.id.compare(m_mainModId, Qt::CaseInsensitive) == 0) {
                m_mainModName = e.displayName;
                m_mainModVersion = e.version;
                break;
            }
        }
        if (m_mainModName.isEmpty())
            m_mainModName = m_mainModId;
        m_hasMainMod = true;
        m_mainModEvidence = "错误信息中直接点名了该模组";
        qDebug() << "通过错误信息定位模组:" << m_mainModName;
        return;
    }

    // 2) 栈帧来源 jar 反查（主方案）
    QRegularExpression frameRe(
        "^\\s*at\\s+([\\w.$]+(?:\\$[\\w]+)*)\\.([\\w$<>]+)\\s*\\(([^)]*)\\)\\s*~\\[([^\\]]+)\\]",
        QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = frameRe.globalMatch(section);

    struct FrameHit {
        QString cls;
        QString method;
        QString src;   // 原始 ~[...] 内容
        QString jar;   // 规范化 jar 名（小写，去掉版本号尾巴）
        QString jarBase; // 去掉版本尾巴的 jar 名（不含 .jar）
        int order = 0;
    };
    QList<FrameHit> candidates;

    int order = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        order++;
        QString cls = m.captured(1);
        QString src = m.captured(4).trimmed();

        // src 形如：
        //   mods/example.jar:1.0
        //   forge-1.16.5-36.2.34-universal.jar%2312!/:1.16.5        <- %<id>!/: 内部路径
        //   client-1.20.1-20230612.114412-srg.jar%23628!/:?
        // 注意：这里若按 "/" 切会把手例的 %…!/ 误当目录分隔、把 jar 文件名切没。
        // 因此：先取首个 ".jar" 为止，再取该节最后一段作文件名（含后缀），
        // 其后 %…!/、:?、路径全部丢弃。
        const int jarIdx = src.indexOf(".jar");
        if (jarIdx == -1)
            continue;
        QString pre = src.left(jarIdx + 4);                // 保留 ".jar"，去掉 %…!/:?
        const int slash = pre.lastIndexOf('/');
        QString jarName = (slash != -1) ? pre.mid(slash + 1) : pre;
        jarName = jarName.trimmed();
        if (jarName.isEmpty())
            continue;
        QString jarLower = jarName.toLower();

        FrameHit hit;
        hit.cls = cls;
        hit.method = m.captured(2);
        hit.src = src;
        QString jb = jarLower;
        if (jb.endsWith(".jar")) jb.chop(4);
        hit.jarBase = jb;
        hit.jar = jb; // 已去掉 .jar；若其后仍有版本尾巴由 idForJar 前缀匹配忽略
        hit.order = order;
        candidates.append(hit);
    }

    // 结束判定：把命中的帧落盘，`src` 作为证据，返回 true 表示已认定
    // 内联 lambda，捕获 this 以访问加载列表
    auto adopt = [&](const FrameHit &h, const QString &modidGuess, const QString &evidence) -> bool {
        m_mainModId = modidGuess;
        m_mainModName.clear();
        m_mainModVersion.clear();
        for (const ModEntry &e : m_loadedMods)
            if (e.id.compare(modidGuess, Qt::CaseInsensitive) == 0) {
                m_mainModName = e.displayName;
                m_mainModVersion = e.version;
                break;
            }
        if (m_mainModName.isEmpty())
            m_mainModName = modidGuess.isEmpty() ? h.jarBase : modidGuess;
        if (m_mainModName.isEmpty())
            return false;
        m_hasMainMod = true;
        m_mainModEvidence = evidence.isEmpty()
            ? (h.cls + "." + h.method + "(" + h.src + ")") : evidence;
        return true;
    };

    // jarBase 前缀匹配已加载模组 id（如 tconstruct-1.16.5-3.1.1.31.jar -> tconstruct），
    // 找不到就用 jar 名兜底。
    auto idForJar = [&](const QString &jb) -> QString {
        int bestLen = 0;
        QString best;
        for (const ModEntry &e : m_loadedMods) {
            const QString idLower = e.id.toLower();
            if (jb.startsWith(idLower) && idLower.size() > bestLen) {
                bestLen = idLower.size();
                best = e.id;
            }
        }
        return best.isEmpty() ? jb : best;
    };

    // ---- 候选帧（跳过平台帧）----
    QList<FrameHit> filtered;
    QList<FrameHit> eventShells;
    for (const FrameHit &hit : candidates) {
        if (isInfraClass(hit.cls))
            continue;
        if (isPlatformJarName(hit.jar))
            continue;
        bool shell = isEventHandlerClass(hit.cls) || isLambdaMethod(hit.method);
        if (shell) eventShells.append(hit);
        else filtered.append(hit);
    }

    // ---- 实体锚点（最强信号），如 alexsmobs:crocodile ---- //
    QString entityModId;
    if (extractEntityModId(section, entityModId)) {
        // 依次在 filtered 与 eventShells 里找 jar 或已加载 modid 匹配的帧
        QList<FrameHit> pool = filtered.isEmpty() ? eventShells : filtered;
        for (const FrameHit &hit : pool) {
            if (hit.jarBase.contains(entityModId)
                    || idForJar(hit.jarBase).compare(entityModId, Qt::CaseInsensitive) == 0) {
                if (adopt(hit, entityModId,
                          "实体类型锚点(Entity Type=" + entityModId + ")；"
                          + hit.cls + "." + hit.method))
                    qDebug() << "[实体锚点] 定位肇事模组:" << m_mainModName
                             << " Entity:" << entityModId;
                return;
            }
        }
    }

    // ---- 普通崩溃：取“最先执行”的非事件壳实作帧 ----
    // Java 栈从顶到底是调用者序列；最靠上（order 最小）的第三方实作帧最接近异常现场。
    const FrameHit *best = nullptr;
    for (const FrameHit &hit : filtered)
        if (best == nullptr || hit.order < best->order)
            best = &hit;

    if (best != nullptr) {
        // 若最靠上的第三方帧是一个对象/静态初始化场景（<init>/<clinit>），
        // 这类“构造函数链”上父类壳往往虚惊一场，真正责任常落在更下方
        // 发起具体构造/注册的模组。这里用“该段里出现次数最多的 jar 帧”来调整归属，
        // 更贴近“谁发起了这些动作”的人类直觉（例如 CreativeBlock 父壳 vs 发起者 create）。
        bool constCtor = best->method.contains("<init>")
                      || best->method.compare("<clinit>", Qt::CaseInsensitive) == 0;
        if (constCtor && filtered.size() > 1) {
            // 对象/静态初始化链：栈里先呈现的是被调用的父构造（super），
            // 其下（order 更大）紧跟的是发起调用的具体子类构造——真正责任方。
            // 例：CreativeBlock.<init>(父) <- ConnectedPillarBlock.<init>(子, create 发起)
            // 取“首个不同 jar 的下一个 <init> 帧”作为 blame。
            const FrameHit *caller = nullptr;
            for (const FrameHit &f : filtered) {
                if (f.order > best->order
                        && f.jarBase != best->jarBase
                        && f.method.contains("<init>")) {
                    caller = &f;
                    break;
                }
            }
            if (caller != nullptr
                && adopt(*caller, idForJar(caller->jarBase),
                         "构造链由子类「" + caller->jarBase + "」发起，"
                         "异常现场 " + best->cls + "." + best->method)) {
                qDebug() << "[构造链] 定位肇事模组:" << m_mainModName;
                return;
            }
        }
        if (adopt(*best, idForJar(best->jarBase), QString()))
            qDebug() << "定位肇事模组:" << m_mainModName
                     << "证据:" << m_mainModEvidence;
        return;
    }

    // ---- 兜底：只剩事件壳帧时，接受最上面的壳（写明证据性质较弱）----
    if (!eventShells.isEmpty()) {
        const FrameHit *s = &eventShells.first();
        for (const FrameHit &hit : eventShells)
            if (hit.order < s->order) s = &hit;
        if (adopt(*s, idForJar(s->jarBase), QString()))
            qDebug() << "定位肇事模组(事件壳兜底):" << m_mainModName;
        return;
    }

    // 3a) 最终保底：报告自带 "Suspected Mod(s)" 字段。
    //     当上面堆栈 / 实体锚点都没有可靠结论时，官方(或整合包作者)点名的嫌疑模组
    //     是最可靠的人工/半官方结论，应作为最后一道防线启用。
    //     仅当值为 NONE/空（明确“无嫌疑”）时不采用。
    {
        // 主流 Forge/NeoForge 崩溃报告格局的 Suspected Mods 是“同行的点名值”，
        // 如 “Suspected Mods: tconstruct” 或 “Suspected Mods: NONE / - / (空)”。
        // 为空 / NONE / - 都表示“无嫌疑”，此时不作保底结论。
        QString firstTok;
        QRegularExpression inlineRe("Suspected\\s+Mods?[\\w\\s,]*:\\s*([^\\r\\n]*)",
                                    QRegularExpression::MultilineOption);
        QRegularExpressionMatch inlineM = inlineRe.match(section);
        QString raw = inlineM.hasMatch() ? inlineM.captured(1).trimmed() : QString();
        const bool noney = raw.isEmpty()
                           || raw.compare("NONE", Qt::CaseInsensitive) == 0
                           || raw.compare("-", Qt::CaseInsensitive) == 0
                           || raw.compare("(none)", Qt::CaseInsensitive) == 0;
        if (!noney) {
            firstTok = raw.section(QRegularExpression("[,\\s\\[\\]]"), 0, 0).trimmed();
            firstTok.remove(QRegularExpression("[^A-Za-z0-9_\\-.@]"));
        }

        if (!firstTok.isEmpty()) {
            // 在加载列表里尽量反查显示名/版本
            QString foundName, foundVer, foundId;
            for (const ModEntry &e : m_loadedMods) {
                const bool idMatch =
                    e.id.compare(firstTok, Qt::CaseInsensitive) == 0
                    || firstTok.startsWith(e.id + "-", Qt::CaseInsensitive);
                const bool nameMatch =
                    !e.displayName.isEmpty()
                    && e.displayName.compare(firstTok, Qt::CaseInsensitive) == 0;
                if (idMatch || nameMatch) {
                    foundId = e.id;
                    foundName = e.displayName;
                    foundVer = e.version;
                    if (idMatch) break;
                }
            }
            m_mainModId = foundId.isEmpty() ? firstTok : foundId;
            m_mainModName = foundName.isEmpty()
                    ? (foundId.isEmpty() ? firstTok : foundId) : foundName;
            m_mainModVersion = foundVer;
            m_hasMainMod = true;
            m_mainModEvidence = "崩溃报告自带 Suspected Mods 字段点名: \"" + firstTok + "\"";
            qDebug() << "[Suspected] 保底定位模组:" << m_mainModName;
            return;
        }
    }

    // 4) 确实连官方嫌疑都没有：说明是原版或引擎自身问题
    qDebug() << "未定位到任何第三方模组(含 Suspected)，判原版/引擎问题";
}
