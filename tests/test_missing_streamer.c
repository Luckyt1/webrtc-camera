#define _XOPEN_SOURCE 700

#include <assert.h>

#define main camera_main
#include "../src/test.c"
#undef main

int main(void) {
    char temp_dir[] = "/tmp/webrtc-camera-test.XXXXXX";

    assert(strcmp(camera_device, "/dev/video4") == 0);
    assert(strcmp(camera_source, "rtsp://127.0.0.1:8554/video4") == 0);
    assert(mkdtemp(temp_dir) != NULL);
    assert(chdir(temp_dir) == 0);
    assert(setenv("PATH", "/nonexistent", 1) == 0);
    assert(launch_webrtc_streamer() == -1);
    assert(streamer_pid == -1);
    assert(chdir("/") == 0);
    assert(rmdir(temp_dir) == 0);
    return 0;
}
