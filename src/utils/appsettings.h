#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>

class AppSettings
{
public:
    enum class DisasmBackend {
        Objdump = 0,
        Radare2 = 1,
    };

    static DisasmBackend disasmBackend();
    static void setDisasmBackend(DisasmBackend backend);

    static QString objdumpPath();
    static void setObjdumpPath(const QString &path);

    static QString radare2Path();
    static void setRadare2Path(const QString &path);

    enum class Radare2AnalysisLevel {
        None = 0,
        Aa   = 1,
        Aaa  = 2,
    };

    enum class AsmSyntax {
        Intel = 0,
        Att   = 1,
    };

    static int disasmInsnLimitPerSection();
    static void setDisasmInsnLimitPerSection(int limit);

    static Radare2AnalysisLevel radare2AnalysisLevel();
    static void setRadare2AnalysisLevel(Radare2AnalysisLevel lvl);

    static AsmSyntax asmSyntax();
    static void setAsmSyntax(AsmSyntax syntax);

    // Optional commands executed in r2 before JSON queries (semicolon-separated).
    static QString radare2PreCommands();
    static void setRadare2PreCommands(const QString &cmds);

    // --- AI Agent settings ---
    static QString llmApiKey();
    static void setLlmApiKey(const QString &key);

    static QString llmModel();
    static void setLlmModel(const QString &model);

    static QString llmSystemPrompt();
    static void setLlmSystemPrompt(const QString &prompt);

    static int llmMaxTokens();
    static void setLlmMaxTokens(int tokens);

    // Import/export settings to share with others (INI file).
    // NOTE: API key is intentionally excluded from export/import.
    static bool exportToIni(const QString &filePath, QString *error = nullptr);
    static bool importFromIni(const QString &filePath, QString *error = nullptr);

private:
    static QString keyDisasmBackend();
    static QString keyObjdumpPath();
    static QString keyRadare2Path();
    static QString keyInsnLimitPerSection();
    static QString keyRadare2AnalysisLevel();
    static QString keyAsmSyntax();
    static QString keyRadare2PreCommands();
    static QString keyLlmApiKey();
    static QString keyLlmModel();
    static QString keyLlmSystemPrompt();
    static QString keyLlmMaxTokens();
};

#endif // APPSETTINGS_H

