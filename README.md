# 使用 webrtc-streamer 推送本地摄像头到局域网

## 安装与运行

```sh
./install_webrtc_streamer.sh
gcc -o src/test src/test.c
./src/test 640 480 30
```

程序固定展示 `/dev/video4`，三个可选参数依次为宽度、高度和帧率；默认值为 `640 480 30`。
程序需要系统已安装 `cvlc`，用于将 RealSense 的 `/dev/video4` 转成 WebRTC 可读取的视频源。
视频流默认使用低延迟 H.264 配置和约 800 kbps 码率，并在关键帧重复发送解码头，减少远程浏览时的卡顿和花屏。
本机浏览器访问 `http://localhost:8000/?video=%2Fdev%2Fvideo4&options=rtptransport%3Dudp`。其他电脑或手机必须把 `localhost` 换成运行程序电脑的局域网 IP，例如 `http://192.168.88.120:8000/?video=%2Fdev%2Fvideo4&options=rtptransport%3Dudp`。端口 `8000` 和本机端口 `8554` 必须空闲；按回车或 `Ctrl+C` 退出。

程序会自动启动和停止 VLC 与仓库内安装的 `webrtc-streamer`。
