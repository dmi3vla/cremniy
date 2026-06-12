# Быстрый старт: Qt 6.8.3 из `qtbase` и сборка Cremniy

## Что действительно нужно проекту

В [`src/CMakeLists.txt`](../../src/CMakeLists.txt) проект требует [`find_package(Qt6 6.8.3 REQUIRED COMPONENTS Widgets Test)`](../../src/CMakeLists.txt#L12), а линковка идёт с [`Qt6::Widgets`](../../src/CMakeLists.txt#L101).

Для такой сборки достаточно только подмодуля `qtbase` версии `6.8.3` из архива Qt:

- `qtbase` содержит `Qt6::Widgets`
- `qtbase` также содержит `Qt6Test`
- дополнительные подмодули Qt для текущей CMake-сборки не требуются

Источник архива:

- <https://download.qt.io/archive/qt/6.8/6.8.3/submodules/>

## 1. Установка системных зависимостей на Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  perl \
  python3 \
  pkg-config \
  libgl1-mesa-dev \
  libfontconfig1-dev \
  libfreetype6-dev \
  libx11-dev \
  libx11-xcb-dev \
  libxext-dev \
  libxfixes-dev \
  libxi-dev \
  libxrender-dev \
  libxcb1-dev \
  libxcb-cursor-dev \
  libxcb-glx0-dev \
  libxcb-keysyms1-dev \
  libxcb-image0-dev \
  libxcb-shm0-dev \
  libxcb-icccm4-dev \
  libxcb-sync-dev \
  libxcb-xfixes0-dev \
  libxcb-shape0-dev \
  libxcb-randr0-dev \
  libxcb-render-util0-dev \
  libxcb-util-dev \
  libxcb-xinerama0-dev \
  libxrandr-dev \
  libxcursor-dev \
  libxcomposite-dev \
  libxdamage-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libgtk-3-dev
```

## 2. Скачивание только `qtbase` 6.8.3

```bash
mkdir -p ./third_party/qt-src
cd ./third_party/qt-src
wget https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz
tar -xf qtbase-everywhere-src-6.8.3.tar.xz
```

## 3. Сборка и установка Qt 6.8.3 в отдельный префикс

Ниже пример установки в `/opt/qt-6.8.3-min`.

```bash
cd ./third_party/qt-src/qtbase-everywhere-src-6.8.3
mkdir -p build
cd build
../configure \
  -prefix /opt/qt-6.8.3-min \
  -release \
  -opensource \
  -confirm-license \
  -nomake examples \
  -nomake tests
cmake --build . --parallel
sudo cmake --install .
```

## 4. Проверка установленного Qt

```bash
/opt/qt-6.8.3-min/bin/qmake -query QT_VERSION
/opt/qt-6.8.3-min/bin/qtpaths --version
```

Ожидаемая версия: `6.8.3`.

## 5. Сборка Cremniy через CMake

Из корня репозитория:

```bash
mkdir -p build
cmake -S ./src -B ./build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/qt-6.8.3-min/lib/cmake
cmake --build ./build --parallel
```

## 6. Запуск собранного приложения

Если Qt установлен в нестандартный путь, перед запуском укажите `LD_LIBRARY_PATH`:

```bash
LD_LIBRARY_PATH=/opt/qt-6.8.3-min/lib ./build/cremniy
```

## 7. Полная последовательность команд

Если нужен непрерывный сценарий без пояснений:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build perl python3 pkg-config \
  libgl1-mesa-dev libfontconfig1-dev libfreetype6-dev \
  libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev \
  libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev libxcb-keysyms1-dev \
  libxcb-image0-dev libxcb-shm0-dev libxcb-icccm4-dev libxcb-sync-dev \
  libxcb-xfixes0-dev libxcb-shape0-dev libxcb-randr0-dev \
  libxcb-render-util0-dev libxcb-util-dev libxcb-xinerama0-dev \
  libxrandr-dev libxcursor-dev libxcomposite-dev libxdamage-dev \
  libxkbcommon-dev libxkbcommon-x11-dev libgtk-3-dev

mkdir -p ./third_party/qt-src
cd ./third_party/qt-src
wget https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz
tar -xf qtbase-everywhere-src-6.8.3.tar.xz
cd ./qtbase-everywhere-src-6.8.3
mkdir -p build
cd build
../configure \
  -prefix /opt/qt-6.8.3-min \
  -release \
  -opensource \
  -confirm-license \
  -nomake examples \
  -nomake tests
cmake --build . --parallel
sudo cmake --install .
cd ../../..
mkdir -p build
cmake -S ./src -B ./build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/qt-6.8.3-min/lib/cmake
cmake --build ./build --parallel
LD_LIBRARY_PATH=/opt/qt-6.8.3-min/lib ./build/cremniy
```

## Примечания

- Если не хотите устанавливать Qt в системный путь, замените `/opt/qt-6.8.3-min` на каталог в домашней директории, например `$HOME/.local/qt-6.8.3-min`.
- Для текущего дерева исходников подмодуль `qttools` не нужен.
- Если в будущем в CMake появятся новые Qt-компоненты помимо `Widgets` и `Test`, минимальный набор подмодулей нужно будет пересмотреть.
