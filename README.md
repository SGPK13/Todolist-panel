# 待办面板

同时做几个项目时，我经常做完一件事，才想起来还有别的事没推进。这个小浮窗把待办放在 Windows 右上角。写文档或看代码时抬眼就能看，挡住内容了，就收成屏幕边缘的小把手。

![实录：勾选待办、切换项目、收起再展开](assets/demo.gif)

[看 MP4 原片](assets/demo.mp4)

## 怎么用

- 点底部的 `+` 记一条待办，按回车保存。要连续录入，用 `Shift+Enter`。
- 点顶部的筛选按钮切换项目。菜单底部可以新建项目，项目标题右侧的 `+` 可以直接往这组里加待办。
- 勾掉做完的事，点任务右侧的色点改紧急程度。如果已经想好了接下来做什么，右键选「设为下一件」。
- 点右上角的箭头收起，点屏幕边缘的把手展开。要换屏幕、切深浅色或调透明度，右键面板空白处。

任务和设置保存在本机的 `%APPDATA%\TodolistPanel\TodolistPanel.ini`。

## 从源码运行

需要 Windows、Qt 6.5 或更新版本，以及配套的 C++ 编译器、CMake 和 Ninja。可以在 Qt Creator 中打开 `CMakeLists.txt`，选桌面套件构建。

也可以在 PowerShell 中构建。下面以 Qt 6.9.3 和 MinGW 13.1 为例，把路径换成自己的安装位置。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build --parallel
cmake --install build --prefix (Join-Path $PWD dist)
```

构建后运行 `dist\bin\TodolistPanel.exe`。

## 许可证

项目源码采用 [MIT 许可证](LICENSE)。Qt 和其他第三方依赖各自适用其许可证。
