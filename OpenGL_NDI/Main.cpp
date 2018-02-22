#include <csignal>
#include <cstddef>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>

#include <GL/glew.h>
#include <GL/glfw3.h>
#include <Processing.NDI.Lib.h>


#define WIDTH 1280
#define HEIGHT 720

static GLFWwindow* window;
static GLuint color_buffer;
static GLuint fbo;


static std::atomic<bool> exit_loop(false);	// signal handler

static void sigint_handler(int)
{	
	exit_loop = true;
}

void generate_buffers() {
	// Color buffer
	glGenTextures(1, &color_buffer);
	glBindTexture(GL_TEXTURE_2D, color_buffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);

	// Frame buffer
	glGenFramebuffersEXT(1, &fbo);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, color_buffer, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

bool initialize() {

	// GLFW
	if (glfwInit() == GL_FALSE) {
		std::cerr << "Can't initilize GLFW" << std::endl;
		return false;
	}

	window = glfwCreateWindow(WIDTH, HEIGHT, "GLFW NDI Receiver Sample", NULL, NULL);
	if (window == nullptr) {
		std::cerr << "Can't create GLFW window." << std::endl;
		glfwTerminate();
		return false;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);


	// GLEW
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		std::cerr << "Can't initilize GLEW" << std::endl;
		return false;
	}

	// GL
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, WIDTH, 0.0f, HEIGHT, -1.0f, 1.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// set vertex position
	static const GLfloat vtx[] = {
		0, 0,
		WIDTH, 0,
		WIDTH, HEIGHT,
		0, HEIGHT,
	};
	glVertexPointer(2, GL_FLOAT, 0, vtx);

	// set uv coordinate
	static const GLfloat texuv[] = {
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
	};
	glTexCoordPointer(2, GL_FLOAT, 0, texuv);



	// Create buffers
	generate_buffers();
	


	// NDI
	if (!NDIlib_initialize()) {
		// NDIlib_is_supported_CPU()関数で、対応しているCPUであるかを調査できる。
		printf("Cannot run NDI.");
		return false;
	}


	return true;
}

NDIlib_recv_instance_t NDIProcess() {

	// Create source finder
	const NDIlib_find_create_t NDI_find_create_desc; /* Use defaults */
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2(&NDI_find_create_desc);
	if (!pNDI_find) return NULL;

	// 最低限1つのソースが見つかるまで待機
	uint32_t no_sources = 0;
	const NDIlib_source_t* p_sources = NULL;
	while (!exit_loop && !no_sources) {	// Wait until the sources on the nwtork have changed
		NDIlib_find_wait_for_sources(pNDI_find, 1000);
		p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
	}
	if (!p_sources) return NULL;

	// ソースが発見された場合の処理 -> レシーバーを作成し、ソースをルックアップする
	// YCbCrで受信するのが好ましい。
	// ソースがアルファチャンネルを持っている場合、BGRAで引き続き提供される。
	NDIlib_recv_create_t NDI_recv_create_desc;
	NDI_recv_create_desc.source_to_connect_to = p_sources[0];	// ソース0番を参照

	NDIlib_recv_instance_t pNDI_recv = NDIlib_recv_create_v2(&NDI_recv_create_desc);
	if (!pNDI_recv) return NULL;

	// Destroy NDI finder.
	// We needed to have access to the pointers to p_sources[0]
	NDIlib_find_destroy(pNDI_find);

	// We are now going to mark this source as being on program output for tally purposes (but not on preview)
	NDIlib_tally_t tally_state;
	tally_state.on_program = true;
	tally_state.on_preview = true;
	NDIlib_recv_set_tally(pNDI_recv, &tally_state);

	// Enable Hardwqre Decompression support if this support has it. Please read the caveats in the documentation
	// regarding this. There are times in which it might reduce the performance although on small stream numbers
	// it almost always yields the same or better performance.
	NDIlib_metadata_frame_t enable_hw_accel;
	enable_hw_accel.p_data = (char*)"<ndi_hwaccel enabled=\"true\"/>";
	NDIlib_recv_send_metadata(pNDI_recv, &enable_hw_accel);

	return pNDI_recv;
}

void video_process(const NDIlib_video_frame_v2_t& video_frame) {
	//std::cout << " --------------------------------- " << std::endl;
	//std::cout << "Video data received : (" << video_frame.xres << ", " << video_frame.yres << ")" << std::endl;
	//std::cout << "Bandwidth (High) : " << NDIlib_recv_bandwidth_highest << std::endl;
	//std::cout << "Bandwidth (Low) : " << NDIlib_recv_bandwidth_lowest << std::endl;
	
	//std::string color_mode = "";
	//switch (video_frame.FourCC) {
	//case NDIlib_FourCC_type_BGRA:
	//	std::cout << "Color Mode : BGRA" << std::endl;
	//	color_mode = "BGRA";
	//	break;
	//case NDIlib_FourCC_type_UYVY:
	//	std::cout << "Color Mode : UYVY" << std::endl;
	//	color_mode = "UYVY";
	//	break;
	//case NDIlib_FourCC_type_BGRX:
	//	std::cout << "Color Mode : BGRX" << std::endl;
	//	color_mode = "BGRX";
	//	break;
	//default:
	//	break;
	//}

	//std::cout << "Data[1082] (" << color_mode << ") : [" << unsigned(video_frame.p_data[0]) << ", " << unsigned(video_frame.p_data[1]) << ", " << unsigned(video_frame.p_data[2]) << ", " << unsigned(video_frame.p_data[3]) << "]" << std::endl;
	//std::cout << "Line stride : " << video_frame.line_stride_in_bytes << std::endl;
	

	// set data to color buffer
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_BGRA, GL_UNSIGNED_BYTE, &video_frame.p_data[0]);

	glBindTexture(GL_TEXTURE_2D, color_buffer);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDrawArrays(GL_QUADS, 0, 4);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_TEXTURE_2D);
}


int main(int argc, char* argv[])
{	

	if (!initialize()) {
		exit(EXIT_FAILURE);
	}
	
	signal(SIGINT, sigint_handler);

	NDIlib_recv_instance_t receiver = NDIProcess();
	if (receiver == NULL) return 0;
	
	while (glfwWindowShouldClose(window) == GL_FALSE) {

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		NDIlib_video_frame_v2_t video_frame;
		NDIlib_audio_frame_v2_t audio_frame;
		NDIlib_metadata_frame_t metadata_frame;

		switch (NDIlib_recv_capture_v2(receiver, &video_frame, &audio_frame, &metadata_frame, 1000)) {
		case NDIlib_frame_type_none:
			printf("No data received.\n");
			break;
		case NDIlib_frame_type_video:
			video_process(video_frame);
			NDIlib_recv_free_video_v2(receiver, &video_frame);
			break;

			// Audio data
		case NDIlib_frame_type_audio:
			printf("Audio data received (%d samples).\n", audio_frame.no_samples);
			NDIlib_recv_free_audio_v2(receiver, &audio_frame);
			break;

			// Meta data
		case NDIlib_frame_type_metadata:
			printf("Meta data received.\n");
			NDIlib_recv_free_metadata(receiver, &metadata_frame);
			break;

			// There is a status change on the receiver (e.g. new web interface)
		case NDIlib_frame_type_status_change:
			printf("Receiver connection status changed.\n");
			break;

		default:
			break;
		}


		glFlush();
		// glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);	// clear frame buffer

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// GL
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(GL_TEXTURE_2D, &color_buffer);

	// NDI
	NDIlib_recv_destroy(receiver);
	NDIlib_destroy();

	return 0;
}

