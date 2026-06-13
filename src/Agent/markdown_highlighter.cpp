#include "markdown_highlighter.h"

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Headers
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor(130, 170, 255));
    headerFormat.setFontWeight(QFont::Bold);
    addRule("^#{1,6}\\s+.+$", headerFormat);

    // Bold
    QTextCharFormat boldFormat;
    boldFormat.setFontWeight(QFont::Bold);
    addRule("\\*\\*[^*]+\\*\\*", boldFormat);
    addRule("__[^_]+__", boldFormat);

    // Italic
    QTextCharFormat italicFormat;
    italicFormat.setFontItalic(true);
    addRule("\\*[^*]+\\*", italicFormat);
    addRule("_[^_]+_", italicFormat);

    // Inline code
    QTextCharFormat inlineCodeFormat;
    inlineCodeFormat.setForeground(QColor(220, 180, 120));
    inlineCodeFormat.setBackground(QColor(45, 45, 55));
    inlineCodeFormat.setFontFamilies({"monospace"});
    addRule("`[^`]+`", inlineCodeFormat);

    // Links
    QTextCharFormat linkFormat;
    linkFormat.setForeground(QColor(100, 180, 255));
    linkFormat.setFontUnderline(true);
    addRule("\\[([^\\]]+)\\]\\([^)]+\\)", linkFormat);

    // Code blocks
    m_codeBlockStart.setPattern("^```\\w*$");
    m_codeBlockEnd.setPattern("^```$");
    m_codeBlockFormat.setForeground(QColor(200, 200, 210));
    m_codeBlockFormat.setBackground(QColor(35, 35, 45));
    m_codeBlockFormat.setFontFamilies({"monospace"});
}

void MarkdownHighlighter::addRule(const QString& pattern, const QTextCharFormat& format)
{
    HighlightRule rule;
    rule.pattern = QRegularExpression(pattern);
    rule.format = format;
    m_rules.append(rule);
}

void MarkdownHighlighter::highlightBlock(const QString& text)
{
    // Handle code blocks (multi-line)
    setCurrentBlockState(0);

    if (previousBlockState() == 1) {
        // Inside a code block
        if (m_codeBlockEnd.match(text).hasMatch()) {
            setCurrentBlockState(0);
        } else {
            setFormat(0, text.length(), m_codeBlockFormat);
            setCurrentBlockState(1);
            return;
        }
    }

    if (m_codeBlockStart.match(text).hasMatch()) {
        setFormat(0, text.length(), m_codeBlockFormat);
        setCurrentBlockState(1);
        return;
    }

    // Single-line rules
    for (const auto& rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
