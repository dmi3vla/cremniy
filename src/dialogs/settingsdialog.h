#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QPlainTextEdit;
class QTabWidget;
class QWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onBrowseObjdump();
    void onBrowseRadare2();
    void onTestTools();
    void onExportIni();
    void onImportIni();
    void onAccept();
    void onBackendChanged(int index);
    void onActiveEndpointChanged(int index);
    void onAddEndpoint();
    void onEditEndpoint();
    void onDeleteEndpoint();
    void onSaveEndpointEdit();
    void onCancelEndpointEdit();
    void onToggleApiKeyVisibility();

private:
    void loadFromSettings();
    void updateUiEnabledState();
    void updateDependencyStatus();
    void refreshEndpointCombo();
    void showEndpointEditor(int index);
    void hideEndpointEditor();
    bool validateEndpointEditor() const;

    QComboBox   *m_backendCombo = nullptr;
    QLineEdit   *m_objdumpPath  = nullptr;
    QLineEdit   *m_radare2Path  = nullptr;
    QLabel      *m_objdumpStatus = nullptr;
    QLabel      *m_radare2Status = nullptr;
    QLabel      *m_fileStatus    = nullptr;

    // Disassembler options
    QSpinBox   *m_insnLimit = nullptr;
    QComboBox  *m_syntaxCombo = nullptr;

    // radare2 options
    QComboBox     *m_r2AnalysisCombo = nullptr;
    QPlainTextEdit *m_r2PreCommands = nullptr;

    QPushButton *m_testBtn      = nullptr;
    QPushButton *m_exportBtn    = nullptr;
    QPushButton *m_importBtn    = nullptr;
    QPushButton *m_okBtn        = nullptr;
    QPushButton *m_cancelBtn    = nullptr;

    // AI Agent endpoint settings
    QComboBox      *m_endpointCombo = nullptr;
    QPushButton    *m_addEndpointBtn = nullptr;
    QPushButton    *m_editEndpointBtn = nullptr;
    QPushButton    *m_deleteEndpointBtn = nullptr;

    QWidget        *m_endpointEditor = nullptr;
    QLineEdit      *m_endpointNameEdit = nullptr;
    QLineEdit      *m_endpointBaseUrlEdit = nullptr;
    QLineEdit      *m_endpointApiKeyEdit = nullptr;
    QPushButton    *m_endpointApiKeyToggleBtn = nullptr;
    QComboBox      *m_endpointModelCombo = nullptr;
    QComboBox      *m_endpointFormatCombo = nullptr;
    QLabel         *m_endpointValidationLabel = nullptr;
    QPushButton    *m_endpointSaveBtn = nullptr;
    QPushButton    *m_endpointCancelBtn = nullptr;
    int             m_editingEndpointIndex = -1;
    bool            m_refreshingEndpoints = false;

    QPlainTextEdit *m_llmSystemPrompt = nullptr;
    QSpinBox       *m_llmMaxTokens   = nullptr;

    QWidget* createDisassemblerPage();
    QWidget* createAgentPage();
};

#endif // SETTINGSDIALOG_H

