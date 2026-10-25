#include <stdio.h>
#include <cstddef>
#include <cstdint>
#include <algorithm>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>

#include <SDL2/SDL.h>

typedef struct notify_request {
  char useless1[45];
  char message[3075];
} notify_request_t;

extern "C" {
   int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);
   int sceSystemServiceHideSplashScreen();
   int sceVideoOutOpen(std::int32_t user_id, std::int32_t bus_type, std::int32_t index,
                       const void *param);
   int sceVideoOutSetFlipRate(std::int32_t handle, std::int32_t rate);
   int sceVideoOutSubmitFlip(std::int32_t handle, std::int32_t buffer_index,
                             std::uint32_t flip_mode, std::int64_t flip_argument);
   int sceVideoOutWaitVblank(std::int32_t handle);
}

const char * VertexShaderSource =
#include "shaders/vertex.glsl"
;

const char * FragmentShaderSource =
#include "shaders/fragment.glsl"
;

struct Vertex {
   float x;
   float y;
   float z;
   float r;
   float g;
   float b;
   float a;
};

static constexpr Vertex TriangleVerts[] = {
    {  0.0f,  0.5f, 0.0f,        1.0f,   0,   0, 1.0f },
    { -0.5f, -0.5f, 0.0f,          0, 1.0f,   0, 1.0f },
    {  0.5f, -0.5f, 0.0f,          0,   0, 1.0f, 1.0f },
};

static GLuint CompileShader(int type, const char * shaderSource) {
   GLuint shader = glCreateShader(type);
   GLint success = 0;
   glShaderSource(shader, 1, &shaderSource, nullptr);
   glCompileShader(shader);
   glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
   if(!success) {
      notify_request_t debugMessage = {};
      int len;

      sprintf(debugMessage.message, "Failed to compile shader:");
      sceKernelSendNotificationRequest(0, &debugMessage, sizeof(debugMessage), 0);

      glGetShaderInfoLog(shader, sizeof(debugMessage.message), &len, debugMessage.message);
      sceKernelSendNotificationRequest(0, &debugMessage, sizeof(debugMessage), 0);

      return static_cast<GLuint>(-1);
   }

   return shader;
} 

int main() {
   EGLSurface surface = EGL_NO_SURFACE;
   EGLConfig config = NULL; 
   EGLint count = 0;
   notify_request_t debugMessage = {};
   SDL_GLContext context;

   SDL_Init(SDL_INIT_VIDEO);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetSwapInterval(1);

   SDL_Window * window = SDL_CreateWindow(
       "ps5", 0, 0, 1920, 1080,
       SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL );

   context = SDL_GL_CreateContext(window);

   GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, VertexShaderSource);
   GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, FragmentShaderSource);

   GLuint program = glCreateProgram();

   glAttachShader(program, vertexShader);
   glAttachShader(program, fragmentShader);
   glLinkProgram(program);
   
   GLint linkSuccess = 0;
   glGetProgramiv(program, GL_LINK_STATUS, &linkSuccess);

   if(!linkSuccess) {
      sprintf(debugMessage.message, "Failed to link shader");
      sceKernelSendNotificationRequest(0, &debugMessage, sizeof(debugMessage), 0);
   }

   glUseProgram(program);

   GLuint vbo, vao;
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);

   // Position
   glEnableVertexAttribArray(0); 
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
   
   // Color
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

   glBufferData(GL_ARRAY_BUFFER,sizeof(TriangleVerts),TriangleVerts,GL_DYNAMIC_DRAW);
   glDisable(GL_DEPTH_TEST);

   while(true) {
      glClear(GL_COLOR_BUFFER_BIT);
      glDrawArrays(GL_TRIANGLES, 0, sizeof(TriangleVerts) / sizeof(Vertex));
      SDL_GL_SwapWindow(window);
   }

   return 0;
}