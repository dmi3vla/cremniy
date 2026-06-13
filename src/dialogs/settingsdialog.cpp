#include "settingsdialog.h"

#include "Agent/endpoint_manager.h"
#include "utils/appsettings.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

static QString resolvedExecutable(const QString &userPath, const QString &exeName)
{
    if (!userPath.trimmed().isEmpty())
        return userPath.trimmed();
    return QStandardPaths::findExecutable(exeName);
}

static bool isRunnableExecutable(const QString &path)
{
    if (path.trimmed().isEmpty())
        return false;
    const QFileInfo fi(path.trimmed());
    return fi.exists() && fi.isFile() && fi.isExecutable();
}

static void setStatusLabel(QLabel *lbl, bool ok, const QString &text)
{
    lbl->setText(ok ? QStringLiteral("✓ ") + text : QStringLiteral("✗ ") + text);
    lbl->setStyleSheet(ok ? "color: #39d353;" : "color: #f85149;");
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setModal(true);
    setMinimumSize(820, 620);
    setSizeGripEnabled(true);

    auto *root = new QVBoxLayout(this);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(createDisassemblerPage(), tr("Disassembler"));
    tabs->addTab(createAgentPage(), tr("AI Assistant"));
    root->addWidget(tabs, 1);

    auto *btnRow = new QHBoxLayout();
    m_testBtn = new QPushButton(tr("Test"), this);
    btnRow->addWidget(m_testBtn);
    m_importBtn = new QPushButton(tr("Import…"), this);
    m_exportBtn = new QPushButton(tr("Export…"), this);
    btnRow->addWidget(m_importBtn);
    btnRow->addWidget(m_exportBtn);
    btnRow->addStretch(1);
    m_okBtn = new QPushButton(tr("OK"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_okBtn->setDefault(true);
    btnRow->addWidget(m_okBtn);
    btnRow->addWidget(m_cancelBtn);
    root->addLayout(btnRow);

    connect(m_testBtn,   &QPushButton::clicked, this, &SettingsDialog::onTestTools);
    connect(m_exportBtn, &QPushButton::clicked, this, &SettingsDialog::onExportIni);
    connect(m_importBtn, &QPushButton::clicked, this, &SettingsDialog::onImportIni);
    connect(m_okBtn,     &QPushButton::clicked, this, &SettingsDialog::onAccept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onBackendChanged);
    connect(m_insnLimit, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::updateDependencyStatus);
    connect(m_syntaxCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::updateDependencyStatus);
    connect(m_r2AnalysisCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::updateDependencyStatus);
    connect(m_r2PreCommands, &QPlainTextEdit::textChanged, this, &SettingsDialog::updateDependencyStatus);

    loadFromSettings();
    updateUiEnabledState();
    updateDependencyStatus();
}

QWidget* SettingsDialog::createDisassemblerPage()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_backendCombo = new QComboBox(page);
    m_backendCombo->addItem(tr("objdump"),  static_cast<int>(AppSettings::DisasmBackend::Objdump));
    m_backendCombo->addItem(tr("radare2"),  static_cast<int>(AppSettings::DisasmBackend::Radare2));
    form->addRow(tr("Disassembler backend"), m_backendCombo);

    m_insnLimit = new QSpinBox(page);
    m_insnLimit->setRange(50, 200000);
    m_insnLimit->setSingleStep(250);
    m_insnLimit->setToolTip(tr("Maximum number of instructions per section (keeps UI responsive)"));
    form->addRow(tr("Instruction limit/section"), m_insnLimit);

    m_syntaxCombo = new QComboBox(page);
    m_syntaxCombo->addItem(tr("Intel"), static_cast<int>(AppSettings::AsmSyntax::Intel));
    m_syntaxCombo->addItem(tr("AT&T"),  static_cast<int>(AppSettings::AsmSyntax::Att));
    form->addRow(tr("Assembly syntax"), m_syntaxCombo);

    auto *objdumpRow = new QWidget(page);
    auto *objdumpLayout = new QHBoxLayout(objdumpRow);
    objdumpLayout->setContentsMargins(0, 0, 0, 0);
    m_objdumpPath = new QLineEdit(objdumpRow);
    m_objdumpPath->setPlaceholderText(tr("Leave empty to use PATH lookup"));
    m_objdumpStatus = new QLabel(objdumpRow);
    m_objdumpStatus->setMinimumWidth(150);
    m_objdumpStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *browseObjdump = new QPushButton(tr("Browse…"), objdumpRow);
    browseObjdump->setFixedWidth(90);
    objdumpLayout->addWidget(m_objdumpPath, 1);
    objdumpLayout->addWidget(m_objdumpStatus);
    objdumpLayout->addWidget(browseObjdump);
    form->addRow(tr("objdump path"), objdumpRow);
    connect(browseObjdump, &QPushButton::clicked, this, &SettingsDialog::onBrowseObjdump);
    connect(m_objdumpPath, &QLineEdit::textChanged, this, &SettingsDialog::updateDependencyStatus);

    auto *r2Row = new QWidget(page);
    auto *r2Layout = new QHBoxLayout(r2Row);
    r2Layout->setContentsMargins(0, 0, 0, 0);
    m_radare2Path = new QLineEdit(r2Row);
    m_radare2Path->setPlaceholderText(tr("Path to r2 (radare2) executable"));
    m_radare2Status = new QLabel(r2Row);
    m_radare2Status->setMinimumWidth(150);
    m_radare2Status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *browseR2 = new QPushButton(tr("Browse…"), r2Row);
    browseR2->setFixedWidth(90);
    r2Layout->addWidget(m_radare2Path, 1);
    r2Layout->addWidget(m_radare2Status);
    r2Layout->addWidget(browseR2);
    form->addRow(tr("radare2 path"), r2Row);
    connect(browseR2, &QPushButton::clicked, this, &SettingsDialog::onBrowseRadare2);
    connect(m_radare2Path, &QLineEdit::textChanged, this, &SettingsDialog::updateDependencyStatus);

    m_fileStatus = new QLabel(page);
    m_fileStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Dependency: file(1)"), m_fileStatus);

    m_r2AnalysisCombo = new QComboBox(page);
    m_r2AnalysisCombo->addItem(tr("None (fast)"), static_cast<int>(AppSettings::Radare2AnalysisLevel::None));
    m_r2AnalysisCombo->addItem(tr("aa (basic)"),  static_cast<int>(AppSettings::Radare2AnalysisLevel::Aa));
    m_r2AnalysisCombo->addItem(tr("aaa (full)"),  static_cast<int>(AppSettings::Radare2AnalysisLevel::Aaa));
    form->addRow(tr("radare2 analysis"), m_r2AnalysisCombo);

    m_r2PreCommands = new QPlainTextEdit(page);
    m_r2PreCommands->setPlaceholderText(tr("Optional r2 commands before JSON queries (one per line). Example:\n"
                                           "e asm.syntax=intel\n"
                                           "e asm.bits=64"));
    m_r2PreCommands->setFixedHeight(90);
    form->addRow(tr("radare2 pre-commands"), m_r2PreCommands);

    return page;
}

QWidget* SettingsDialog::createAgentPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *activeRow = new QHBoxLayout();
    activeRow->addWidget(new QLabel(tr("Active Endpoint:"), page));
    m_endpointCombo = new QComboBox(page);
    activeRow->addWidget(m_endpointCombo, 1);
    m_addEndpointBtn = new QPushButton(tr("+ Add"), page);
    m_editEndpointBtn = new QPushButton(tr("Edit"), page);
    m_deleteEndpointBtn = new QPushButton(tr("Delete"), page);
    activeRow->addWidget(m_addEndpointBtn);
    activeRow->addWidget(m_editEndpointBtn);
    activeRow->addWidget(m_deleteEndpointBtn);
    layout->addLayout(activeRow);

    auto *editorGroup = new QGroupBox(tr("Endpoint"), page);
    auto *editorForm = new QFormLayout(editorGroup);
    editorForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_endpointEditor = editorGroup;

    m_endpointNameEdit = new QLineEdit(editorGroup);
    editorForm->addRow(tr("Name"), m_endpointNameEdit);

    m_endpointBaseUrlEdit = new QLineEdit(editorGroup);
    m_endpointBaseUrlEdit->setPlaceholderText(tr("http://localhost:20128/v1"));
    editorForm->addRow(tr("Base URL"), m_endpointBaseUrlEdit);

    auto *keyRow = new QWidget(editorGroup);
    auto *keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    m_endpointApiKeyEdit = new QLineEdit(keyRow);
    m_endpointApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_endpointApiKeyEdit->setPlaceholderText(tr("Optional for local gateways"));
    m_endpointApiKeyToggleBtn = new QPushButton(tr("👁"), keyRow);
    m_endpointApiKeyToggleBtn->setFixedWidth(42);
    keyLayout->addWidget(m_endpointApiKeyEdit, 1);
    keyLayout->addWidget(m_endpointApiKeyToggleBtn);
    editorForm->addRow(tr("API Key"), keyRow);

    m_endpointModelCombo = new QComboBox(editorGroup);
    m_endpointModelCombo->setEditable(true);
    m_endpointModelCombo->addItems({
        QStringLiteral("claude-sonnet-4-6"),
        QStringLiteral("claude-sonnet-4-20250514"),
        QStringLiteral("claude-opus-4-20250514"),
        QStringLiteral("claude-3-5-haiku-20241022"),
        QStringLiteral("gpt-4o"),
        QStringLiteral("gpt-4o-mini"),
        QStringLiteral("llama3.1")
    });
    editorForm->addRow(tr("Model"), m_endpointModelCombo);

    m_endpointFormatCombo = new QComboBox(editorGroup);
    m_endpointFormatCombo->addItem(tr("Anthropic"), static_cast<int>(ApiEndpoint::ApiFormat::AnthropicMessages));
    m_endpointFormatCombo->addItem(tr("OpenAI-compatible"), static_cast<int>(ApiEndpoint::ApiFormat::OpenAIChatCompletions));
    editorForm->addRow(tr("API Format"), m_endpointFormatCombo);

    m_endpointValidationLabel = new QLabel(editorGroup);
    m_endpointValidationLabel->setStyleSheet("color: #f85149;");
    m_endpointValidationLabel->setWordWrap(true);
    editorForm->addRow(m_endpointValidationLabel);

    auto *editButtons = new QHBoxLayout();
    editButtons->addStretch(1);
    m_endpointSaveBtn = new QPushButton(tr("Save"), editorGroup);
    m_endpointCancelBtn = new QPushButton(tr("Cancel"), editorGroup);
    editButtons->addWidget(m_endpointSaveBtn);
    editButtons->addWidget(m_endpointCancelBtn);
    editorForm->addRow(editButtons);
    layout->addWidget(editorGroup);

    auto *settingsForm = new QFormLayout();
    m_llmMaxTokens = new QSpinBox(page);
    m_llmMaxTokens->setRange(256, 128000);
    m_llmMaxTokens->setSingleStep(512);
    m_llmMaxTokens->setToolTip(tr("Maximum number of tokens in the response."));
    settingsForm->addRow(tr("Max tokens"), m_llmMaxTokens);

    m_llmSystemPrompt = new QPlainTextEdit(page);
    m_llmSystemPrompt->setPlaceholderText(tr("System prompt for the AI assistant..."));
    m_llmSystemPrompt->setMinimumHeight(120);
    settingsForm->addRow(tr("System prompt"), m_llmSystemPrompt);
    layout->addLayout(settingsForm);

    auto *infoLabel = new QLabel(
        tr("API keys are stored in a separate local secrets.ini file under the user config directory and are not exported."),
        page);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #b08030; font-size: 11px; margin-top: 8px;");
    layout->addWidget(infoLabel);
    layout->addStretch(1);

    connect(m_endpointCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onActiveEndpointChanged);
    connect(m_addEndpointBtn, &QPushButton::clicked, this, &SettingsDialog::onAddEndpoint);
    connect(m_editEndpointBtn, &QPushButton::clicked, this, &SettingsDialog::onEditEndpoint);
    connect(m_deleteEndpointBtn, &QPushButton::clicked, this, &SettingsDialog::onDeleteEndpoint);
    connect(m_endpointSaveBtn, &QPushButton::clicked, this, &SettingsDialog::onSaveEndpointEdit);
    connect(m_endpointCancelBtn, &QPushButton::clicked, this, &SettingsDialog::onCancelEndpointEdit);
    connect(m_endpointApiKeyToggleBtn, &QPushButton::clicked, this, &SettingsDialog::onToggleApiKeyVisibility);

    hideEndpointEditor();
    return page;
}

void SettingsDialog::onExportIni()
{
    const QString file = QFileDialog::getSaveFileName(
        this,
        tr("Export settings"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/cremniy-settings.ini",
        tr("INI files (*.ini)"));
    if (file.isEmpty()) return;

    QString err;
    if (!AppSettings::exportToIni(file, &err)) {
        QMessageBox::warning(this, tr("Export failed"), err.isEmpty() ? tr("Failed to export settings") : err);
        return;
    }
    QMessageBox::information(this, tr("Export"), tr("Settings exported to:\n%1").arg(file));
}

void SettingsDialog::onImportIni()
{
    const QString file = QFileDialog::getOpenFileName(
        this,
        tr("Import settings"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("INI files (*.ini)"));
    if (file.isEmpty()) return;

    QString err;
    if (!AppSettings::importFromIni(file, &err)) {
        QMessageBox::warning(this, tr("Import failed"), err.isEmpty() ? tr("Failed to import settings") : err);
        return;
    }

    loadFromSettings();
    updateUiEnabledState();
    updateDependencyStatus();
    QMessageBox::information(this, tr("Import"), tr("Settings imported from:\n%1").arg(file));
}

void SettingsDialog::loadFromSettings()
{
    const auto backend = AppSettings::disasmBackend();
    const int want = static_cast<int>(backend);
    int idx = m_backendCombo->findData(want);
    if (idx < 0) idx = 0;
    m_backendCombo->setCurrentIndex(idx);

    m_objdumpPath->setText(AppSettings::objdumpPath());
    m_radare2Path->setText(AppSettings::radare2Path());
    m_insnLimit->setValue(AppSettings::disasmInsnLimitPerSection());

    const int syntaxIdx = m_syntaxCombo->findData(static_cast<int>(AppSettings::asmSyntax()));
    m_syntaxCombo->setCurrentIndex(syntaxIdx < 0 ? 0 : syntaxIdx);

    const int r2Idx = m_r2AnalysisCombo->findData(static_cast<int>(AppSettings::radare2AnalysisLevel()));
    m_r2AnalysisCombo->setCurrentIndex(r2Idx < 0 ? 0 : r2Idx);

    m_r2PreCommands->setPlainText(AppSettings::radare2PreCommands().replace(';', '\n'));

    EndpointManager::instance().loadFromSettings();
    refreshEndpointCombo();

    m_llmMaxTokens->setValue(AppSettings::llmMaxTokens());
    m_llmSystemPrompt->setPlainText(AppSettings::llmSystemPrompt());
}

void SettingsDialog::updateUiEnabledState()
{
    const bool useRadare2 =
        (m_backendCombo->currentData().toInt() == static_cast<int>(AppSettings::DisasmBackend::Radare2));

    m_radare2Path->setEnabled(true);
    m_objdumpPath->setEnabled(true);
    m_radare2Path->setToolTip(useRadare2 ? tr("Active backend") : tr("Inactive backend (still configurable)"));
    m_objdumpPath->setToolTip(useRadare2 ? tr("Inactive backend (still configurable)") : tr("Active backend"));
    m_r2AnalysisCombo->setEnabled(useRadare2);
    m_r2PreCommands->setEnabled(useRadare2);
}

void SettingsDialog::refreshEndpointCombo()
{
    m_refreshingEndpoints = true;
    m_endpointCombo->clear();

    const auto endpoints = EndpointManager::instance().endpoints();
    for (int i = 0; i < endpoints.size(); ++i) {
        m_endpointCombo->addItem(endpoints[i].name, i);
    }

    const int active = EndpointManager::instance().activeEndpointIndex();
    if (active >= 0 && active < m_endpointCombo->count()) {
        m_endpointCombo->setCurrentIndex(active);
    }

    const bool hasEndpoints = !endpoints.isEmpty();
    m_editEndpointBtn->setEnabled(hasEndpoints);
    m_deleteEndpointBtn->setEnabled(hasEndpoints && endpoints.size() > 1);
    m_refreshingEndpoints = false;
}

void SettingsDialog::showEndpointEditor(int index)
{
    m_editingEndpointIndex = index;
    m_endpointValidationLabel->clear();
    m_endpointApiKeyEdit->setEchoMode(QLineEdit::Password);

    ApiEndpoint ep;
    if (index >= 0) {
        const auto endpoints = EndpointManager::instance().endpoints();
        if (index < endpoints.size()) {
            ep = endpoints[index];
        }
    } else {
        ep.name = tr("New Endpoint");
        ep.baseUrl = QStringLiteral("http://localhost:20128/v1");
        ep.format = ApiEndpoint::ApiFormat::OpenAIChatCompletions;
    }

    m_endpointNameEdit->setText(ep.name);
    m_endpointBaseUrlEdit->setText(ep.baseUrl);
    m_endpointApiKeyEdit->setText(ep.apiKey);
    m_endpointModelCombo->setEditText(ep.defaultModel);
    const int formatIdx = m_endpointFormatCombo->findData(static_cast<int>(ep.format));
    m_endpointFormatCombo->setCurrentIndex(formatIdx < 0 ? 0 : formatIdx);
    m_endpointEditor->setVisible(true);
}

void SettingsDialog::hideEndpointEditor()
{
    m_editingEndpointIndex = -1;
    if (m_endpointEditor) {
        m_endpointEditor->setVisible(false);
    }
}

bool SettingsDialog::validateEndpointEditor() const
{
    const QString baseUrl = m_endpointBaseUrlEdit->text().trimmed();
    if (m_endpointNameEdit->text().trimmed().isEmpty()) {
        m_endpointValidationLabel->setText(tr("Name must not be empty."));
        return false;
    }
    if (baseUrl.isEmpty()) {
        m_endpointValidationLabel->setText(tr("Base URL must not be empty."));
        return false;
    }
    if (!baseUrl.startsWith(QStringLiteral("http://")) &&
        !baseUrl.startsWith(QStringLiteral("https://"))) {
        m_endpointValidationLabel->setText(tr("Base URL must start with http:// or https://."));
        return false;
    }
    m_endpointValidationLabel->clear();
    return true;
}

void SettingsDialog::onBrowseObjdump()
{
    const QString cur = m_objdumpPath->text().trimmed();
    const QString file = QFileDialog::getOpenFileName(this, tr("Select objdump executable"), cur);
    if (!file.isEmpty())
        m_objdumpPath->setText(file);
}

void SettingsDialog::onBrowseRadare2()
{
    const QString cur = m_radare2Path->text().trimmed();
    const QString file = QFileDialog::getOpenFileName(this, tr("Select radare2 (r2) executable"), cur);
    if (!file.isEmpty())
        m_radare2Path->setText(file);
}

static bool runVersionCheck(const QString &exe, const QStringList &args, QString *out, QString *err)
{
    QProcess p;
    p.start(exe, args);
    if (!p.waitForStarted(2000))
        return false;
    if (!p.waitForFinished(4000))
        return false;
    if (out) *out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (err) *err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

void SettingsDialog::onTestTools()
{
    const QString objdumpExe = resolvedExecutable(m_objdumpPath->text(), "objdump");
    const QString r2Exe      = resolvedExecutable(m_radare2Path->text(), "r2");
    QStringList lines;

    QString out, err;
    const bool objdumpOk = !objdumpExe.isEmpty() && runVersionCheck(objdumpExe, {"--version"}, &out, &err);
    lines << (objdumpOk ? tr("objdump: OK (%1)").arg(objdumpExe)
                        : tr("objdump: FAIL (%1)").arg(objdumpExe.isEmpty() ? tr("not found") : objdumpExe));
    if (!objdumpOk && !err.isEmpty()) lines << "  " + err;

    out.clear();
    err.clear();
    const bool r2Ok = !r2Exe.isEmpty() && runVersionCheck(r2Exe, {"-v"}, &out, &err);
    lines << (r2Ok ? tr("radare2: OK (%1)").arg(r2Exe)
                   : tr("radare2: FAIL (%1)").arg(r2Exe.isEmpty() ? tr("not found") : r2Exe));
    if (r2Ok && !out.isEmpty()) lines << "  " + out.split('\n').value(0);
    if (!r2Ok && !err.isEmpty()) lines << "  " + err;

    QMessageBox::information(this, tr("Tool check"), lines.join('\n'));
    updateDependencyStatus();
}

void SettingsDialog::onAccept()
{
    const int backendInt = m_backendCombo->currentData().toInt();
    const auto backend = (backendInt == static_cast<int>(AppSettings::DisasmBackend::Radare2))
        ? AppSettings::DisasmBackend::Radare2
        : AppSettings::DisasmBackend::Objdump;

    AppSettings::setDisasmBackend(backend);
    AppSettings::setObjdumpPath(m_objdumpPath->text());
    AppSettings::setRadare2Path(m_radare2Path->text());
    AppSettings::setDisasmInsnLimitPerSection(m_insnLimit->value());
    AppSettings::setAsmSyntax(static_cast<AppSettings::AsmSyntax>(m_syntaxCombo->currentData().toInt()));
    AppSettings::setRadare2AnalysisLevel(static_cast<AppSettings::Radare2AnalysisLevel>(m_r2AnalysisCombo->currentData().toInt()));

    const QString pre = m_r2PreCommands->toPlainText()
                            .split('\n', Qt::SkipEmptyParts)
                            .join(';');
    AppSettings::setRadare2PreCommands(pre);

    AppSettings::setLlmMaxTokens(m_llmMaxTokens->value());
    AppSettings::setLlmSystemPrompt(m_llmSystemPrompt->toPlainText());
    EndpointManager::instance().saveToSettings();

    accept();
}

void SettingsDialog::onBackendChanged(int)
{
    updateUiEnabledState();
    updateDependencyStatus();
}

void SettingsDialog::onActiveEndpointChanged(int index)
{
    if (m_refreshingEndpoints || index < 0) {
        return;
    }
    EndpointManager::instance().setActive(index);
}

void SettingsDialog::onAddEndpoint()
{
    showEndpointEditor(-1);
}

void SettingsDialog::onEditEndpoint()
{
    showEndpointEditor(m_endpointCombo->currentIndex());
}

void SettingsDialog::onDeleteEndpoint()
{
    const int index = m_endpointCombo->currentIndex();
    if (index < 0) return;

    const auto endpoints = EndpointManager::instance().endpoints();
    if (endpoints.size() <= 1) {
        QMessageBox::warning(this, tr("Delete endpoint"), tr("At least one endpoint must remain."));
        return;
    }

    if (QMessageBox::question(this, tr("Delete endpoint"),
                              tr("Delete endpoint '%1'?").arg(endpoints[index].name)) != QMessageBox::Yes) {
        return;
    }

    EndpointManager::instance().removeEndpoint(index);
    refreshEndpointCombo();
    hideEndpointEditor();
}

void SettingsDialog::onSaveEndpointEdit()
{
    if (!validateEndpointEditor()) {
        return;
    }

    ApiEndpoint ep;
    ep.name = m_endpointNameEdit->text().trimmed();
    ep.baseUrl = m_endpointBaseUrlEdit->text().trimmed();
    ep.apiKey = m_endpointApiKeyEdit->text().trimmed();
    ep.defaultModel = m_endpointModelCombo->currentText().trimmed();
    ep.format = static_cast<ApiEndpoint::ApiFormat>(m_endpointFormatCombo->currentData().toInt());

    if (m_editingEndpointIndex >= 0) {
        EndpointManager::instance().updateEndpoint(m_editingEndpointIndex, ep);
        EndpointManager::instance().setActive(m_editingEndpointIndex);
    } else {
        EndpointManager::instance().addEndpoint(ep);
        EndpointManager::instance().setActive(EndpointManager::instance().endpoints().size() - 1);
    }

    refreshEndpointCombo();
    hideEndpointEditor();
}

void SettingsDialog::onCancelEndpointEdit()
{
    hideEndpointEditor();
}

void SettingsDialog::onToggleApiKeyVisibility()
{
    const bool password = m_endpointApiKeyEdit->echoMode() == QLineEdit::Password;
    m_endpointApiKeyEdit->setEchoMode(password ? QLineEdit::Normal : QLineEdit::Password);
}

void SettingsDialog::updateDependencyStatus()
{
    const QString resolvedObjdump = resolvedExecutable(m_objdumpPath->text(), "objdump");
    const bool objdumpOk = isRunnableExecutable(resolvedObjdump);
    setStatusLabel(m_objdumpStatus, objdumpOk, objdumpOk ? tr("found") : tr("missing"));
    m_objdumpStatus->setToolTip(objdumpOk ? resolvedObjdump : tr("Not found in PATH and no valid path set"));

    const QString resolvedR2 = resolvedExecutable(m_radare2Path->text(), "r2");
    const bool r2Ok = isRunnableExecutable(resolvedR2);
    setStatusLabel(m_radare2Status, r2Ok, r2Ok ? tr("found") : tr("missing"));
    m_radare2Status->setToolTip(r2Ok ? resolvedR2 : tr("Not found in PATH and no valid path set"));

    const QString fileExe = QStandardPaths::findExecutable("file");
    const bool fileOk = isRunnableExecutable(fileExe);
    setStatusLabel(m_fileStatus, fileOk, fileOk ? tr("found") : tr("missing"));
    m_fileStatus->setToolTip(fileOk ? fileExe : tr("The objdump backend uses 'file -b <path>' for arch detection"));
}
