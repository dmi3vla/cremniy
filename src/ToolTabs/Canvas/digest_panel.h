#ifndef DIGEST_PANEL_H
#define DIGEST_PANEL_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include "codemap.h"

class DigestPanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal panelOpacity READ panelOpacity WRITE setPanelOpacity)

public:
    explicit DigestPanel(QWidget* parent = nullptr);

    void showMap(const Codemap& map);
    void setMotivation(const QString& motivation);
    void setDetails(const QString& details);
    void setCodeSnippet(const QString& snippet, const QString& filePath, int lineNumber);

    void playAppearAnimation(int durationMs = 300);
    void playDisappearAnimation(int durationMs = 250);

    qreal panelOpacity() const { return m_panelOpacity; }
    void setPanelOpacity(qreal opacity) { m_panelOpacity = opacity; update(); }

signals:
    void goToCodeRequested(const QString& filePath, int lineNumber);
    void stepNavigationRequested(const QString& filePath, int lineNumber);

private slots:
    void onMotivationToggle();
    void onDetailsToggle();
    void onGoToClicked();

private:
    void setupUI();
    void createCollapsibleSection(const QString& title, QWidget* content, QToolButton*& toggleBtn, QWidget*& container);

    QVBoxLayout* m_layout;
    QList<QWidget*> m_clusterSections;

    QLabel* m_motivationLabel;
    QTextEdit* m_motivationContent;
    QToolButton* m_motivationToggle;
    QWidget* m_motivationContainer;

    QLabel* m_detailsLabel;
    QTextEdit* m_detailsContent;
    QToolButton* m_detailsToggle;
    QWidget* m_detailsContainer;

    QLabel* m_codeSnippetLabel;
    QTextEdit* m_codeSnippetContent;
    QPushButton* m_goToButton;

    QString m_currentFilePath;
    int m_currentLineNumber;

    qreal m_panelOpacity = 1.0;
    QParallelAnimationGroup* m_appearGroup = nullptr;
    QParallelAnimationGroup* m_disappearGroup = nullptr;

    bool m_motivationExpanded = true;
    bool m_detailsExpanded = true;
};

#endif // DIGEST_PANEL_H
