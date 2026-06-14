#include "digest_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QGraphicsOpacityEffect>

DigestPanel::DigestPanel(QWidget* parent)
    : QWidget(parent)
    , m_currentLineNumber(0)
{
    setupUI();
    setStyleSheet(R"(
        DigestPanel {
            background-color: #2a2a35;
            border-radius: 8px;
        }
        QLabel {
            color: #ffffff;
            font-size: 11px;
        }
        QTextEdit {
            background-color: #1e1e28;
            color: #c0c0d0;
            border: 1px solid #3a3a45;
            border-radius: 4px;
            padding: 8px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 10px;
        }
        QToolButton {
            background-color: #3a3a45;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 10px;
        }
        QToolButton:hover {
            background-color: #4a4a55;
        }
        QPushButton {
            background-color: #4a7ab0;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 10px;
        }
        QPushButton:hover {
            background-color: #5a8ac0;
        }
        QPushButton:pressed {
            background-color: #3a6a90;
        }
    )");
}

void DigestPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // Motivation section
    m_motivationLabel = new QLabel("Motivation");
    m_motivationLabel->setStyleSheet("font-weight: bold; color: #7ab0ff;");
    
    m_motivationContent = new QTextEdit();
    m_motivationContent->setMaximumHeight(100);
    m_motivationContent->setReadOnly(true);
    
    m_motivationToggle = new QToolButton();
    m_motivationToggle->setText("▼");
    m_motivationToggle->setMaximumWidth(30);
    
    createCollapsibleSection("Motivation", m_motivationContent, m_motivationToggle, m_motivationContainer);
    mainLayout->addWidget(m_motivationContainer);

    // Details section
    m_detailsLabel = new QLabel("Details");
    m_detailsLabel->setStyleSheet("font-weight: bold; color: #7ab0ff;");
    
    m_detailsContent = new QTextEdit();
    m_detailsContent->setMaximumHeight(150);
    m_detailsContent->setReadOnly(true);
    
    m_detailsToggle = new QToolButton();
    m_detailsToggle->setText("▼");
    m_detailsToggle->setMaximumWidth(30);
    
    createCollapsibleSection("Details", m_detailsContent, m_detailsToggle, m_detailsContainer);
    mainLayout->addWidget(m_detailsContainer);

    // Code snippet section
    m_codeSnippetLabel = new QLabel("Code Snippet");
    m_codeSnippetLabel->setStyleSheet("font-weight: bold; color: #7ab0ff;");
    
    m_codeSnippetContent = new QTextEdit();
    m_codeSnippetContent->setMaximumHeight(120);
    m_codeSnippetContent->setReadOnly(true);
    m_codeSnippetContent->setStyleSheet("font-family: 'Consolas', 'Monaco', monospace; font-size: 9px;");
    
    mainLayout->addWidget(m_codeSnippetLabel);
    mainLayout->addWidget(m_codeSnippetContent);

    // Go to button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_goToButton = new QPushButton("Перейти");
    m_goToButton->setMaximumWidth(80);
    buttonLayout->addWidget(m_goToButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_motivationToggle, &QToolButton::clicked, this, &DigestPanel::onMotivationToggle);
    connect(m_detailsToggle, &QToolButton::clicked, this, &DigestPanel::onDetailsToggle);
    connect(m_goToButton, &QPushButton::clicked, this, &DigestPanel::onGoToClicked);
}

void DigestPanel::createCollapsibleSection(const QString& title, QWidget* content, QToolButton*& toggleBtn, QWidget*& container)
{
    QVBoxLayout* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(4);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-weight: bold; color: #7ab0ff;");
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(toggleBtn);
    
    containerLayout->addLayout(headerLayout);
    containerLayout->addWidget(content);
}

void DigestPanel::setMotivation(const QString& motivation)
{
    m_motivationContent->setPlainText(motivation);
}

void DigestPanel::setDetails(const QString& details)
{
    m_detailsContent->setPlainText(details);
}

void DigestPanel::setCodeSnippet(const QString& snippet, const QString& filePath, int lineNumber)
{
    m_codeSnippetContent->setPlainText(snippet);
    m_currentFilePath = filePath;
    m_currentLineNumber = lineNumber;
}

void DigestPanel::playAppearAnimation(int durationMs)
{
    if (m_appearGroup) {
        m_appearGroup->stop();
        delete m_appearGroup;
    }

    m_panelOpacity = 0.0;

    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    effect->setOpacity(0.0);

    m_appearGroup = new QParallelAnimationGroup(this);

    QPropertyAnimation* opacityAnim = new QPropertyAnimation(effect, "opacity");
    opacityAnim->setDuration(durationMs);
    opacityAnim->setStartValue(0.0);
    opacityAnim->setEndValue(1.0);
    opacityAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_appearGroup->addAnimation(opacityAnim);
    m_appearGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void DigestPanel::playDisappearAnimation(int durationMs)
{
    if (m_disappearGroup) {
        m_disappearGroup->stop();
        delete m_disappearGroup;
    }

    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    effect->setOpacity(1.0);

    m_disappearGroup = new QParallelAnimationGroup(this);

    QPropertyAnimation* opacityAnim = new QPropertyAnimation(effect, "opacity");
    opacityAnim->setDuration(durationMs);
    opacityAnim->setStartValue(1.0);
    opacityAnim->setEndValue(0.0);
    opacityAnim->setEasingCurve(QEasingCurve::InCubic);

    m_disappearGroup->addAnimation(opacityAnim);
    m_disappearGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void DigestPanel::onMotivationToggle()
{
    m_motivationExpanded = !m_motivationExpanded;
    m_motivationToggle->setText(m_motivationExpanded ? "▼" : "▶");
    m_motivationContent->setVisible(m_motivationExpanded);
}

void DigestPanel::onDetailsToggle()
{
    m_detailsExpanded = !m_detailsExpanded;
    m_detailsToggle->setText(m_detailsExpanded ? "▼" : "▶");
    m_detailsContent->setVisible(m_detailsExpanded);
}

void DigestPanel::onGoToClicked()
{
    if (!m_currentFilePath.isEmpty()) {
        emit goToCodeRequested(m_currentFilePath, m_currentLineNumber);
    }
}
