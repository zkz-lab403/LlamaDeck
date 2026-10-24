# LlamaDeck

基于 **C++17 + Qt 6** 的 [llama.cpp](https://github.com/ggml-org/llama.cpp) 桌面图形管理器：扫描本地 GGUF 模型、一键启动 `llama-server`、实时查看生成速度与日志、记录启动历史，并内置 OpenAI 兼容 API 调试页与本地控制 API。

`Windows 10/11` · `C++17` · `Qt 6.4+` · `CMake`

## 功能

| 模块 | 说明 |
| --- | --- |
| 模型管理 | 递归扫描模型目录下的 `.gguf`，解析元数据（架构、参数量、上下文长度、量化类型），支持搜索、文件大小展示与一键启动 |
| 运行监控 | 健康检查、实时 token/s 速度曲线、峰值/平均速度、已生成 token 数、请求数、运行时长、实时日志 |
| 历史记录 | 每次启动自动落盘：模型、状态、时长、平均/峰值速度、token 数、完整命令，支持复制、重放、导出 CSV |
| API 接口 | 展示 `llama-server` 与本地控制 API 的端点，内置流式聊天测试（SSE），可直接在 GUI 里验证模型 |
| 设置 | 服务路径、模型目录、默认参数、启动行为、控制 API 开关与端口 |
| 命令生成 | 参数改动后实时生成完整启动命令，可一键复制到终端复用 |

其他细节：

- 深色主题、自绘图标/开关/速度图表，高 DPI 适配，窗口按屏幕可用区域自动调整
- 中文界面，Windows 原生暗色标题栏
- 关闭窗口时可自动停止推理进程，避免端口占用
- 上次使用的模型可自动启动

## 快速开始

1. 下载 `dist/LlamaDeck` 整个目录（已包含 Qt 运行库与 MinGW 运行时），双击 `LlamaDeck.exe` 运行。
2. 准备 `llama-server.exe`：从 [llama.cpp Releases](https://github.com/ggml-org/llama.cpp/releases) 下载 Windows 构建（CPU 或 CUDA/Vulkan 版本均可），解压到任意目录。
3. 首次启动进入 **设置** 页：
   - 指定 `llama-server.exe` 路径（放在 exe 同目录、`bin/` 或 PATH 中时可自动检测）
   - 指定 GGUF 模型所在目录（也可把模型放进 `dist/LlamaDeck/models`）
4. 回到 **模型管理**，选中模型，按需调整端口 / 上下文 / GPU 层数，点击 **一键启动**。
5. 服务就绪后 **运行监控** 页会显示速度曲线，**API 接口** 页可直接发一条测试消息。

> 仓库中的 `dist/LlamaDeck` 仅包含 GUI 本体和 Qt 依赖，不包含 `llama-server` 与模型文件。

## 自动生成的启动命令

GUI 会根据当前配置拼出类似命令，并显示在模型页卡片底部：

```bash
llama-server.exe --model "D:/models/Qwen3-4B-Q4_K_M.gguf" --host 127.0.0.1 --port 8080 \
  -c 4096 -ngl 99 -t 0 -fa on --jinja
```

规则：

| 参数 | 含义 |
| --- | --- |
| `--model` / `--host` / `--port` | 模型路径、监听地址（固定 `127.0.0.1`）、端口 |
| `-c` | 上下文长度 |
| `-ngl` | GPU 卸载层数（`-1` 表示不传，即纯 CPU） |
| `-t` | 线程数（`0` 表示由 llama.cpp 自动决定，不传参） |
| `-fa on` | 开启 Flash Attention（新版 llama.cpp 的 `-fa` 需要取值 `on/off/auto`） |
| `--jinja` | 使用模型自带聊天模板（OpenAI 接口必须） |
| `--api-key` | 设置了 API Key 时追加 |
| 额外参数 | 设置页/模型页填写的自定义参数，原样追加 |

## API 接口

### 1. llama-server（OpenAI 兼容）

服务运行后，默认监听 `http://127.0.0.1:8080`：

```bash
curl http://127.0.0.1:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"local","messages":[{"role":"user","content":"你好"}],"stream":true}'
```

`/v1/chat/completions`、`/v1/completions`、`/v1/models` 等端点均由 llama.cpp 提供，可被任何 OpenAI SDK 直接调用。若在设置中填写了 API Key，请加上 `Authorization: Bearer <key>`。

### 2. LlamaDeck 控制 API

用于脚本化控制 GUI 本体，默认端口 `8745`（可在设置中关闭或改端口），仅绑定 `127.0.0.1`。

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/api/health` | 存活检查 |
| GET | `/api/status` | 运行状态、当前模型、端口、命令、实时/峰值/平均速度、token 数、PID、错误信息 |
| GET | `/api/models` | 模型目录列表及元数据 |
| GET | `/api/history` | 启动历史 |
| POST | `/api/launch` | 启动指定模型 |
| POST | `/api/stop` | 停止当前推理服务 |

示例：

```bash
# 查看状态
curl http://127.0.0.1:8745/api/status

# 按文件名 / 模型名 / 完整路径启动
curl -X POST http://127.0.0.1:8745/api/launch \
  -H "Content-Type: application/json" \
  -d '{"model":"Qwen3-4B-Q4_K_M.gguf","port":8080,"ctx":8192,"ngl":99}'

# 停止
curl -X POST http://127.0.0.1:8745/api/stop
```

`/api/launch` 支持的字段：`model`（必填）、`port`、`ctx`、`ngl`、`threads`、`flashAttn`、`jinja`、`apiKey`、`extraArgs`；未提供的字段沿用设置页默认值。

## 从源码构建

环境要求：

- Windows 10/11（目前项目仅针对 Windows 打包，Qt 代码本身跨平台）
- Qt 6.4 及以上（Widgets + Network）
- CMake 3.20 及以上，C++17 编译器（MSVC 2022 或 MinGW-w64）

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/mingw_64"
cmake --build build --config Release
```

产物在 `build/LlamaDeck.exe`。如需得到本仓库 `dist/LlamaDeck` 那样可独立运行的目录：

```powershell
windeployqt --release --no-translations --compiler-runtime build/LlamaDeck.exe
```

## 项目结构

```
├─ CMakeLists.txt
├─ src
│  ├─ main.cpp
│  ├─ core                 # 与界面无关的业务层
│  │  ├─ Types.h           # 启动参数、状态、统计、历史记录结构
│  │  ├─ AppSettings.*     # INI 配置、路径检测、数据目录
│  │  ├─ GgufParser.*      # GGUF 头部元数据解析
│  │  ├─ ModelStore.*      # 模型扫描、量化识别、格式化
│  │  ├─ HistoryStore.*    # 历史 JSON 持久化、CSV 导出
│  │  ├─ ServerController.*# 进程管理、健康检查、metrics 与日志测速
│  │  └─ ControlApiServer.*# 本地 HTTP 控制 API
│  └─ ui                   # 界面层
│     ├─ Theme.*           # 配色与 QSS
│     ├─ Icons.*           # 自绘图标
│     ├─ Widgets.*         # 卡片、开关、徽标等通用控件
│     ├─ SpeedChart.*      # 速度曲线
│     ├─ ModelPage.*       # 模型管理
│     ├─ MonitorPage.*     # 运行监控
│     ├─ HistoryPage.*     # 历史记录
│     ├─ ApiPage.*         # API 文档与聊天测试
│     ├─ SettingsPage.*    # 设置
│     └─ MainWindow.*      # 侧边栏、页面调度、自动启动
└─ dist/LlamaDeck          # 打包好的 Windows 运行目录（不含 llama-server）
```

## 数据位置

配置与历史保存在 `%LOCALAPPDATA%\LlamaDeck`：

- `settings.ini`：界面与服务配置
- `history.json`：启动历史（最多保留 300 条）

首次运行会自动创建该目录；删除即可恢复出厂状态。

## 速度是怎么算的

- 优先使用 `llama-server` 日志中的计时行（prompt eval / eval time）计算 tokens/s，更贴近实际生成速度；
- 同时轮询 `/metrics` 获取累计 token 数、请求数、活跃请求数；
- 峰值取单次评估的最高速度，平均按已生成 token 与累计生成耗时的加权值计算，保证平均不超过峰值。

## 常见问题

- **提示未检测到 llama-server**：在设置页手动选择 `llama-server.exe`；也可把它放到 GUI 同目录、`bin/` 子目录或加入 PATH。
- **启动失败 / 端口占用**：换一个端口，或确认没有残留的 `llama-server` 进程。
- **模型列表为空**：确认目录下有 `.gguf` 文件；扫描是递归的，子目录也会被包含。
- **浏览器打开 Web UI 空白**：Web UI 属于 `llama-server`，需等健康检查通过后再访问。
