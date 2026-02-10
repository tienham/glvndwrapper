#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdint.h>

#define MALI_VENDOR_NAME "mali"

#ifndef PFNGLGETSTRINGPROC
typedef const GLubyte *(GL_APIENTRYP PFNGLGETSTRINGPROC)(GLenum name);
#endif
