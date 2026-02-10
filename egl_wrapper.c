/*
 * Copyright (c) 2016, NVIDIA CORPORATION.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * unaltered in all copies or substantial portions of the Materials.
 * Any additions, deletions, or changes to the original source files
 * must be clearly indicated in accompanying documentation.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 */

#include "egl_wrapper.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glvnd/libeglabi.h"
#include "compiler.h"

static void *mali_handle = NULL;
static EGLBoolean mali_symbols_loaded = EGL_FALSE;

#define MALI_EGL_REQUIRED_FUNCS(X) \
    X(PFNEGLINITIALIZEPROC, eglInitialize) \
    X(PFNEGLTERMINATEPROC, eglTerminate) \
    X(PFNEGLCHOOSECONFIGPROC, eglChooseConfig) \
    X(PFNEGLGETCONFIGSPROC, eglGetConfigs) \
    X(PFNEGLCOPYBUFFERSPROC, eglCopyBuffers) \
    X(PFNEGLCREATECONTEXTPROC, eglCreateContext) \
    X(PFNEGLDESTROYCONTEXTPROC, eglDestroyContext) \
    X(PFNEGLCREATEPLATFORMWINDOWSURFACEPROC, eglCreatePlatformWindowSurface) \
    X(PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC, eglCreatePlatformPixmapSurface) \
    X(PFNEGLCREATEPBUFFERSURFACEPROC, eglCreatePbufferSurface) \
    X(PFNEGLCREATEPIXMAPSURFACEPROC, eglCreatePixmapSurface) \
    X(PFNEGLCREATEWINDOWSURFACEPROC, eglCreateWindowSurface) \
    X(PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC, eglCreatePbufferFromClientBuffer) \
    X(PFNEGLDESTROYSURFACEPROC, eglDestroySurface) \
    X(PFNEGLGETCONFIGATTRIBPROC, eglGetConfigAttrib) \
    X(PFNEGLMAKECURRENTPROC, eglMakeCurrent) \
    X(PFNEGLQUERYCONTEXTPROC, eglQueryContext) \
    X(PFNEGLQUERYSTRINGPROC, eglQueryString) \
    X(PFNEGLQUERYSURFACEPROC, eglQuerySurface) \
    X(PFNEGLSWAPBUFFERSPROC, eglSwapBuffers) \
    X(PFNEGLWAITGLPROC, eglWaitGL) \
    X(PFNEGLWAITNATIVEPROC, eglWaitNative) \
    X(PFNEGLBINDTEXIMAGEPROC, eglBindTexImage) \
    X(PFNEGLRELEASETEXIMAGEPROC, eglReleaseTexImage) \
    X(PFNEGLSURFACEATTRIBPROC, eglSurfaceAttrib) \
    X(PFNEGLSWAPINTERVALPROC, eglSwapInterval) \
    X(PFNEGLBINDAPIPROC, eglBindAPI) \
    X(PFNEGLRELEASETHREADPROC, eglReleaseThread) \
    X(PFNEGLWAITCLIENTPROC, eglWaitClient) \
    X(PFNEGLGETERRORPROC, eglGetError) \
    X(PFNEGLGETDISPLAYPROC, eglGetDisplay)

#define MALI_GL_REQUIRED_FUNCS(X) \
    X(PFNGLGETSTRINGPROC, glGetString)

#define DECLARE_FUNC(type, name) static type mali_##name##_ptr = NULL;
MALI_EGL_REQUIRED_FUNCS(DECLARE_FUNC)
MALI_GL_REQUIRED_FUNCS(DECLARE_FUNC)
#undef DECLARE_FUNC

static PFNEGLGETPROCADDRESSPROC mali_eglGetProcAddress_ptr = NULL;
static PFNEGLGETPLATFORMDISPLAYPROC mali_eglGetPlatformDisplay_ptr = NULL;
static PFNEGLGETPLATFORMDISPLAYEXTPROC mali_eglGetPlatformDisplayEXT_ptr = NULL;

typedef struct MaliProcAddressEntry {
    const char *name;
    void *addr;
} MaliProcAddressEntry;

static const MaliProcAddressEntry PROC_ADDRESSES[];

static void *LoadMaliSymbol(const char *name, EGLBoolean required)
{
    void *symbol = dlsym(mali_handle, name);
    if (symbol == NULL && required) {
        fprintf(stderr, "[EGL_mali] dlsym %s failed: %s\n", name, dlerror());
        abort();
    }
    return symbol;
}

static void LoadMali(void)
{
    if (mali_handle != NULL && mali_symbols_loaded == EGL_TRUE) {
        return;
    }

    if (mali_handle == NULL) {
        mali_handle = dlopen("libmali.so", RTLD_NOW | RTLD_GLOBAL);
        if (!mali_handle) {
            fprintf(stderr, "[EGL_mali] dlopen libmali.so failed: %s\n", dlerror());
            abort();
        }
    }

#define LOAD_REQUIRED(type, name) \
    mali_##name##_ptr = (type) LoadMaliSymbol(#name, EGL_TRUE);
    MALI_EGL_REQUIRED_FUNCS(LOAD_REQUIRED)
    MALI_GL_REQUIRED_FUNCS(LOAD_REQUIRED)
#undef LOAD_REQUIRED

    mali_eglGetProcAddress_ptr = (PFNEGLGETPROCADDRESSPROC)
        LoadMaliSymbol("eglGetProcAddress", EGL_FALSE);
    mali_eglGetPlatformDisplay_ptr = (PFNEGLGETPLATFORMDISPLAYPROC)
        LoadMaliSymbol("eglGetPlatformDisplay", EGL_FALSE);
    mali_eglGetPlatformDisplayEXT_ptr = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
        LoadMaliSymbol("eglGetPlatformDisplayEXT", EGL_FALSE);

    mali_symbols_loaded = EGL_TRUE;
}

static const char *maliGetVendorString(int name)
{
    LoadMali();
    if (name == __EGL_VENDOR_STRING_PLATFORM_EXTENSIONS) {
        const char *ext = mali_eglQueryString_ptr(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        return ext != NULL ? ext : "";
    }

    return NULL;
}

static EGLDisplay maliGetPlatformDisplay(EGLenum platform, void *native_display,
      const EGLAttrib *attrib_list)
{
    LoadMali();
    if (mali_eglGetPlatformDisplay_ptr != NULL) {
        return mali_eglGetPlatformDisplay_ptr(platform, native_display, attrib_list);
    }
    if (mali_eglGetPlatformDisplayEXT_ptr != NULL) {
        return mali_eglGetPlatformDisplayEXT_ptr(platform, native_display, attrib_list);
    }
    if (platform == EGL_NONE || platform == EGL_PLATFORM_DEVICE_EXT) {
        return mali_eglGetDisplay_ptr((EGLNativeDisplayType) native_display);
    }
    return EGL_NO_DISPLAY;
}

static EGLBoolean maliGetSupportsAPI(EGLenum api)
{
    if (api == EGL_OPENGL_ES_API || api == EGL_OPENGL_API) {
        return EGL_TRUE;
    }
    return EGL_FALSE;
}

static void *maliGetProcAddress(const char *procName)
{
    int i;

    LoadMali();
    for (i = 0; PROC_ADDRESSES[i].name != NULL; i++) {
        if (strcmp(procName, PROC_ADDRESSES[i].name) == 0) {
            return PROC_ADDRESSES[i].addr;
        }
    }
    if (mali_eglGetProcAddress_ptr != NULL) {
        return mali_eglGetProcAddress_ptr(procName);
    }
    return LoadMaliSymbol(procName, EGL_FALSE);
}

static EGLBoolean EGLAPIENTRY mali_eglInitialize(EGLDisplay dpy,
        EGLint *major, EGLint *minor)
{
    LoadMali();
    return mali_eglInitialize_ptr(dpy, major, minor);
}

static EGLBoolean EGLAPIENTRY mali_eglTerminate(EGLDisplay dpy)
{
    LoadMali();
    return mali_eglTerminate_ptr(dpy);
}

static EGLBoolean EGLAPIENTRY mali_eglChooseConfig(EGLDisplay dpy,
        const EGLint *attrib_list, EGLConfig *configs, EGLint config_size,
        EGLint *num_config)
{
    LoadMali();
    return mali_eglChooseConfig_ptr(dpy, attrib_list, configs, config_size, num_config);
}

static EGLBoolean EGLAPIENTRY mali_eglGetConfigs(EGLDisplay dpy, EGLConfig *configs,
        EGLint config_size, EGLint *num_config)
{
    LoadMali();
    return mali_eglGetConfigs_ptr(dpy, configs, config_size, num_config);
}

static EGLBoolean EGLAPIENTRY mali_eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
{
    LoadMali();
    return mali_eglCopyBuffers_ptr(dpy, surface, target);
}

static EGLContext EGLAPIENTRY mali_eglCreateContext(EGLDisplay dpy,
        EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
{
    LoadMali();
    return mali_eglCreateContext_ptr(dpy, config, share_context, attrib_list);
}

static EGLBoolean EGLAPIENTRY mali_eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    LoadMali();
    return mali_eglDestroyContext_ptr(dpy, ctx);
}

static EGLSurface EGLAPIENTRY mali_eglCreatePlatformWindowSurface(EGLDisplay dpy,
        EGLConfig config, void *native_window, const EGLAttrib *attrib_list)
{
    LoadMali();
    return mali_eglCreatePlatformWindowSurface_ptr(dpy, config, native_window, attrib_list);
}

static EGLSurface EGLAPIENTRY mali_eglCreatePlatformPixmapSurface(EGLDisplay dpy,
        EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list)
{
    LoadMali();
    return mali_eglCreatePlatformPixmapSurface_ptr(dpy, config, native_pixmap, attrib_list);
}

static EGLSurface EGLAPIENTRY mali_eglCreatePbufferSurface(EGLDisplay dpy,
        EGLConfig config, const EGLint *attrib_list)
{
    LoadMali();
    return mali_eglCreatePbufferSurface_ptr(dpy, config, attrib_list);
}

static EGLSurface EGLAPIENTRY mali_eglCreatePixmapSurface(EGLDisplay dpy,
        EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list)
{
    LoadMali();
    return mali_eglCreatePixmapSurface_ptr(dpy, config, pixmap, attrib_list);
}

static EGLSurface EGLAPIENTRY mali_eglCreateWindowSurface(EGLDisplay dpy,
        EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
    LoadMali();
    return mali_eglCreateWindowSurface_ptr(dpy, config, win, attrib_list);
}

static EGLSurface EGLAPIENTRY mali_eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
        EGLenum buftype, EGLClientBuffer buffer, EGLConfig config,
        const EGLint *attrib_list)
{
    LoadMali();
    return mali_eglCreatePbufferFromClientBuffer_ptr(dpy, buftype, buffer, config, attrib_list);
}

static EGLBoolean EGLAPIENTRY mali_eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    LoadMali();
    return mali_eglDestroySurface_ptr(dpy, surface);
}

static EGLBoolean EGLAPIENTRY mali_eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
    LoadMali();
    return mali_eglGetConfigAttrib_ptr(dpy, config, attribute, value);
}

static EGLBoolean EGLAPIENTRY mali_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    LoadMali();
    return mali_eglMakeCurrent_ptr(dpy, draw, read, ctx);
}

static EGLBoolean EGLAPIENTRY mali_eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
{
    LoadMali();
    return mali_eglQueryContext_ptr(dpy, ctx, attribute, value);
}

static const char * EGLAPIENTRY mali_eglQueryString(EGLDisplay dpy, EGLenum name)
{
    LoadMali();
    return mali_eglQueryString_ptr(dpy, name);
}

static EGLBoolean EGLAPIENTRY mali_eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value)
{
    LoadMali();
    return mali_eglQuerySurface_ptr(dpy, surface, attribute, value);
}

static EGLBoolean EGLAPIENTRY mali_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    LoadMali();
    return mali_eglSwapBuffers_ptr(dpy, surface);
}

static EGLBoolean EGLAPIENTRY mali_eglWaitGL(void)
{
    LoadMali();
    return mali_eglWaitGL_ptr();
}

static EGLBoolean EGLAPIENTRY mali_eglWaitNative(EGLint engine)
{
    LoadMali();
    return mali_eglWaitNative_ptr(engine);
}

static EGLBoolean EGLAPIENTRY mali_eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    LoadMali();
    return mali_eglBindTexImage_ptr(dpy, surface, buffer);
}

static EGLBoolean EGLAPIENTRY mali_eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    LoadMali();
    return mali_eglReleaseTexImage_ptr(dpy, surface, buffer);
}

static EGLBoolean EGLAPIENTRY mali_eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
{
    LoadMali();
    return mali_eglSurfaceAttrib_ptr(dpy, surface, attribute, value);
}

static EGLBoolean EGLAPIENTRY mali_eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
    LoadMali();
    return mali_eglSwapInterval_ptr(dpy, interval);
}

static EGLBoolean EGLAPIENTRY mali_eglBindAPI(EGLenum api)
{
    LoadMali();
    return mali_eglBindAPI_ptr(api);
}

static EGLBoolean EGLAPIENTRY mali_eglReleaseThread(void)
{
    LoadMali();
    return mali_eglReleaseThread_ptr();
}

static EGLBoolean EGLAPIENTRY mali_eglWaitClient(void)
{
    LoadMali();
    return mali_eglWaitClient_ptr();
}

static EGLint EGLAPIENTRY mali_eglGetError(void)
{
    LoadMali();
    return mali_eglGetError_ptr();
}

static EGLBoolean EGLAPIENTRY mali_eglQueryDevicesEXT(EGLint max_devices, EGLDeviceEXT *devices, EGLint *num_devices)
{
    // LoadMali();
    // return mali_eglQueryDevicesEXT_ptr(max_devices, devices, num_devices);
    return EGL_FALSE;
}

static EGLBoolean EGLAPIENTRY mali_eglQueryDisplayAttribEXT(EGLDisplay dpy, EGLint attribute, EGLAttrib *value)
{
    // LoadMali();
    // return mali_eglQueryDisplayAttribEXT_ptr(dpy, attribute, value);
    return EGL_FALSE;
}

static EGLBoolean EGLAPIENTRY mali_eglQueryDeviceAttribEXT(EGLDeviceEXT device, EGLint attribute, EGLAttrib *value)
{
    // LoadMali();
    // return mali_eglQueryDeviceAttribEXT_ptr(device, attribute, value);
    return EGL_FALSE;
}

static const char *EGLAPIENTRY mali_eglQueryDeviceStringEXT(EGLDeviceEXT device, EGLint name)
{
    // LoadMali();
    // return mali_eglQueryDeviceStringEXT_ptr(device, name);
    return EGL_FALSE;
}

static EGLint EGLAPIENTRY mali_eglDebugMessageControlKHR(EGLDEBUGPROCKHR callback, const EGLAttrib *attrib_list)
{
    // LoadMali();
    // return mali_eglDebugMessageControlKHR_ptr(callback, attrib_list);
    return EGL_FALSE;
}

static EGLBoolean EGLAPIENTRY mali_eglQueryDebugKHR(EGLint attribute, EGLAttrib *value)
{
    // LoadMali();
    // return mali_eglQueryDebugKHR_ptr(attribute, value);
    return EGL_FALSE;
}

static EGLint EGLAPIENTRY mali_eglLabelObjectKHR(EGLDisplay dpy,
        EGLenum objectType, EGLObjectKHR object, EGLLabelKHR label)
{
    // LoadMali();
    // return mali_eglLabelObjectKHR_ptr(dpy, objectType, object, label);
    return EGL_FALSE;
}

static const GLubyte *mali_glGetString(GLenum name)
{
    LoadMali();
    if (mali_glGetString_ptr != NULL) {
        return mali_glGetString_ptr(name);
    }
    if (name == GL_VENDOR) {
        return (const GLubyte *) MALI_VENDOR_NAME;
    }
    return NULL;
}

static const MaliProcAddressEntry PROC_ADDRESSES[] = {
#define PROC_ENTRY(name) { #name, (void *)mali_ ## name }
    PROC_ENTRY(eglInitialize),
    PROC_ENTRY(eglTerminate),
    PROC_ENTRY(eglChooseConfig),
    PROC_ENTRY(eglGetConfigs),
    PROC_ENTRY(eglCopyBuffers),
    PROC_ENTRY(eglCreateContext),
    PROC_ENTRY(eglDestroyContext),
    PROC_ENTRY(eglCreatePlatformWindowSurface),
    PROC_ENTRY(eglCreatePlatformPixmapSurface),
    PROC_ENTRY(eglCreatePbufferSurface),
    PROC_ENTRY(eglCreatePixmapSurface),
    PROC_ENTRY(eglCreateWindowSurface),
    PROC_ENTRY(eglCreatePbufferFromClientBuffer),
    PROC_ENTRY(eglDestroySurface),
    PROC_ENTRY(eglGetConfigAttrib),
    PROC_ENTRY(eglMakeCurrent),
    PROC_ENTRY(eglQueryContext),
    PROC_ENTRY(eglQueryString),
    PROC_ENTRY(eglQuerySurface),
    PROC_ENTRY(eglSwapBuffers),
    PROC_ENTRY(eglWaitGL),
    PROC_ENTRY(eglWaitNative),
    PROC_ENTRY(eglBindTexImage),
    PROC_ENTRY(eglReleaseTexImage),
    PROC_ENTRY(eglSurfaceAttrib),
    PROC_ENTRY(eglSwapInterval),
    PROC_ENTRY(eglBindAPI),
    PROC_ENTRY(eglReleaseThread),
    PROC_ENTRY(eglWaitClient),
    PROC_ENTRY(eglGetError),

    // PROC_ENTRY(eglQueryDevicesEXT),
    // PROC_ENTRY(eglQueryDisplayAttribEXT),
    // PROC_ENTRY(eglQueryDeviceAttribEXT),
    // PROC_ENTRY(eglQueryDeviceStringEXT),
    // PROC_ENTRY(eglDebugMessageControlKHR),
    // PROC_ENTRY(eglQueryDebugKHR),
    // PROC_ENTRY(eglLabelObjectKHR),

    PROC_ENTRY(glGetString),
#undef PROC_ENTRY
    { NULL, NULL }
};

static void *maliFindDispatchFunction(const char *name)
{
    (void) name;
    return NULL;
}

static void maliSetDispatchIndex(const char *name, int index)
{
    (void) name;
    (void) index;
}

// void *__egl_Main(void)
// {
//     printf("%s\n",__func__);
//     return NULL;
// }

PUBLIC EGLBoolean
__egl_Main(uint32_t version, const __EGLapiExports *exports,
     __EGLvendorInfo *vendor, __EGLapiImports *imports)
{

    printf("enter __egl_Main~~~~~~~~~~~~~~~~~~\n");
    if (EGL_VENDOR_ABI_GET_MAJOR_VERSION(version) !=
        EGL_VENDOR_ABI_MAJOR_VERSION) {
        return EGL_FALSE;
    }

    imports->getPlatformDisplay = maliGetPlatformDisplay;
    imports->getSupportsAPI = maliGetSupportsAPI;
    imports->getVendorString = maliGetVendorString;
    imports->getProcAddress = maliGetProcAddress;
    imports->getDispatchAddress = maliFindDispatchFunction;
    imports->setDispatchIndex = maliSetDispatchIndex;

    return EGL_TRUE;
}

