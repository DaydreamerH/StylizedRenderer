# StylizedRenderer

StylizedRenderer 是一个以 OpenGL 为后端、以实时三渲二角色渲染为首个目标的 C++ 渲染项目。

当前仓库已完成最小开发环境，并正在实施第一阶段 Graphics Foundation：

- C++20；
- CMake；
- Visual Studio 2022 x64；
- GLFW 3.4；
- GLAD 2.0.8 生成的 OpenGL 4.5 Core 加载代码；
- `stylized_engine` 静态库；
- Application、Window 和 OpenGLContext 生命周期；
- 最小 GraphicsDevice；
- 带 OpenGL 调试输出的清屏窗口。

详细设计和开发计划见 [PROJECT_PROPOSAL.md](PROJECT_PROPOSAL.md)。

## 环境要求

- Windows 10/11；
- Visual Studio 2022，并安装“使用 C++ 的桌面开发”；
- CMake 3.25 或更高版本；
- Git；
- 支持 OpenGL 4.5 的显卡驱动；
- 首次配置时可以访问 GitHub，用于下载 GLFW。

## 配置

```powershell
cmake --preset windows-vs2022
```

不要在同一个 `build` 目录中混用不同版本的 CMake。若从 Visual Studio
内置 CMake 切换到独立安装的 CMake，应该重新生成对应构建目录。

## 构建

Debug：

```powershell
cmake --build --preset debug
```

Release：

```powershell
cmake --build --preset release
```

## 运行

```powershell
.\build\windows-vs2022\apps\viewer\Debug\stylized_viewer.exe
```

按 `Esc` 退出。

无窗口冒烟测试：

```powershell
.\build\windows-vs2022\apps\viewer\Debug\stylized_viewer.exe --smoke-test
```

## 当前边界

GLM、Dear ImGui、glTF 加载器和测试框架将在对应里程碑真正需要时再引入，当前不作为基础环境依赖。
