#ifndef ENDPOINT_MANAGER_H
#define ENDPOINT_MANAGER_H

#include <QObject>
#include <QString>
#include <QList>

struct ApiEndpoint {
    enum class ApiFormat { AnthropicMessages, OpenAIChatCompletions };

    QString name;
    QString baseUrl;
    QString apiKey;
    QString defaultModel;
    ApiFormat format = ApiFormat::AnthropicMessages;
};

class EndpointManager : public QObject
{
    Q_OBJECT
public:
    static EndpointManager& instance();

    QList<ApiEndpoint> endpoints() const;
    ApiEndpoint activeEndpoint() const;
    int activeEndpointIndex() const;

    static QString formatToString(ApiEndpoint::ApiFormat format);
    static ApiEndpoint::ApiFormat formatFromString(const QString& value);

    void addEndpoint(const ApiEndpoint& ep);
    void updateEndpoint(int index, const ApiEndpoint& ep);
    void removeEndpoint(int index);
    void setActive(int index);

    void saveToSettings();
    void loadFromSettings();

signals:
    void activeEndpointChanged();
    void endpointsChanged();

private:
    explicit EndpointManager(QObject* parent = nullptr);
    ~EndpointManager() override = default;

    QList<ApiEndpoint> m_endpoints;
    int m_activeIndex = 0;

    QString secretsFilePath() const;
    void ensureDefaults();
    void migrateLegacySettings();
    void clampActiveIndex();
};

#endif // ENDPOINT_MANAGER_H
