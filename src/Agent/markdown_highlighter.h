#ifndef MARKDOWN_HIGHLIGHTER_H
#define MARKDOWN_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>

class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightRule> m_rules;
    QRegularExpression m_codeBlockStart;
    QRegularExpression m_codeBlockEnd;
    QTextCharFormat m_codeBlockFormat;

    void addRule(const QString& pattern, const QTextCharFormat& format);
};

#endif // MARKDOWN_HIGHLIGHTER_H
