#!/bin/bash

echo "正在安装webrtc-streamer..."

# 创建临时目录
mkdir -p /tmp/webrtc-streamer
cd /tmp/webrtc-streamer

# 下载webrtc-streamer (Linux版本)
echo "下载webrtc-streamer..."
wget https://github.com/mpromonet/webrtc-streamer/releases/download/v0.6.4/webrtc-streamer-v0.6.4-Linux-x86_64-Release.tar.gz

# 解压
echo "解压文件..."
tar -xzf webrtc-streamer-v0.6.4-Linux-x86_64-Release.tar.gz

# 移动到系统目录
echo "安装到系统目录..."
sudo cp webrtc-streamer /usr/local/bin/
sudo chmod +x /usr/local/bin/webrtc-streamer

# 清理临时文件
cd /
rm -rf /tmp/webrtc-streamer

echo "webrtc-streamer安装完成!"
echo "您现在可以运行: ./test 来启动摄像头流媒体服务"
