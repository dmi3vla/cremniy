#include "endpoint_manager.h"

#include "utils/appsettings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

EndpointManager& EndpointManager::instance()
{
    static EndpointManager inst;
    return inst;
}

EndpointManager::EndpointManager(QObject* parent)
    : QObject(parent)
{
    loadFromSettings();
}

QList<ApiEndpoint> EndpointManager::endpoints() const
{
    return m_endpoints;
}

ApiEndpoint EndpointManager::activeEndpoint() const
{
    if (m_activeIndex >= 0 && m_activeIndex < m_endpoints.size()) {
        return m_endpoints[m_activeIndex];
    }
    if (!m_endpoints.isEmpty()) {
        return m_endpoints[0];
    }
    return ApiEndpoint();
}

int EndpointManager::activeEndpointIndex() const
{
    return m_activeIndex;
}

QString EndpointManager::formatToString(ApiEndpoint::ApiFormat format)
{
    switch (format) {
    case ApiEndpoint::ApiFormat::OpenAIChatCompletions:
        return QStringLiteral("openai_chat_completions");
    case ApiEndpoint::ApiFormat::AnthropicMessages:
    default:
        return QStringLiteral("anthropic_messages");
    }
}

ApiEndpoint::ApiFormat EndpointManager::formatFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("openai") ||
        normalized == QLatin1String("openai_chat_completions") ||
        normalized == QLatin1String("openai-compatible")) {
        return ApiEndpoint::ApiFormat::OpenAIChatCompletions;
    }
    return ApiEndpoint::ApiFormat::AnthropicMessages;
}

void EndpointManager::addEndpoint(const ApiEndpoint& ep)
{
    m_endpoints.append(ep);
    clampActiveIndex();
    saveToSettings();
    emit endpointsChanged();
}

void EndpointManager::updateEndpoint(int index, const ApiEndpoint& ep)
{
    if (index < 0 || index >= m_endpoints.size()) {
        return;
    }

    m_endpoints[index] = ep;
    clampActiveIndex();
    saveToSettings();
    emit endpointsChanged();
    if (index == m_activeIndex) {
        emit activeEndpointChanged();
    }
}

void EndpointManager::removeEndpoint(int index)
{
    if (index < 0 || index >= m_endpoints.size()) {
        return;
    }

    m_endpoints.removeAt(index);
    ensureDefaults();
    clampActiveIndex();
    saveToSettings();
    emit endpointsChanged();
    emit activeEndpointChanged();
}

void EndpointManager::setActive(int index)
{
    if (index < 0 || index >= m_endpoints.size() || index == m_activeIndex) {
        return;
    }

    m_activeIndex = index;
    saveToSettings();
    emit activeEndpointChanged();
}

QString EndpointManager::secretsFilePath() const
{
    const QString path = QDir::homePath() + QStringLiteral("/.config/cremniy/secrets.ini");
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    return path;
}

void EndpointManager::ensureDefaults()
{
    if (!m_endpoints.isEmpty()) {
        return;
    }

    ApiEndpoint anthropic;
    anthropic.name = QStringLiteral("Anthropic Cloud");
    anthropic.baseUrl = QStringLiteral("https://api.anthropic.com/v1");
    anthropic.defaultModel = QStringLiteral("claude-sonnet-4-6");
    anthropic.format = ApiEndpoint::ApiFormat::AnthropicMessages;

    ApiEndpoint local;
    local.name = QStringLiteral("Local");
    local.baseUrl = QStringLiteral("http://localhost:20128/v1");
    local.defaultModel = QString();
    local.format = ApiEndpoint::ApiFormat::OpenAIChatCompletions;

    m_endpoints.append(anthropic);
    m_endpoints.append(local);
    m_activeIndex = 0;
}

void EndpointManager::migrateLegacySettings()
{
    if (m_endpoints.isEmpty()) {
        return;
    }

    const QString legacyKey = AppSettings::llmApiKey();
    const QString legacyModel = AppSettings::llmModel();
    if (legacyKey.isEmpty() && legacyModel.isEmpty()) {
        return;
    }

    int anthropicIndex = -1;
    for (int i = 0; i < m_endpoints.size(); ++i) {
        if (m_endpoints[i].format == ApiEndpoint::ApiFormat::AnthropicMessages) {
            anthropicIndex = i;
            break;
        }
    }
    if (anthropicIndex < 0) {
        anthropicIndex = 0;
    }

    if (!legacyKey.isEmpty() && m_endpoints[anthropicIndex].apiKey.isEmpty()) {
        m_endpoints[anthropicIndex].apiKey = legacyKey;
    }
    if (!legacyModel.isEmpty() && m_endpoints[anthropicIndex].defaultModel.isEmpty()) {
        m_endpoints[anthropicIndex].defaultModel = legacyModel;
    }
}

void EndpointManager::clampActiveIndex()
{
    if (m_endpoints.isEmpty()) {
        m_activeIndex = 0;
        return;
    }
    if (m_activeIndex < 0 || m_activeIndex >= m_endpoints.size()) {
        m_activeIndex = 0;
    }
}

void EndpointManager::saveToSettings()
{
    ensureDefaults();
    clampActiveIndex();

    QSettings s;
    s.beginWriteArray("AIAssistant/Endpoints", m_endpoints.size());
    for (int i = 0; i < m_endpoints.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("name", m_endpoints[i].name);
        s.setValue("baseUrl", m_endpoints[i].baseUrl);
        s.setValue("defaultModel", m_endpoints[i].defaultModel);
        s.setValue("format", formatToString(m_endpoints[i].format));
    }
    s.endArray();
    s.setValue("AIAssistant/ActiveEndpointIndex", m_activeIndex);
    s.sync();

    QSettings secrets(secretsFilePath(), QSettings::IniFormat);
    secrets.beginWriteArray("EndpointsKeys", m_endpoints.size());
    for (int i = 0; i < m_endpoints.size(); ++i) {
        secrets.setArrayIndex(i);
        secrets.setValue("name", m_endpoints[i].name);
        secrets.setValue("apiKey", m_endpoints[i].apiKey);
    }
    secrets.endArray();
    secrets.sync();
}

void EndpointManager::loadFromSettings()
{
    m_endpoints.clear();

    QSettings s;
    const int size = s.beginReadArray("AIAssistant/Endpoints");
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        ApiEndpoint ep;
        ep.name = s.value("name").toString().trimmed();
        ep.baseUrl = s.value("baseUrl").toString().trimmed();
        ep.defaultModel = s.value("defaultModel").toString().trimmed();

        const QVariant formatValue = s.value("format", QStringLiteral("anthropic_messages"));
        if (formatValue.typeId() == QMetaType::Int) {
            ep.format = static_cast<ApiEndpoint::ApiFormat>(formatValue.toInt());
        } else {
            ep.format = formatFromString(formatValue.toString());
        }

        if (!ep.name.isEmpty() || !ep.baseUrl.isEmpty()) {
            m_endpoints.append(ep);
        }
    }
    s.endArray();

    m_activeIndex = s.value("AIAssistant/ActiveEndpointIndex", 0).toInt();

    QSettings secrets(secretsFilePath(), QSettings::IniFormat);
    const int keysSize = secrets.beginReadArray("EndpointsKeys");
    for (int i = 0; i < keysSize; ++i) {
        secrets.setArrayIndex(i);
        if (i < m_endpoints.size()) {
            m_endpoints[i].apiKey = secrets.value("apiKey").toString().trimmed();
        }
    }
    secrets.endArray();

    ensureDefaults();
    migrateLegacySettings();
    clampActiveIndex();
    saveToSettings();
}
