/*
 * Copyright 2014 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#pragma once

#ifdef __cplusplus
#if !defined(__DEFINED_pthread_t)
typedef unsigned long pthread_t;
#define __DEFINED_pthread_t
#endif
#else
#if !defined(__DEFINED_pthread_t)
typedef struct __pthread * pthread_t;
#define __DEFINED_pthread_t
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* This file defines Emscripten low-level glue bindings for interfacing with HTML5 APIs
 * 
 * Documentation for the public APIs defined in this file must be updated in: 
 *    site/source/docs/api_reference/html5.h.rst
 * A prebuilt local version of the documentation is available at: 
 *    site/build/text/docs/api_reference/html5.h.txt
 * You can also build docs locally as HTML or other formats in site/
 * An online HTML version (which may be of a different version of Emscripten)
 *    is up at http://kripken.github.io/emscripten-site/docs/api_reference/html5.h.html
 */

#define EMSCRIPTEN_EVENT_KEYPRESS               1
#define EMSCRIPTEN_EVENT_KEYDOWN                2
#define EMSCRIPTEN_EVENT_KEYUP                  3
#define EMSCRIPTEN_EVENT_CLICK                  4
#define EMSCRIPTEN_EVENT_MOUSEDOWN              5
#define EMSCRIPTEN_EVENT_MOUSEUP                6
#define EMSCRIPTEN_EVENT_DBLCLICK               7
#define EMSCRIPTEN_EVENT_MOUSEMOVE              8
#define EMSCRIPTEN_EVENT_WHEEL                  9
#define EMSCRIPTEN_EVENT_RESIZE                10
#define EMSCRIPTEN_EVENT_SCROLL                11
#define EMSCRIPTEN_EVENT_BLUR                  12
#define EMSCRIPTEN_EVENT_FOCUS                 13
#define EMSCRIPTEN_EVENT_FOCUSIN               14
#define EMSCRIPTEN_EVENT_FOCUSOUT              15
#define EMSCRIPTEN_EVENT_DEVICEORIENTATION     16
#define EMSCRIPTEN_EVENT_DEVICEMOTION          17
#define EMSCRIPTEN_EVENT_ORIENTATIONCHANGE     18
#define EMSCRIPTEN_EVENT_FULLSCREENCHANGE      19
#define EMSCRIPTEN_EVENT_POINTERLOCKCHANGE     20
#define EMSCRIPTEN_EVENT_VISIBILITYCHANGE      21
#define EMSCRIPTEN_EVENT_TOUCHSTART            22
#define EMSCRIPTEN_EVENT_TOUCHEND              23
#define EMSCRIPTEN_EVENT_TOUCHMOVE             24
#define EMSCRIPTEN_EVENT_TOUCHCANCEL           25
#define EMSCRIPTEN_EVENT_GAMEPADCONNECTED      26
#define EMSCRIPTEN_EVENT_GAMEPADDISCONNECTED   27
#define EMSCRIPTEN_EVENT_BEFOREUNLOAD          28
#define EMSCRIPTEN_EVENT_BATTERYCHARGINGCHANGE 29
#define EMSCRIPTEN_EVENT_BATTERYLEVELCHANGE    30
#define EMSCRIPTEN_EVENT_WEBGLCONTEXTLOST      31
#define EMSCRIPTEN_EVENT_WEBGLCONTEXTRESTORED  32
#define EMSCRIPTEN_EVENT_MOUSEENTER            33
#define EMSCRIPTEN_EVENT_MOUSELEAVE            34
#define EMSCRIPTEN_EVENT_MOUSEOVER             35
#define EMSCRIPTEN_EVENT_MOUSEOUT              36
#define EMSCRIPTEN_EVENT_CANVASRESIZED         37
#define EMSCRIPTEN_EVENT_POINTERLOCKERROR      38

#define EMSCRIPTEN_RESULT int


#define EMSCRIPTEN_RESULT_SUCCESS              0
#define EMSCRIPTEN_RESULT_DEFERRED             1
#define EMSCRIPTEN_RESULT_NOT_SUPPORTED       -1
#define EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED -2
#define EMSCRIPTEN_RESULT_INVALID_TARGET      -3
#define EMSCRIPTEN_RESULT_UNKNOWN_TARGET      -4
#define EMSCRIPTEN_RESULT_INVALID_PARAM       -5
#define EMSCRIPTEN_RESULT_FAILED              -6
#define EMSCRIPTEN_RESULT_NO_DATA             -7
#define EMSCRIPTEN_RESULT_TIMED_OUT           -8

#define EMSCRIPTEN_EVENT_TARGET_INVALID        0
#define EMSCRIPTEN_EVENT_TARGET_DOCUMENT       ((const char*)1)
#define EMSCRIPTEN_EVENT_TARGET_WINDOW         ((const char*)2)
#define EMSCRIPTEN_EVENT_TARGET_SCREEN         ((const char*)3)

#define EM_BOOL int
#define EM_TRUE 1
#define EM_FALSE 0
#define EM_UTF8 char

#define DOM_KEY_LOCATION int
#define DOM_KEY_LOCATION_STANDARD 0x00
#define DOM_KEY_LOCATION_LEFT     0x01
#define DOM_KEY_LOCATION_RIGHT    0x02
#define DOM_KEY_LOCATION_NUMPAD   0x03

#define EM_HTML5_SHORT_STRING_LEN_BYTES 32
#define EM_HTML5_MEDIUM_STRING_LEN_BYTES 64
#define EM_HTML5_LONG_STRING_LEN_BYTES 128

typedef struct EmscriptenKeyboardEvent {
  EM_UTF8 key[EM_HTML5_SHORT_STRING_LEN_BYTES];
  EM_UTF8 code[EM_HTML5_SHORT_STRING_LEN_BYTES];
  unsigned long location;
  EM_BOOL ctrlKey;
  EM_BOOL shiftKey;
  EM_BOOL altKey;
  EM_BOOL metaKey;
  EM_BOOL repeat;
  EM_UTF8 locale[EM_HTML5_SHORT_STRING_LEN_BYTES];
  EM_UTF8 charValue[EM_HTML5_SHORT_STRING_LEN_BYTES];
  unsigned long charCode;
  unsigned long keyCode;
  unsigned long which;
} EmscriptenKeyboardEvent;


typedef EM_BOOL (*em_key_callback_func)(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_keypress_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_key_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_keydown_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_key_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_keyup_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_key_callback_func callback, pthread_t targetThread);

typedef struct EmscriptenMouseEvent {
  long screenX;
  long screenY;
  long clientX;
  long clientY;
  EM_BOOL ctrlKey;
  EM_BOOL shiftKey;
  EM_BOOL altKey;
  EM_BOOL metaKey;
  unsigned short button;
  unsigned short buttons;
  long movementX;
  long movementY;
  long targetX;
  long targetY;
  // canvasX and canvasY are deprecated - there no longer exists a Module['canvas'] object, so canvasX/Y are no longer reported (register a listener on canvas directly to get canvas coordinates, or translate manually)
  long canvasX;
  long canvasY;
  long padding;
} EmscriptenMouseEvent;


typedef EM_BOOL (*em_mouse_callback_func)(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_click_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_mousedown_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_mouseup_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_dblclick_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_mousemove_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_mouseenter_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_mouseleave_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_mouseover_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_mouseout_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_mouse_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_get_mouse_status(EmscriptenMouseEvent *mouseState);

#define DOM_DELTA_PIXEL 0x00
#define DOM_DELTA_LINE  0x01
#define DOM_DELTA_PAGE  0x02

typedef struct EmscriptenWheelEvent {
  EmscriptenMouseEvent mouse;
  double deltaX;
  double deltaY;
  double deltaZ;
  unsigned long deltaMode;
} EmscriptenWheelEvent;


typedef EM_BOOL (*em_wheel_callback_func)(int eventType, const EmscriptenWheelEvent *wheelEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_wheel_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_wheel_callback_func callback, pthread_t targetThread);

typedef struct EmscriptenUiEvent {
  long detail;
  int documentBodyClientWidth;
  int documentBodyClientHeight;
  int windowInnerWidth;
  int windowInnerHeight;
  int windowOuterWidth;
  int windowOuterHeight;
  int scrollTop;
  int scrollLeft;
} EmscriptenUiEvent;


typedef EM_BOOL (*em_ui_callback_func)(int eventType, const EmscriptenUiEvent *uiEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_resize_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_ui_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_scroll_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_ui_callback_func callback, pthread_t targetThread);

typedef struct EmscriptenFocusEvent {
  EM_UTF8 nodeName[EM_HTML5_LONG_STRING_LEN_BYTES];
  EM_UTF8 id[EM_HTML5_LONG_STRING_LEN_BYTES];
} EmscriptenFocusEvent;

typedef EM_BOOL (*em_focus_callback_func)(int eventType, const EmscriptenFocusEvent *focusEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_blur_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_focus_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_focus_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_focus_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_focusin_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_focus_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_focusout_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_focus_callback_func callback, pthread_t targetThread);

typedef struct EmscriptenDeviceOrientationEvent {
  double alpha;
  double beta;
  double gamma;
  EM_BOOL absolute;
} EmscriptenDeviceOrientationEvent;


typedef EM_BOOL (*em_deviceorientation_callback_func)(int eventType, const EmscriptenDeviceOrientationEvent *deviceOrientationEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_deviceorientation_callback_on_thread(void *userData, EM_BOOL useCapture, em_deviceorientation_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_get_deviceorientation_status(EmscriptenDeviceOrientationEvent *orientationState);

#define EMSCRIPTEN_DEVICE_MOTION_EVENT_SUPPORTS_ACCELERATION                   0x01
#define EMSCRIPTEN_DEVICE_MOTION_EVENT_SUPPORTS_ACCELERATION_INCLUDING_GRAVITY 0x02
#define EMSCRIPTEN_DEVICE_MOTION_EVENT_SUPPORTS_ROTATION_RATE                  0x04

typedef struct EmscriptenDeviceMotionEvent {
  double accelerationX;
  double accelerationY;
  double accelerationZ;
  double accelerationIncludingGravityX;
  double accelerationIncludingGravityY;
  double accelerationIncludingGravityZ;
  double rotationRateAlpha;
  double rotationRateBeta;
  double rotationRateGamma;
  int supportedFields;
} EmscriptenDeviceMotionEvent;


typedef EM_BOOL (*em_devicemotion_callback_func)(int eventType, const EmscriptenDeviceMotionEvent *deviceMotionEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_devicemotion_callback_on_thread(void *userData, EM_BOOL useCapture, em_devicemotion_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_get_devicemotion_status(EmscriptenDeviceMotionEvent *motionState);

#define EMSCRIPTEN_ORIENTATION_PORTRAIT_PRIMARY    1
#define EMSCRIPTEN_ORIENTATION_PORTRAIT_SECONDARY  2
#define EMSCRIPTEN_ORIENTATION_LANDSCAPE_PRIMARY   4
#define EMSCRIPTEN_ORIENTATION_LANDSCAPE_SECONDARY 8

typedef struct EmscriptenOrientationChangeEvent {
  int orientationIndex;
  int orientationAngle;
} EmscriptenOrientationChangeEvent;


typedef EM_BOOL (*em_orientationchange_callback_func)(int eventType, const EmscriptenOrientationChangeEvent *orientationChangeEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_orientationchange_callback_on_thread(void *userData, EM_BOOL useCapture, em_orientationchange_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_get_orientation_status(EmscriptenOrientationChangeEvent *orientationStatus);
extern EMSCRIPTEN_RESULT emscripten_lock_orientation(int allowedOrientations);
extern EMSCRIPTEN_RESULT emscripten_unlock_orientation(void);

typedef struct EmscriptenFullscreenChangeEvent {
  EM_BOOL isFullscreen;
  EM_BOOL fullscreenEnabled;
  EM_UTF8 nodeName[EM_HTML5_LONG_STRING_LEN_BYTES];
  EM_UTF8 id[EM_HTML5_LONG_STRING_LEN_BYTES];
  int elementWidth;
  int elementHeight;
  int screenWidth;
  int screenHeight;
} EmscriptenFullscreenChangeEvent;


typedef EM_BOOL (*em_fullscreenchange_callback_func)(int eventType, const EmscriptenFullscreenChangeEvent *fullscreenChangeEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_fullscreenchange_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_fullscreenchange_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_get_fullscreen_status(EmscriptenFullscreenChangeEvent *fullscreenStatus);

#define EMSCRIPTEN_FULLSCREEN_SCALE int
#define EMSCRIPTEN_FULLSCREEN_SCALE_DEFAULT 0
#define EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH 1
#define EMSCRIPTEN_FULLSCREEN_SCALE_ASPECT  2
#define EMSCRIPTEN_FULLSCREEN_SCALE_CENTER  3

#define EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE int
#define EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE   0
#define EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF 1
#define EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF  2

#define EMSCRIPTEN_FULLSCREEN_FILTERING int
#define EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT 0
#define EMSCRIPTEN_FULLSCREEN_FILTERING_NEAREST 1
#define EMSCRIPTEN_FULLSCREEN_FILTERING_BILINEAR 2

typedef EM_BOOL (*em_canvasresized_callback_func)(int eventType, const void *reserved, void *userData);

typedef struct EmscriptenFullscreenStrategy {
  EMSCRIPTEN_FULLSCREEN_SCALE scaleMode;
  EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE canvasResolutionScaleMode;
  EMSCRIPTEN_FULLSCREEN_FILTERING filteringMode;
  em_canvasresized_callback_func canvasResizedCallback;
  void *canvasResizedCallbackUserData;
  pthread_t canvasResizedCallbackTargetThread;
} EmscriptenFullscreenStrategy;

extern EMSCRIPTEN_RESULT emscripten_request_fullscreen(const char *target, EM_BOOL deferUntilInEventHandler);
extern EMSCRIPTEN_RESULT emscripten_request_fullscreen_strategy(const char *target, EM_BOOL deferUntilInEventHandler, const EmscriptenFullscreenStrategy *fullscreenStrategy);

extern EMSCRIPTEN_RESULT emscripten_exit_fullscreen(void);

extern EMSCRIPTEN_RESULT emscripten_enter_soft_fullscreen(const char *target, const EmscriptenFullscreenStrategy *fullscreenStrategy);

extern EMSCRIPTEN_RESULT emscripten_exit_soft_fullscreen(void);

typedef struct EmscriptenPointerlockChangeEvent {
  EM_BOOL isActive;
  EM_UTF8 nodeName[EM_HTML5_LONG_STRING_LEN_BYTES];
  EM_UTF8 id[EM_HTML5_LONG_STRING_LEN_BYTES];
} EmscriptenPointerlockChangeEvent;


typedef EM_BOOL (*em_pointerlockchange_callback_func)(int eventType, const EmscriptenPointerlockChangeEvent *pointerlockChangeEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_pointerlockchange_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_pointerlockchange_callback_func callback, pthread_t targetThread);

typedef EM_BOOL (*em_pointerlockerror_callback_func)(int eventType, const void *reserved, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_pointerlockerror_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_pointerlockerror_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_get_pointerlock_status(EmscriptenPointerlockChangeEvent *pointerlockStatus);

extern EMSCRIPTEN_RESULT emscripten_request_pointerlock(const char *target, EM_BOOL deferUntilInEventHandler);

extern EMSCRIPTEN_RESULT emscripten_exit_pointerlock(void);

#define EMSCRIPTEN_VISIBILITY_HIDDEN    0
#define EMSCRIPTEN_VISIBILITY_VISIBLE   1
#define EMSCRIPTEN_VISIBILITY_PRERENDER 2
#define EMSCRIPTEN_VISIBILITY_UNLOADED  3

typedef struct EmscriptenVisibilityChangeEvent {
  EM_BOOL hidden;
  int visibilityState;
} EmscriptenVisibilityChangeEvent;

typedef EM_BOOL (*em_visibilitychange_callback_func)(int eventType, const EmscriptenVisibilityChangeEvent *visibilityChangeEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_visibilitychange_callback_on_thread(void *userData, EM_BOOL useCapture, em_visibilitychange_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_get_visibility_status(EmscriptenVisibilityChangeEvent *visibilityStatus);


typedef struct EmscriptenTouchPoint
{
  long identifier;
  long screenX;
  long screenY;
  long clientX;
  long clientY;
  long pageX;
  long pageY;
  EM_BOOL isChanged;
  EM_BOOL onTarget;
  long targetX;
  long targetY;
  // canvasX and canvasY are deprecated - there no longer exists a Module['canvas'] object, so canvasX/Y are no longer reported (register a listener on canvas directly to get canvas coordinates, or translate manually)
  long canvasX;
  long canvasY;
} EmscriptenTouchPoint;

typedef struct EmscriptenTouchEvent {
  int numTouches;
  EM_BOOL ctrlKey;
  EM_BOOL shiftKey;
  EM_BOOL altKey;
  EM_BOOL metaKey;
  EmscriptenTouchPoint touches[32];
} EmscriptenTouchEvent;


typedef EM_BOOL (*em_touch_callback_func)(int eventType, const EmscriptenTouchEvent *touchEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_touchstart_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_touch_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_touchend_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_touch_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_touchmove_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_touch_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_touchcancel_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_touch_callback_func callback, pthread_t targetThread);


typedef struct EmscriptenGamepadEvent {
  double timestamp;
  int numAxes;
  int numButtons;
  double axis[64];
  double analogButton[64];
  EM_BOOL digitalButton[64];
  EM_BOOL connected;
  long index;
  EM_UTF8 id[EM_HTML5_MEDIUM_STRING_LEN_BYTES];
  EM_UTF8 mapping[EM_HTML5_MEDIUM_STRING_LEN_BYTES];
} EmscriptenGamepadEvent;


typedef EM_BOOL (*em_gamepad_callback_func)(int eventType, const EmscriptenGamepadEvent *gamepadEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_gamepadconnected_callback_on_thread(void *userData, EM_BOOL useCapture, em_gamepad_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_gamepaddisconnected_callback_on_thread(void *userData, EM_BOOL useCapture, em_gamepad_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_sample_gamepad_data(void);
extern int emscripten_get_num_gamepads(void);
extern EMSCRIPTEN_RESULT emscripten_get_gamepad_status(int index, EmscriptenGamepadEvent *gamepadState);

typedef struct EmscriptenBatteryEvent {
  double chargingTime;
  double dischargingTime;
  double level;
  EM_BOOL charging;
} EmscriptenBatteryEvent;

typedef EM_BOOL (*em_battery_callback_func)(int eventType, const EmscriptenBatteryEvent *batteryEvent, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_batterychargingchange_callback_on_thread(void *userData, em_battery_callback_func callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_batterylevelchange_callback_on_thread(void *userData, em_battery_callback_func callback, pthread_t targetThread);

extern EMSCRIPTEN_RESULT emscripten_get_battery_status(EmscriptenBatteryEvent *batteryState);


extern EMSCRIPTEN_RESULT emscripten_vibrate(int msecs);
extern EMSCRIPTEN_RESULT emscripten_vibrate_pattern(int *msecsArray, int numEntries);

typedef const char *(*em_beforeunload_callback)(int eventType, const void *reserved, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_beforeunload_callback_on_thread(void *userData, em_beforeunload_callback callback, pthread_t targetThread);

typedef int EMSCRIPTEN_WEBGL_CONTEXT_HANDLE;

typedef int EMSCRIPTEN_WEBGL_CONTEXT_PROXY_MODE;
#define EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW 0
#define EMSCRIPTEN_WEBGL_CONTEXT_PROXY_FALLBACK 1
#define EMSCRIPTEN_WEBGL_CONTEXT_PROXY_ALWAYS   2

typedef int EM_WEBGL_POWER_PREFERENCE;
#define EM_WEBGL_POWER_PREFERENCE_DEFAULT 0
#define EM_WEBGL_POWER_PREFERENCE_LOW_POWER 1
#define EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE 2

typedef struct EmscriptenWebGLContextAttributes {
  EM_BOOL alpha;
  EM_BOOL depth;
  EM_BOOL stencil;
  EM_BOOL antialias;
  EM_BOOL premultipliedAlpha;
  EM_BOOL preserveDrawingBuffer;
  EM_WEBGL_POWER_PREFERENCE powerPreference;
  EM_BOOL failIfMajorPerformanceCaveat;

  int majorVersion;
  int minorVersion;

  EM_BOOL enableExtensionsByDefault;
  EM_BOOL explicitSwapControl;
  EMSCRIPTEN_WEBGL_CONTEXT_PROXY_MODE proxyContextToMainThread;
  EM_BOOL renderViaOffscreenBackBuffer;
} EmscriptenWebGLContextAttributes;

extern void emscripten_webgl_init_context_attributes(EmscriptenWebGLContextAttributes *attributes);

extern EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_create_context(const char *target, const EmscriptenWebGLContextAttributes *attributes);

extern EMSCRIPTEN_RESULT emscripten_webgl_make_context_current(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);

extern EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_get_current_context(void);

extern EMSCRIPTEN_RESULT emscripten_webgl_get_drawing_buffer_size(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context, int *width, int *height);

extern EMSCRIPTEN_RESULT emscripten_webgl_get_context_attributes(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context, EmscriptenWebGLContextAttributes *outAttributes);

extern EMSCRIPTEN_RESULT emscripten_webgl_destroy_context(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);

extern EM_BOOL emscripten_webgl_enable_extension(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context, const char *extension);

extern EM_BOOL emscripten_webgl_enable_ANGLE_instanced_arrays(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);

extern EM_BOOL emscripten_webgl_enable_OES_vertex_array_object(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);

extern EM_BOOL emscripten_webgl_enable_WEBGL_draw_buffers(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);

extern EM_BOOL emscripten_webgl_enable_WEBGL_draw_instanced_base_vertex_base_instance(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);

typedef EM_BOOL (*em_webgl_context_callback)(int eventType, const void *reserved, void *userData);
extern EMSCRIPTEN_RESULT emscripten_set_webglcontextlost_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_webgl_context_callback callback, pthread_t targetThread);
extern EMSCRIPTEN_RESULT emscripten_set_webglcontextrestored_callback_on_thread(const char *target, void *userData, EM_BOOL useCapture, em_webgl_context_callback callback, pthread_t targetThread);

extern EM_BOOL emscripten_is_webgl_context_lost(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);

extern EMSCRIPTEN_RESULT emscripten_webgl_commit_frame(void);

extern EM_BOOL emscripten_supports_offscreencanvas(void);

// Returns function pointers to WebGL 1 functions. Please avoid using this function ever - all WebGL1/GLES2 functions, even those for WebGL1 extensions, are available to user code via static linking. Calling GL functions
// via function pointers obtained here is slow, and using this function can greatly increase resulting compiled program size. This functionality is available only for easier program code porting purposes, but be aware
// that calling this is causing a noticeable performance and compiled code size hit.
extern void *emscripten_webgl1_get_proc_address(const char *name);

// Returns function pointers to WebGL 2 functions. Please avoid using this function ever - all WebGL2/GLES3 functions, even those for WebGL2 extensions, are available to user code via static linking. Calling GL functions
// via function pointers obtained here is slow, and using this function can greatly increase resulting compiled program size. This functionality is available only for easier program code porting purposes, but be aware
// that calling this is causing a noticeable performance and compiled code size hit.
extern void *emscripten_webgl2_get_proc_address(const char *name);

// Combines emscripten_webgl1_get_proc_address() and emscripten_webgl2_get_proc_address() to return function pointers to both WebGL1 and WebGL2 functions. Same drawbacks apply.
extern void *emscripten_webgl_get_proc_address(const char *name);

typedef struct WGPUDeviceImpl* WGPUDevice;
extern WGPUDevice emscripten_webgpu_get_device();

extern EMSCRIPTEN_RESULT emscripten_set_canvas_element_size(const char *target, int width, int height);
extern EMSCRIPTEN_RESULT emscripten_get_canvas_element_size(const char *target, int *width, int *height);

extern EMSCRIPTEN_RESULT emscripten_set_element_css_size(const char *target, double width, double height);
extern EMSCRIPTEN_RESULT emscripten_get_element_css_size(const char *target, double *width, double *height);

extern void emscripten_html5_remove_all_event_listeners(void);

#define EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD ((pthread_t)0x1)
#define EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD ((pthread_t)0x2)

#define emscripten_set_keypress_callback(target, userData, useCapture, callback)              emscripten_set_keypress_callback_on_thread(             (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_keydown_callback(target, userData, useCapture, callback)               emscripten_set_keydown_callback_on_thread(              (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_keyup_callback(target, userData, useCapture, callback)                 emscripten_set_keyup_callback_on_thread(                (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_click_callback(target, userData, useCapture, callback)                 emscripten_set_click_callback_on_thread(                (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_mousedown_callback(target, userData, useCapture, callback)             emscripten_set_mousedown_callback_on_thread(            (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_mouseup_callback(target, userData, useCapture, callback)               emscripten_set_mouseup_callback_on_thread(              (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_dblclick_callback(target, userData, useCapture, callback)              emscripten_set_dblclick_callback_on_thread(             (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_mousemove_callback(target, userData, useCapture, callback)             emscripten_set_mousemove_callback_on_thread(            (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_mouseenter_callback(target, userData, useCapture, callback)            emscripten_set_mouseenter_callback_on_thread(           (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_mouseleave_callback(target, userData, useCapture, callback)            emscripten_set_mouseleave_callback_on_thread(           (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_mouseover_callback(target, userData, useCapture, callback)             emscripten_set_mouseover_callback_on_thread(            (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_mouseout_callback(target, userData, useCapture, callback)              emscripten_set_mouseout_callback_on_thread(             (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_wheel_callback(target, userData, useCapture, callback)                 emscripten_set_wheel_callback_on_thread(                (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_resize_callback(target, userData, useCapture, callback)                emscripten_set_resize_callback_on_thread(               (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_scroll_callback(target, userData, useCapture, callback)                emscripten_set_scroll_callback_on_thread(               (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_blur_callback(target, userData, useCapture, callback)                  emscripten_set_blur_callback_on_thread(                 (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_focus_callback(target, userData, useCapture, callback)                 emscripten_set_focus_callback_on_thread(                (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_focusin_callback(target, userData, useCapture, callback)               emscripten_set_focusin_callback_on_thread(              (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_focusout_callback(target, userData, useCapture, callback)              emscripten_set_focusout_callback_on_thread(             (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_deviceorientation_callback(userData, useCapture, callback)             emscripten_set_deviceorientation_callback_on_thread(              (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_devicemotion_callback(userData, useCapture, callback)                  emscripten_set_devicemotion_callback_on_thread(                   (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_orientationchange_callback(userData, useCapture, callback)             emscripten_set_orientationchange_callback_on_thread(              (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_fullscreenchange_callback(target, userData, useCapture, callback)      emscripten_set_fullscreenchange_callback_on_thread(     (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_pointerlockchange_callback(target, userData, useCapture, callback)     emscripten_set_pointerlockchange_callback_on_thread(    (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_pointerlockerror_callback(target, userData, useCapture, callback)      emscripten_set_pointerlockerror_callback_on_thread(     (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_visibilitychange_callback(userData, useCapture, callback)              emscripten_set_visibilitychange_callback_on_thread(               (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_touchstart_callback(target, userData, useCapture, callback)            emscripten_set_touchstart_callback_on_thread(           (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_touchend_callback(target, userData, useCapture, callback)              emscripten_set_touchend_callback_on_thread(             (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_touchmove_callback(target, userData, useCapture, callback)             emscripten_set_touchmove_callback_on_thread(            (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_touchcancel_callback(target, userData, useCapture, callback)           emscripten_set_touchcancel_callback_on_thread(          (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_gamepadconnected_callback(userData, useCapture, callback)              emscripten_set_gamepadconnected_callback_on_thread(               (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_gamepaddisconnected_callback(userData, useCapture, callback)           emscripten_set_gamepaddisconnected_callback_on_thread(            (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_batterychargingchange_callback(userData, callback)                     emscripten_set_batterychargingchange_callback_on_thread(          (userData),               (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_batterylevelchange_callback(userData, callback)                        emscripten_set_batterylevelchange_callback_on_thread(             (userData),               (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_beforeunload_callback(userData, callback)                              emscripten_set_beforeunload_callback_on_thread(                   (userData),               (callback), EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD)
#define emscripten_set_webglcontextlost_callback(target, userData, useCapture, callback)      emscripten_set_webglcontextlost_callback_on_thread(     (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)
#define emscripten_set_webglcontextrestored_callback(target, userData, useCapture, callback)  emscripten_set_webglcontextrestored_callback_on_thread( (target), (userData), (useCapture), (callback), EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD)

extern long emscripten_set_timeout(void (*cb)(void *userData), double msecs, void *userData);
extern void emscripten_clear_timeout(long setTimeoutId);
extern void emscripten_set_timeout_loop(EM_BOOL (*cb)(double time, void *userData), double intervalMsecs, void *userData);

extern long emscripten_request_animation_frame(EM_BOOL (*cb)(double time, void *userData), void *userData);
extern void emscripten_cancel_animation_frame(long requestAnimationFrameId);
extern void emscripten_request_animation_frame_loop(EM_BOOL (*cb)(double time, void *userData), void *userData);

extern long emscripten_set_immediate(void (*cb)(void *userData), void *userData);
extern void emscripten_clear_immediate(long setImmediateId);
extern void emscripten_set_immediate_loop(EM_BOOL (*cb)(void *userData), void *userData);

extern long emscripten_set_interval(void (*cb)(void *userData), double intervalMsecs, void *userData);
extern void emscripten_clear_interval(long setIntervalId);

extern double emscripten_date_now(void);
extern double emscripten_performance_now(void);

extern void emscripten_console_log(const char *utf8String);
extern void emscripten_console_warn(const char *utf8String);
extern void emscripten_console_error(const char *utf8String);

extern void emscripten_throw_number(double number);
extern void emscripten_throw_string(const char *utf8String);

extern void emscripten_unwind_to_js_event_loop(void) __attribute__((noreturn));

#ifdef __cplusplus
} // ~extern "C"
#endif
