#include "gource_animator.h"
#include <QDir>

GourceAnimator::GourceAnimator(const QString& projectPath, QObject* parent)
    : QObject(parent)
    , m_projectPath(projectPath)
{
    connect(&m_animTimer, &QTimer::timeout, this, &GourceAnimator::animationTick);
}

void GourceAnimator::loadHistory()
{
    m_gitProcess = new QProcess(this);
    connect(m_gitProcess, &QProcess::readyReadStandardOutput, this, &GourceAnimator::onGitOutput);
    connect(m_gitProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GourceAnimator::onGitFinished);

    m_gitProcess->setWorkingDirectory(m_projectPath);
    m_gitProcess->start("git", {"log", "--name-only", "--pretty=format:%H|%ae|%at", "--diff-filter=ACDMR"});
}

void GourceAnimator::onGitOutput()
{
    if (!m_gitProcess) return;
    QByteArray data = m_gitProcess->readAllStandardOutput();
    parseGitLog(QString::fromUtf8(data));
}

void GourceAnimator::onGitFinished(int exitCode)
{
    Q_UNUSED(exitCode)
    emit historyLoaded(m_commits.size());
}

void GourceAnimator::parseGitLog(const QString& output)
{
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    GitCommit current;
    bool inCommit = false;

    for (const QString& line : lines) {
        if (line.contains('|') && line.length() > 40) {
            if (inCommit && !current.files.isEmpty())
                m_commits.append(current);

            QStringList parts = line.split('|');
            if (parts.size() >= 3) {
                current = GitCommit();
                current.hash = parts[0];
                current.author = parts[1];
                current.timestamp = parts[2].toLongLong();
                inCommit = true;
            }
        } else if (inCommit && !line.trimmed().isEmpty()) {
            current.files.append(line.trimmed());
        }
    }
    if (inCommit && !current.files.isEmpty())
        m_commits.append(current);
}

void GourceAnimator::play()
{
    if (m_commits.isEmpty()) return;
    m_playing = true;
    m_animTimer.start(1000 / m_speed);
}

void GourceAnimator::pause()
{
    m_playing = false;
    m_animTimer.stop();
}

void GourceAnimator::stop()
{
    pause();
    m_currentIndex = 0;
    emit progressChanged(0, m_commits.size());
}

void GourceAnimator::setSpeed(int commitsPerSecond)
{
    m_speed = qMax(1, commitsPerSecond);
    if (m_playing) {
        m_animTimer.setInterval(1000 / m_speed);
    }
}

void GourceAnimator::scrubTo(int index)
{
    m_currentIndex = qBound(0, index, m_commits.size() - 1);
    emit progressChanged(m_currentIndex, m_commits.size());
    emit commitReady(m_commits[m_currentIndex]);
}

void GourceAnimator::animationTick()
{
    if (m_currentIndex >= m_commits.size()) {
        pause();
        return;
    }

    emit commitReady(m_commits[m_currentIndex]);
    m_currentIndex++;
    emit progressChanged(m_currentIndex, m_commits.size());
}
