# webrtc-camera

将 Linux 本地 V4L2 摄像头通过 WebRTC 推送到同一局域网内的浏览器。

本项目固定读取 `/dev/video4`：程序先用 VLC 将摄像头画面转为低延迟 H.264 RTSP 流，再由 [webrtc-streamer](https://github.com/mpromonet/webrtc-streamer) 转为浏览器可播放的 WebRTC 视频。

```text
/dev/video4
    │
    ▼
VLC（H.264 转码）
    │  rtsp://127.0.0.1:8554/video4
    ▼
webrtc-streamer
    │  http://<主机 IP>:8000
    ▼
局域网浏览器
```

## 功能

- 自动检测并配置 `/dev/video4`
- 优先使用 MJPEG，失败时回退到 YUYV
- 自动启动和停止 VLC、webrtc-streamer
- 使用 H.264 Baseline、约 800 kbps 和低延迟编码参数
- 启动前检查 `8000`、`8554` 端口，并等待服务真正就绪
- 支持自定义分辨率和帧率

## 环境要求

- Linux x86_64
- 可用的 V4L2 摄像头设备 `/dev/video4`
- GCC、VLC（提供 `cvlc`）、curl、tar 和 sha256sum
- 支持 WebRTC 的现代浏览器
- 运行端与观看端处于可互通的局域网

在 Ubuntu/Debian 上可安装以下依赖：

```bash
sudo apt update
sudo apt install build-essential vlc curl ca-certificates v4l-utils
```

> 安装脚本当前只下载 webrtc-streamer 的 Linux x86_64 版本，不支持 ARM。请使用普通用户运行程序，不要使用 `sudo` 启动 VLC。

## 快速开始

### 1. 获取项目

```bash
git clone https://github.com/Luckyt1/webrtc-camera.git
cd webrtc-camera
```

### 2. 确认摄像头

```bash
ls -l /dev/video4
v4l2-ctl --device=/dev/video4 --list-formats-ext
```

如果系统中不存在 `/dev/video4`，请先用 `v4l2-ctl --list-devices` 确认设备编号。当前版本的设备路径写在 `src/test.c` 中，需要修改后重新编译。

### 3. 安装 webrtc-streamer

```bash
./install_webrtc_streamer.sh
```

脚本会下载并校验 webrtc-streamer `v0.7.2`，然后安装到项目内的 `.webrtc-streamer/` 目录，不会写入系统目录。

### 4. 编译并运行

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o src/test src/test.c
./src/test
```

也可以指定宽度、高度和帧率：

```bash
./src/test 1280 720 30
```

参数均为可选正整数：

| 参数 | 默认值 | 含义 |
| --- | ---: | --- |
| `width` | `640` | 视频宽度 |
| `height` | `480` | 视频高度 |
| `fps` | `30` | 视频帧率 |

指定的组合必须是摄像头支持的格式；可以通过 `v4l2-ctl --device=/dev/video4 --list-formats-ext` 查看。

## 观看视频

服务启动成功后，在浏览器中打开：

- 本机：<http://localhost:8000/?video=%2Fdev%2Fvideo4&options=rtptransport%3Dudp>
- 其他设备：`http://<运行端局域网IP>:8000/?video=%2Fdev%2Fvideo4&options=rtptransport%3Dudp`

例如，运行端 IP 为 `192.168.88.120` 时：

```text
http://192.168.88.120:8000/?video=%2Fdev%2Fvideo4&options=rtptransport%3Dudp
```

可使用下面的命令查找运行端的局域网 IP：

```bash
hostname -I
```

按回车或 `Ctrl+C` 退出。程序会清理本次启动的 VLC 和 webrtc-streamer 进程。

## 默认配置

| 项目 | 值 |
| --- | --- |
| 摄像头 | `/dev/video4` |
| RTSP 地址 | `rtsp://127.0.0.1:8554/video4` |
| Web 服务 | `0.0.0.0:8000` |
| 视频编码 | H.264 Baseline，无音频 |
| 目标码率 | 约 800 kbps |
| 默认画面 | 640 × 480，30 fps |

## 常见问题

### 无法打开 `/dev/video4`

先确认设备存在且当前用户有权限：

```bash
ls -l /dev/video4
groups
```

Ubuntu/Debian 通常需要将用户加入 `video` 组，重新登录后生效：

```bash
sudo usermod -aG video "$USER"
```

如果设备正被其他程序占用，可用 `fuser /dev/video4` 查找占用进程。

### VLC 视频流启动失败

确认已安装 `cvlc`，摄像头未被占用，并且所选分辨率、帧率受设备支持：

```bash
command -v cvlc
v4l2-ctl --device=/dev/video4 --list-formats-ext
```

### 端口已被占用

程序需要监听 TCP 端口 `8000` 和 `8554`。可检查占用者：

```bash
ss -ltnp | grep -E ':(8000|8554)\b'
```

关闭冲突服务后重新运行程序。

### 其他设备无法访问

请依次确认：

1. URL 中使用的是运行端的局域网 IP，而不是 `localhost`。
2. 两台设备网络互通，且未启用阻止客户端互访的访客网络或 AP 隔离。
3. 防火墙允许访问 `8000/tcp` 以及 WebRTC 协商产生的 UDP 流量。
4. 页面已成功打开，浏览器没有禁用 WebRTC。

### 找不到 webrtc-streamer

在项目根目录重新执行安装脚本：

```bash
./install_webrtc_streamer.sh
```

程序支持从项目根目录运行 `./src/test`，也支持进入 `src/` 后运行 `./test`。

## 验证

执行以下命令检查安装脚本语法、编译主程序，并运行“缺少 webrtc-streamer”回归测试：

```bash
sh -n install_webrtc_streamer.sh
gcc -std=c11 -Wall -Wextra -Wpedantic -o /tmp/webrtc-camera src/test.c
gcc -std=c11 -Wall -Wextra -Wpedantic \
  -o /tmp/test-missing-streamer tests/test_missing_streamer.c
/tmp/test-missing-streamer
```

完整的视频链路测试仍需要真实的 `/dev/video4` 摄像头。

## 项目结构

```text
.
├── install_webrtc_streamer.sh   # 下载并校验 webrtc-streamer
├── src/
│   └── test.c                   # 摄像头检测与服务编排程序
└── tests/
    └── test_missing_streamer.c  # 缺失依赖时的回归测试
```

## 安全说明与限制

- 当前服务监听所有网络接口，且没有访问认证；同一网络中能访问 `8000` 端口的设备可能看到视频。
- HTTP 页面未配置 TLS。请只在可信局域网使用，并通过主机防火墙限制访问来源。
- 不建议直接暴露到公网。公网使用需要额外配置 HTTPS、身份认证和合适的 WebRTC NAT 穿透方案，这些不在本项目范围内。
- 当前摄像头路径、RTSP 地址和端口均为代码内固定配置。
- 当前只传输视频，不传输音频。
