#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <cstring>
#include <mutex>  
#include <cmath> 
#include <iomanip>
#include <csignal>
#include <atomic>

std::mutex cout_mutex;

struct Buffer {
	void* start;
	size_t length;
};

std::atomic<bool> keep_running{true};

void signal_handler(int signum){
	keep_running = false;
}

void grab_frames(int camera_index, const std::string& device_path) {
	int fd = open(device_path.c_str(), O_RDWR);
	if(fd < 0){
		std::lock_guard<std::mutex> lock(cout_mutex);
		std::cerr << "[Cam " << camera_index << "] ERROR: Failed to open " << device_path << "\n";
		return;
	}
	
	struct v4l2_requestbuffers req = {0};
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	
	if(ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		std::lock_guard<std::mutex> lock(cout_mutex);
		std::cerr << "[Cam " << camera_index << "] ERROR: Failed to query buffer\n";
		close(fd);
		return;
	}
	
	std::vector<Buffer> buffers(req.count);
	
	for(size_t i = 0; i < req.count; i++) {
		struct v4l2_buffer buf = {0};
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		
		if(ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
			std::lock_guard<std::mutex> lock(cout_mutex);
			std::cerr << "[Cam " << camera_index << "] ERROR: Failed to query buffer\n";
			close(fd);
			return;
		}
		buffers[i].length = buf.length;
		buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
		
		if(buffers[i].start == MAP_FAILED) {
			std::lock_guard<std::mutex> lock(cout_mutex);
			std::cerr << "[Cam " << camera_index << "] ERROR: Failed to mmap\n";
			close(fd);
			return;
		}
		
		if(ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			std::lock_guard<std::mutex> lock(cout_mutex);
			std::cerr << "[Cam " << camera_index << "] ERROR: Failed to queue buffer\n";
			close(fd);
			return;
		}
	}
	
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
		std::lock_guard<std::mutex> lock(cout_mutex);
		std::cerr << "[Cam " << camera_index << "] ERROR: Failed to start stream\n";
		close(fd);
		return;
	}
	
	// ---- TIMESTAMP DROP TRACKER INITIALIZATION ----
	double last_timestamp_ms = -1.0;
	int local_frame_count = 0;
	int total_dropped_frames = 0;

	// Change this based on your camera profile (e.g., 33.33 for 30FPS, 50.0 for 20FPS)
	const double expected_interval_ms = 1000.0 / 30.0; 
	const double drop_threshold_ms = expected_interval_ms * 1.5; // ~50ms alert zone
	// ------------------------------------------------

	while (keep_running) {
		struct v4l2_buffer buf = {0};
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		
		if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
			std::lock_guard<std::mutex> lock(cout_mutex);
			std::cerr << "[Cam " << camera_index << "] ERROR: Failed to dequeue buffer\n";
			break;
		}
		
		local_frame_count++;

		// Convert the v4l2 timeval struct (seconds + microseconds) into absolute milliseconds
		double current_timestamp_ms = (buf.timestamp.tv_sec * 1000.0) + (buf.timestamp.tv_usec / 1000.0);
		
		int frames_dropped_this_turn = 0;
		double frame_gap_ms = 0.0;

		if (last_timestamp_ms > 0) {
			frame_gap_ms = current_timestamp_ms - last_timestamp_ms;
			
			// If the gap between frames is significantly longer than 33.3ms, a drop occurred
			if (frame_gap_ms > drop_threshold_ms) {
				frames_dropped_this_turn = std::round(frame_gap_ms / expected_interval_ms) - 1;
				total_dropped_frames += frames_dropped_this_turn;
			}
		}
		last_timestamp_ms = current_timestamp_ms;

		// Safe thread print block
		//if (local_frame_count == 1 || local_frame_count % 1800 == 0 || frames_dropped_this_turn > 0){
		{
			std::lock_guard<std::mutex> lock(cout_mutex);
			std::cout << "[Cam " << camera_index 
			          << "] Recv#" << local_frame_count;
			
			if (frame_gap_ms > 0) {
				std::cout << " | Gap: " << frame_gap_ms << " ms";
			}

			if (frames_dropped_this_turn > 0) {
				std::cout << "  ❌ [⚠️ DROP DETECTED! Skipped " << frames_dropped_this_turn << " frame(s)]";
			}
			
			std::cout << " | Timestamp: " << buf.timestamp.tv_sec << "." << std::setfill('0') << std::setw(6) << buf.timestamp.tv_usec << "\n";
		}
		
		if(ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			std::lock_guard<std::mutex> lock(cout_mutex);
			std::cerr << "[Cam " << camera_index << "] ERROR: Failed to re-queue buffer\n";
			break;
		}
	}
	
	ioctl(fd, VIDIOC_STREAMOFF, &type);
	for(size_t i = 0; i < req.count; i++) {
		munmap(buffers[i].start, buffers[i].length);
	}
	close(fd);

	// Final Thread Summary
	{
		std::lock_guard<std::mutex> lock(cout_mutex);
		std::cout << ">> [Cam " << camera_index << "] Finished. Recv: " << local_frame_count 
		          << " | Total Dropped by Timing: " << total_dropped_frames << "\n";
	}
}

int main() {
	std::cout << "Starting 4-Camera Threaded Capture...\n";
	
	std::signal(SIGINT, signal_handler);
	
	std::thread t0(grab_frames, 100, "/dev/video100");
	std::thread t1(grab_frames, 101, "/dev/video101");
	std::thread t2(grab_frames, 102, "/dev/video102");
	std::thread t3(grab_frames, 103, "/dev/video103");
	
	cpu_set_t cpuset0, cpuset1, cpuset2, cpuset3;
	CPU_ZERO(&cpuset0); CPU_SET(0, &cpuset0); 
	CPU_ZERO(&cpuset1); CPU_SET(1, &cpuset1); 
	CPU_ZERO(&cpuset2); CPU_SET(2, &cpuset2);
	CPU_ZERO(&cpuset3); CPU_SET(3, &cpuset3);
	
	pthread_setaffinity_np(t0.native_handle(), sizeof(cpu_set_t), &cpuset0);
	pthread_setaffinity_np(t1.native_handle(), sizeof(cpu_set_t), &cpuset1);
	pthread_setaffinity_np(t2.native_handle(), sizeof(cpu_set_t), &cpuset2);
	pthread_setaffinity_np(t3.native_handle(), sizeof(cpu_set_t), &cpuset3);
	
	t0.join();
	t1.join();
	t2.join();
	t3.join();
	
	std::cout << "Capture Complete. Safe to close.\n";
	return 0;
}
