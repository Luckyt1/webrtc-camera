#define _XOPEN_SOURCE 700

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
#include <limits.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

static pid_t streamer_pid = -1;
static pid_t vlc_pid = -1;
static volatile sig_atomic_t stop_requested = 0;
static const char* camera_device = "/dev/video4";
static const char* camera_source = "rtsp://127.0.0.1:8554/video4";

static void wait_100ms(void) {
    struct timespec delay = {0, 100000000};
    nanosleep(&delay, NULL);
}

static void stop_process(pid_t* pid) {
    if (*pid <= 0) {
        return;
    }

    kill(*pid, SIGTERM);
    for (int i = 0; i < 20; i++) {
        pid_t result = waitpid(*pid, NULL, WNOHANG);
        if (result == *pid || result == -1) {
            *pid = -1;
            return;
        }
        wait_100ms();
    }

    kill(*pid, SIGKILL);
    waitpid(*pid, NULL, 0);
    *pid = -1;
}

static int port_is_available(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return 0;
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);
    int available = bind(fd, (struct sockaddr*)&address, sizeof(address)) == 0;
    close(fd);
    return available;
}

static int wait_for_service(int port, const char* request, const char* expected, pid_t pid) {
    for (int attempt = 0; attempt < 50; attempt++) {
        if (waitpid(pid, NULL, WNOHANG) == pid) {
            return -1;
        }

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd != -1) {
            struct timeval timeout = {0, 200000};
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            struct sockaddr_in address = {0};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons((unsigned short)port);

            if (connect(fd, (struct sockaddr*)&address, sizeof(address)) == 0) {
                send(fd, request, strlen(request), MSG_NOSIGNAL);
                char response[8192];
                size_t size = 0;
                ssize_t received;
                while (size < sizeof(response) - 1 &&
                       (received = recv(fd, response + size, sizeof(response) - 1 - size, 0)) > 0) {
                    size += (size_t)received;
                }
                response[size] = '\0';
                close(fd);
                if (strstr(response, expected) != NULL) {
                    return 0;
                }
            } else {
                close(fd);
            }
        }
        wait_100ms();
    }
    return -1;
}

static const char* find_webrtc_streamer(char* resolved, char* webroot, size_t webroot_size) {
    const char* candidates[] = {
        ".webrtc-streamer/webrtc-streamer",
        "../.webrtc-streamer/webrtc-streamer"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (access(candidates[i], X_OK) == 0 && realpath(candidates[i], resolved) != NULL) {
            char* slash = strrchr(resolved, '/');
            if (slash != NULL) {
                *slash = '\0';
                snprintf(webroot, webroot_size, "%s/html", resolved);
                *slash = '/';
            }
            return resolved;
        }
    }

    return NULL;
}

static int launch_vlc(int width, int height, int fps) {
    char camera_url[PATH_MAX];
    char width_option[32];
    char height_option[32];
    char fps_option[32];

    snprintf(camera_url, sizeof(camera_url), "v4l2://%s", camera_device);
    snprintf(width_option, sizeof(width_option), ":v4l2-width=%d", width);
    snprintf(height_option, sizeof(height_option), ":v4l2-height=%d", height);
    snprintf(fps_option, sizeof(fps_option), ":v4l2-fps=%d", fps);

    if (!port_is_available(8554)) {
        printf("无法启动 VLC: 端口 8554 已被占用\n");
        return -1;
    }

    vlc_pid = fork();
    if (vlc_pid == -1) {
        perror("启动 VLC 失败");
        return -1;
    }

    if (vlc_pid == 0) {
        execlp("cvlc", "cvlc", "-I", "dummy", camera_url,
               width_option, height_option, fps_option,
               "--sout", "#transcode{vcodec=h264,vb=800,acodec=none}:rtp{sdp=rtsp://127.0.0.1:8554/video4}",
               "--live-caching=50", "--sout-rtp-caching=50",
               "--sout-x264-profile=baseline", "--sout-x264-preset=ultrafast",
               "--sout-x264-tune=zerolatency", "--sout-x264-keyint=30",
               "--sout-x264-min-keyint=30", "--sout-x264-bframes=0",
               "--sout-x264-options", "{repeat-headers=1,annexb=1}",
               "--sout-x264-ref=1", "--sout-keep", (char*)NULL);
        perror("无法执行 cvlc");
        _exit(127);
    }

    const char* request = "DESCRIBE rtsp://127.0.0.1:8554/video4 RTSP/1.0\r\nCSeq: 1\r\nAccept: application/sdp\r\n\r\n";
    if (wait_for_service(8554, request, "m=video", vlc_pid) != 0) {
        stop_process(&vlc_pid);
        printf("VLC 视频流启动失败，请确认已安装 VLC 且摄像头未被占用\n");
        return -1;
    }

    return 0;
}

static int launch_webrtc_streamer(void) {
    char resolved[PATH_MAX];
    char webroot[PATH_MAX];
    const char* executable = find_webrtc_streamer(resolved, webroot, sizeof(webroot));

    if (executable == NULL) {
        printf("找不到 webrtc-streamer，请先运行: ./install_webrtc_streamer.sh\n");
        return -1;
    }

    if (!port_is_available(8000)) {
        printf("无法启动 webrtc-streamer: 端口 8000 已被占用\n");
        return -1;
    }

    streamer_pid = fork();
    if (streamer_pid == -1) {
        perror("启动 webrtc-streamer 失败");
        return -1;
    }

    if (streamer_pid == 0) {
        char* args[12];
        int i = 0;

        args[i++] = (char*)executable;
        args[i++] = "-a10";
        args[i++] = "-H";
        args[i++] = "0.0.0.0:8000";
        if (webroot[0] != '\0') {
            args[i++] = "-w";
            args[i++] = webroot;
        }
        args[i++] = "-n";
        args[i++] = (char*)camera_device;
        args[i++] = "-u";
        args[i++] = (char*)camera_source;
        args[i] = NULL;

        execv(executable, args);
        perror("无法执行 webrtc-streamer");
        _exit(127);
    }

    const char* request = "GET /api/version HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
    if (wait_for_service(8000, request, "200 OK", streamer_pid) != 0) {
        stop_process(&streamer_pid);
        printf("webrtc-streamer HTTP 服务启动失败\n");
        return -1;
    }

    return 0;
}

static void stop_webrtc_streamer(void) {
    stop_process(&streamer_pid);
}

static void stop_vlc(void) {
    stop_process(&vlc_pid);
}

static void request_stop(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

int start_webrtc_streamer(int width, int height, int fps) {
    printf("启动 VLC 摄像头转码服务...\n");
    if (launch_vlc(width, height, fps) != 0) {
        return -1;
    }

    printf("启动WebRTC流媒体服务器...\n");
    if (launch_webrtc_streamer() == 0) {
        printf("WebRTC流媒体服务器启动成功!\n");
        return 0;
    } else {
        stop_vlc();
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
	struct sigaction stop_action = {0};
	stop_action.sa_handler = request_stop;
	sigemptyset(&stop_action.sa_mask);
	sigaction(SIGINT, &stop_action, NULL);
	sigaction(SIGTERM, &stop_action, NULL);

	int width = 640;
	int height = 480;
	int fps = 30;
	
	// 解析命令行参数
	if(argc >= 2) {
		width = atoi(argv[1]);
	}
	if(argc >= 3) {
		height = atoi(argv[2]);
	}
	if(argc >= 4) {
		fps = atoi(argv[3]);
	}
	if(argc > 4 || width <= 0 || height <= 0 || fps <= 0) {
		printf("用法: %s [宽度] [高度] [帧率]\n", argv[0]);
		printf("固定摄像头: %s\n", camera_device);
		printf("示例: %s 1280 720 30\n", argv[0]);
		printf("默认: %s 640 480 30\n", argv[0]);
		return -1;
	}
	
	printf("摄像头: %s\n", camera_device);
	printf("分辨率: %dx%d, 帧率: %d fps\n", width, height, fps);
	
	// 检测和配置摄像头设备
	printf("\n开始检测摄像头设备...\n");
	
	printf("正在检测摄像头: %s\n", camera_device);
	int fd = open(camera_device, O_RDWR);
	if(fd < 0)
	{
		printf("打开设备失败: %s\n", camera_device);
		perror("错误");
		return -1;
	}
	printf("设备打开成功!\n");

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

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		printf("错误: 设备不支持视频捕获\n");
		close(fd);
		return -1;
	}
	printf("设备支持视频捕获功能\n");

	printf("配置摄像头参数...\n");
	if (set_camera_format(fd, width, height, V4L2_PIX_FMT_MJPEG) == -1) {
		printf("MJPEG格式设置失败，尝试YUYV格式...\n");
		if (set_camera_format(fd, width, height, V4L2_PIX_FMT_YUYV) == -1) {
			printf("无法设置设备的任何支持格式\n");
			close(fd);
			return -1;
		}
	}

	struct v4l2_fmtdesc fmtdesc;
	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.index = 0;
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	printf("\n摄像头支持的视频格式:\n");
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

	close(fd);
	
	// 启动webrtc-streamer
	printf("\n准备启动WebRTC流媒体服务...\n");
	int start_result = start_webrtc_streamer(width, height, fps);
	
	if (start_result == 0) {
		printf("\n服务启动成功！\n");
		printf("您可以通过以下方式访问视频流：\n");
		printf("1. 本机访问: http://localhost:8000/?video=%%2Fdev%%2Fvideo4&options=rtptransport%%3Dudp\n");
		printf("2. 其他设备访问时，请将 localhost 替换为运行程序电脑的局域网 IP\n");
		printf("3. 按Ctrl+C停止服务\n\n");
		
		// 保持程序运行，等待用户中断
		printf("按任意键停止服务...\n");
		if (!stop_requested) {
			getchar();
		}
		stop_webrtc_streamer();
		stop_vlc();
	} else {
		return 1;
	}
	
	printf("程序结束\n");
	return 0;
}
