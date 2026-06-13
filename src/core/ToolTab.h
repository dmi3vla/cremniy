#ifndef TOOLTAB_H
#define TOOLTAB_H

#include "FileDataBuffer.h"
#include "utils/filecontext.h"
#include <QWidget>

class ToolTab : public QWidget {
    Q_OBJECT

protected:
    /**
     * @brief Общий буфер данных
     *
     * Хранит все данные файла и обеспечивает синхронизацию между вкладками
     */
    FileDataBuffer* m_dataBuffer;

    /**
     * @brief Контекст файла
     */
    FileContext* m_fileContext = nullptr;

    /**
     * @brief Флаг означающий изменения в данных
     *
     * Если true - данные изменены, false - данные равны данным в файле
     */
    bool m_modifyIndicator = false;

public:
    /**
     * @brief Конструктор класса
     *
     * @param buffer указатель на общий буфер данных
     * @param parent родительский виджет
     */
    explicit ToolTab(FileDataBuffer* buffer, QWidget* parent = nullptr)
        : QWidget(parent), m_dataBuffer(buffer)
    {
        // Подписываемся на сигналы буфера
        connect(m_dataBuffer, &FileDataBuffer::byteChanged,
            this, &ToolTab::onByteChanged);
        connect(m_dataBuffer, &FileDataBuffer::bytesChanged,
            this, &ToolTab::onBytesChanged);
        connect(m_dataBuffer, &FileDataBuffer::selectionChanged,
            this, &ToolTab::onSelectionChanged);
        connect(m_dataBuffer, &FileDataBuffer::dataChanged,
            this, &ToolTab::onDataChanged);
    }

    /**
     * @brief Получить название инструмента для вкладки
     */
    virtual QString toolName() const = 0;

    /**
     * @brief Получить лого инструмента для вкладки
     */
    virtual QIcon toolIcon() const = 0;

    /**
     * @brief Получить значение индикатора изменений
     */
    bool getModifyIndicator() {
        return m_modifyIndicator;
    }

    /**
     * @brief Установить значение индикатора изменений
     */
    void setModifyIndicator(bool value) {
        m_modifyIndicator = value;
    }

protected slots:
    /**
     * @brief Обработчик изменения байта
     *
     * Вызывается при изменении байта в буфере
     * @param pos позиция измененного байта
     */
    virtual void onByteChanged(qint64 pos) { Q_UNUSED(pos); }

    /**
     * @brief Обработчик изменения диапазона байтов
     *
     * Вызывается при изменении диапазона байтов в буфере
     * @param pos начальная позиция
     * @param length длина диапазона
     */
    virtual void onBytesChanged(qint64 pos, qint64 length) { Q_UNUSED(pos); Q_UNUSED(length); }

    /**
     * @brief Обработчик изменения выделения
     *
     * Вызывается при изменении выделения в буфере
     * @param pos начальная позиция выделения
     * @param length длина выделения
     */
    virtual void onSelectionChanged(qint64 pos, qint64 length) { Q_UNUSED(pos); Q_UNUSED(length); }

    /**
     * @brief Обработчик полного изменения данных
     *
     * Вызывается при загрузке нового файла
     */
    virtual void onDataChanged() {}

public slots:
    /**
     * @brief Установить файл инструменту
     *
     * @param filepath путь к файлу
     */
    virtual void setFile(QString filepath) = 0;

    /**
     * @brief Установить данные из файла во вкладку
     */
    virtual void setTabData() = 0;

    /**
     * @brief Сохраняет данные из вкладки в файл
     */
    virtual void saveTabData() = 0;

signals:
    void refreshDataAllTabsSignal();
    void modifyData();
    void dataEqual();
    void fileOpenRequested(const QString& filePath);
};

#endif // TOOLTAB_H
