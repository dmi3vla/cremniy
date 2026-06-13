#ifndef GOURCE_ANIMATOR_H
#define GOURCE_ANIMATOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QTimer>
#include <QProcess>

struct GitCommit {
    QString hash;
    QString author;
    qint64 timestamp;
    QStringList files;
};

class GourceAnimator : public QObject
{
    Q_OBJECT

public:
    explicit GourceAnimator(const QString& projectPath, QObject* parent = nullptr);

    void loadHistory();
    void play();
    void pause();
    void stop();
    bool isPlaying() const { return m_playing; }

    void setSpeed(int commitsPerSecond);
    int speed() const { return m_speed; }

    int totalCommits() const { return m_commits.size(); }
    int currentCommit() const { return m_currentIndex; }

    void scrubTo(int index);

signals:
    void commitReady(const GitCommit& commit);
    void progressChanged(int current, int total);
    void historyLoaded(int commitCount);

private slots:
    void onGitOutput();
    void onGitFinished(int exitCode);
    void animationTick();

private:
    QString m_projectPath;
    QList<GitCommit> m_commits;
    QProcess* m_gitProcess = nullptr;
    QTimer m_animTimer;
    int m_currentIndex = 0;
    int m_speed = 2;
    bool m_playing = false;

    void parseGitLog(const QString& output);
};

#endif // GOURCE_ANIMATOR_H
