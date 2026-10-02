#!/bin/bash
# Скрипт для сборки cremniy с правильным путём к Qt 6.8.3

cd "$(dirname "$0")"

# Путь к локальной сборке Qt
QT_PATH="$(pwd)/qt-6.8.3-install/lib/cmake/Qt6"

# Проверка наличия Qt
if [ ! -f "$QT_PATH/Qt6Config.cmake" ]; then
    echo "ОШИБКА: Qt6Config.cmake не найден по пути:"
    echo "  $QT_PATH"
    echo ""
    echo "Нужно сначала собрать Qt 6.8.3 или проверить путь."
    exit 1
fi

# Удалить старую сборку и создать новую
rm -rf build
mkdir -p build
cd build

# Конфигурация и сборка
cmake ../src -DQt6_DIR="$QT_PATH" && \
cmake --build . --parallel "$(nproc)"

echo ""
echo "Сборка завершена!"
echo "Исполняемый файл: ./build/cremniy"
