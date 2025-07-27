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

// 启动webrtc-streamer的函数（支持多个摄像头）
int start_webrtc_streamer_dual(const char* device1, const char* device2, int width, int height, int fps) {
    char command[1024];
    
    // 构建支持两个摄像头的命令
    snprintf(command, sizeof(command), 
        "webrtc-streamer -H 0.0.0.0:8000 -v %s=%dx%d@%d -v %s=%dx%d@%d &", 
        device1, width, height, fps,
        device2, width, height, fps);
    
    printf("启动双摄像头WebRTC流媒体服务器...\n");
    printf("命令: %s\n", command);
    
    int result = system(command);
    if (result == 0) {
        printf("WebRTC流媒体服务器启动成功!\n");
        printf("请在浏览器中访问: http://localhost:8000\n");
        printf("摄像头1: %s\n", device1);
        printf("摄像头2: %s\n", device2);
        return 0;
    } else {
        printf("WebRTC流媒体服务器启动失败\n");
        return -1;
    }
}

// 启动webrtc-streamer的函数（单个摄像头）
int start_webrtc_streamer(const char* device, int width, int height, int fps) {
    char command[512];
    snprintf(command, sizeof(command), 
        "webrtc-streamer -H 0.0.0.0:8000 -v %s=%dx%d@%d &", 
        device, width, height, fps);
    
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
	char* device1 = "/dev/video0";  // 默认设备1
	char* device2 = NULL;           // 默认设备2（可选）
	int width = 640;
	int height = 480;
	int fps = 60;
	int dual_camera = 0;           // 双摄像头模式标志
	
	// 解析命令行参数
	if(argc >= 2) {
		device1 = argv[1];
	}
	if(argc >= 3) {
		// 检查第二个参数是否是设备路径
		if(strstr(argv[2], "/dev/video") != NULL) {
			device2 = argv[2];
			dual_camera = 1;
			// 如果有第三、四、五个参数，它们是宽度、高度、帧率
			if(argc >= 5) {
				width = atoi(argv[3]);
				height = atoi(argv[4]);
			}
			if(argc >= 6) {
				fps = atoi(argv[5]);
			}
		} else {
			// 单摄像头模式，第二、三、四个参数是宽度、高度、帧率
			width = atoi(argv[2]);
			if(argc >= 4) {
				height = atoi(argv[3]);
			}
			if(argc >= 5) {
				fps = atoi(argv[4]);
			}
		}
	}
	
	if(argc > 6) {
		printf("用法: \n");
		printf("单摄像头: %s [设备] [宽度] [高度] [帧率]\n", argv[0]);
		printf("双摄像头: %s [设备1] [设备2] [宽度] [高度] [帧率]\n", argv[0]);
		printf("示例: \n");
		printf("  %s /dev/video0 1280 720 30\n", argv[0]);
		printf("  %s /dev/video0 /dev/video1 640 480 30\n", argv[0]);
		printf("默认: %s /dev/video0 640 480 60\n", argv[0]);
		return -1;
	}
	
	if(dual_camera) {
		printf("双摄像头模式\n");
		printf("摄像头1: %s\n", device1);
		printf("摄像头2: %s\n", device2);
	} else {
		printf("单摄像头模式\n");
		printf("摄像头: %s\n", device1);
	}
	printf("分辨率: %dx%d, 帧率: %d fps\n", width, height, fps);
	
	// 检测和配置摄像头设备
	printf("\n开始检测摄像头设备...\n");
	
	// 检测第一个摄像头
	printf("正在检测摄像头1: %s\n", device1);
	int fd1 = open(device1, O_RDWR);
	if(fd1 < 0)
	{
		printf("打开设备1失败: %s\n", device1);
		perror("错误");
		return -1;
	}
	printf("设备1打开成功!\n");

	// 获取摄像头1设备信息
	struct v4l2_capability cap1;
	if (ioctl(fd1, VIDIOC_QUERYCAP, &cap1) == -1) 
	{
		perror("获取设备1信息失败");
		close(fd1);
		return -1;
	}

	printf("\n设备1信息:\n");
	printf("驱动名称: %s\n", cap1.driver);
	printf("设备名称: %s\n", cap1.card);
	printf("总线信息: %s\n", cap1.bus_info);

	// 检查设备1是否支持视频捕获
	if (!(cap1.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
	{
		printf("错误: 设备1不支持视频捕获\n");
		close(fd1);
		return -1;
	}
	printf("设备1支持视频捕获功能\n");

	// 设置摄像头1格式
	printf("配置摄像头1参数...\n");
	if (set_camera_format(fd1, width, height, V4L2_PIX_FMT_MJPEG) == -1) {
		printf("MJPEG格式设置失败，尝试YUYV格式...\n");
		if (set_camera_format(fd1, width, height, V4L2_PIX_FMT_YUYV) == -1) {
			printf("无法设置设备1的任何支持格式\n");
			close(fd1);
			return -1;
		}
	}

	// 如果是双摄像头模式，检测第二个摄像头
	int fd2 = -1;
	if(dual_camera && device2) {
		printf("\n正在检测摄像头2: %s\n", device2);
		fd2 = open(device2, O_RDWR);
		if(fd2 < 0)
		{
			printf("警告: 打开设备2失败: %s\n", device2);
			perror("错误");
			printf("将以单摄像头模式运行\n");
			dual_camera = 0;
		} else {
			printf("设备2打开成功!\n");

			// 获取摄像头2设备信息
			struct v4l2_capability cap2;
			if (ioctl(fd2, VIDIOC_QUERYCAP, &cap2) == -1) 
			{
				perror("获取设备2信息失败");
				close(fd2);
				dual_camera = 0;
			} else {
				printf("\n设备2信息:\n");
				printf("驱动名称: %s\n", cap2.driver);
				printf("设备名称: %s\n", cap2.card);
				printf("总线信息: %s\n", cap2.bus_info);

				// 检查设备2是否支持视频捕获
				if (!(cap2.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
				{
					printf("警告: 设备2不支持视频捕获\n");
					close(fd2);
					dual_camera = 0;
				} else {
					printf("设备2支持视频捕获功能\n");

					// 设置摄像头2格式
					printf("配置摄像头2参数...\n");
					if (set_camera_format(fd2, width, height, V4L2_PIX_FMT_MJPEG) == -1) {
						printf("MJPEG格式设置失败，尝试YUYV格式...\n");
						if (set_camera_format(fd2, width, height, V4L2_PIX_FMT_YUYV) == -1) {
							printf("警告: 无法设置设备2的任何支持格式\n");
							close(fd2);
							dual_camera = 0;
						}
					}
				}
			}
		}
	}

	// 显示支持的视频格式（使用第一个摄像头）
	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index = 0;
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	printf("\n摄像头1支持的视频格式:\n");
	while (ioctl(fd1, VIDIOC_ENUM_FMT, &fmtdesc) == 0) 
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
	close(fd1);
	if(fd2 >= 0) {
		close(fd2);
	}
	
	// 启动webrtc-streamer
	printf("\n准备启动WebRTC流媒体服务...\n");
	int start_result;
	if(dual_camera && device2) {
		start_result = start_webrtc_streamer_dual(device1, device2, width, height, fps);
	} else {
		start_result = start_webrtc_streamer(device1, width, height, fps);
	}
	
	if (start_result == 0) {
		printf("\n服务启动成功！\n");
		printf("您可以通过以下方式访问视频流：\n");
		printf("1. 浏览器访问: http://localhost:8000\n");
		if(dual_camera) {
			printf("   - 摄像头1流: http://localhost:8000/?video=%s\n", device1);
			printf("   - 摄像头2流: http://localhost:8000/?video=%s\n", device2);
		}
		printf("2. WebRTC客户端连接到: ws://localhost:8000/webrtc\n");
		printf("3. 按Ctrl+C停止服务\n\n");
		
		// 保持程序运行，等待用户中断
		printf("按任意键停止服务...\n");
		getchar();
	}
	
	printf("程序结束\n");
	return 0;
}