This is a guide to compile the library for android contributed by Jérémy VIENNE:

EDIT: Experimental script for compiling available there:
[Compilation script](https://gist.github.com/jav974/072425f14927e6ca2c7a4439d8ac5457)

Pre-requisites to execute this script:

- Android SDK
- Android NDK
- java, cmake, git

Just invoke it like any usual script after setting the path to Android SDK and Android NDK (the 2 first export of the file).

You might want to adjust java path too in the first lines.

Tested on ubuntu 17.04 64bit.

It should compile openalpr for these architectures:
armeabi, armeabi-v7a, armeabi-v7a with NEON, arm64-v8a, mips, mips64, x86, x86_64

Output libraries are located in project submodules. For example:

* openalpr/android-build/{arch}/openalpr/libopenalpr.so and libopenalpr-static.a
* openalpr/android-build/{arch}/openalpr/simpleini/libsimpleini.a
* openalpr/android-build/{arch}/openalpr/support/libsupport.a
* openalpr/android-build/{arch}/openalpr/video/libvideo.a
* openalpr/android-build/{arch}/openalpr/bindings/go/libopenalprgo.so
* openalpr/android-build/{arch}/openalpr/bindings/java/libopenalprjni.so
* openalpr/android-build/{arch}/openalpr/bindings/python/libopenalprpy.so

Please try it and let me know.

Below is a sample Android.mk and Application.mk to help you getting started with the usage of this lib.

Android.mk
```
LOCAL_PATH := $(call my-dir)
LIB_PATH := $(LOCAL_PATH)/../libs/armeabi-v7a

include $(CLEAR_VARS)

LOCAL_MODULE := leptonica
LOCAL_SRC_FILES := liblept.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := tesseract
LOCAL_SRC_FILES := libtess.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := simpleini
LOCAL_SRC_FILES := libsimpleini.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := support
LOCAL_SRC_FILES := libsupport.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := openalpr
LOCAL_SRC_FILES := libopenalpr-static.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

OPENCV_INSTALL_MODULES:=on
OPENCV_CAMERA_MODULES:=off

include path_to_opencv4android/OpenCV.mk

LOCAL_MODULE := nativealpr
LOCAL_SRC_FILES := NativeAlpr.cpp
LOCAL_SHARED_LIBRARIES += tesseract leptonica
LOCAL_STATIC_LIBRARIES += openalpr support simpleini

include $(BUILD_SHARED_LIBRARY)
```

Application.mk
```
APP_STL := gnustl_static
APP_CPPFLAGS := -frtti -fexceptions
APP_ABI := armeabi-v7a
```

In the Android.mk file, the NativeAlpr.cpp file contains the necessary native code to use the OpenALPR library