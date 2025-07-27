#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/wait.h>

// 启动webrtc-streamer的函数
int start_webrtc_streamer(const char* device, int width, int height, int fps) {
    char command[512];
    snprintf(command, sizeof(command), 
        "webrtc-streamer -H 0.0.0.0:8000 -v /dev/video0=%dx%d@%d &", 
        width, height, fps);
    
    printf("启动WebRTC流媒体服务器...\n");
    printf("命令: %s\n", command);
    
    int result = system(command);
    if (result == 0) {
        printf("WebRTC流媒体服务器启动成功!\n");
        printf("请在浏览器中访问: http://localhost:8000\n");
        return 0;
    } else {
        printf("WebRTC流媒体服务器启动失败\n");
        return -1;
    }
}

// 设置摄像头格式的函数
int set_camera_format(int fd, int width, int height, unsigned int pixelformat) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("设置视频格式失败");
        return -1;
    }
    
    printf("视频格式设置成功: %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    return 0;
}

int main(int argc, char**argv)
{
	char* device = "/dev/video0";  // 默认设备
	int width = 640;
	int height = 480;
	int fps = 30;
	
	// 解析命令行参数
	if(argc >= 2) {
		device = argv[1];
	}
	if(argc >= 4) {
		width = atoi(argv[2]);
		height = atoi(argv[3]);
	}
	if(argc >= 5) {
		fps = atoi(argv[4]);
	}
	
	if(argc > 5) {
		printf("用法: %s [设备] [宽度] [高度] [帧率]\n", argv[0]);
		printf("示例: %s /dev/video0 1280 720 30\n", argv[0]);
		printf("默认: %s /dev/video0 640 480 30\n", argv[0]);
		return -1;
	}
	
	printf("正在配置摄像头设备: %s\n", device);
	printf("分辨率: %dx%d, 帧率: %d fps\n", width, height, fps);
	
	int fd = open(device, O_RDWR);
	if(fd < 0)
	{
		perror("打开设备失败");
		return -1;
	}

	printf("设备打开成功!\n");

	// 获取摄像头设备信息
	struct v4l2_capability cap;
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) 
	{
		perror("获取设备信息失败");
		close(fd);
		return -1;
	}

	printf("\n设备信息:\n");
	printf("驱动名称: %s\n", cap.driver);
	printf("设备名称: %s\n", cap.card);
	printf("总线信息: %s\n", cap.bus_info);
	printf("版本: %u.%u.%u\n", 
		(cap.version >> 16) & 0xFF,
		(cap.version >> 8) & 0xFF,
		cap.version & 0xFF);

	// 检查设备是否支持视频捕获
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
	{
		printf("错误: 设备不支持视频捕获\n");
		close(fd);
		return -1;
	}

	printf("设备支持视频捕获功能\n");

	// 设置摄像头格式
	printf("\n配置摄像头参数...\n");
	if (set_camera_format(fd, width, height, V4L2_PIX_FMT_MJPEG) == -1) {
		// 如果MJPEG失败，尝试YUYV
		printf("MJPEG格式设置失败，尝试YUYV格式...\n");
		if (set_camera_format(fd, width, height, V4L2_PIX_FMT_YUYV) == -1) {
			printf("无法设置任何支持的格式\n");
			close(fd);
			return -1;
		}
	}

	// 查询支持的视频格式
	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index = 0;
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	printf("\n支持的视频格式:\n");
	while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) 
	{
		printf("  %d. %s (fourcc: %c%c%c%c)\n", 
			fmtdesc.index,
			fmtdesc.description,
			fmtdesc.pixelformat & 0xFF,
			(fmtdesc.pixelformat >> 8) & 0xFF,
			(fmtdesc.pixelformat >> 16) & 0xFF,
			(fmtdesc.pixelformat >> 24) & 0xFF);
		fmtdesc.index++;
	}

	// 关闭设备文件描述符
	close(fd);
	
	// 启动webrtc-streamer
	printf("\n准备启动WebRTC流媒体服务...\n");
	if (start_webrtc_streamer(device, width, height, fps) == 0) {
		printf("\n服务启动成功！\n");
		printf("您可以通过以下方式访问视频流：\n");
		printf("1. 浏览器访问: http://localhost:8000\n");
		printf("2. WebRTC客户端连接到: ws://localhost:8000/webrtc\n");
		printf("3. 按Ctrl+C停止服务\n\n");
		
		// 保持程序运行，等待用户中断
		printf("按任意键停止服务...\n");
		getchar();
	}
	
	printf("程序结束\n");
	return 0;
}