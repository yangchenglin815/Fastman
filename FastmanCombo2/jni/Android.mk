LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := zmaster_lib
FILE_LIST := $(wildcard $(LOCAL_PATH)/../zmaster2/*.cpp $(LOCAL_PATH)/../zmaster2/*.c)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/..
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)
#LOCAL_CFLAGS += -DCMD_DEBUG -DNO_QTLOG
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
ifeq ($(TARGET_ARCH),arm)
    LOCAL_MODULE    := zmaster2
else
    ifeq ($(TARGET_ARCH),mips)
        LOCAL_MODULE    := zmaster2_mips
    else
        ifeq ($(TARGET_ARCH),x86)
            LOCAL_MODULE    := zmaster2_x86
        endif
    endif
endif
LOCAL_LDLIBS := -lz
LOCAL_STATIC_LIBRARIES := zmaster_lib
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
ifeq ($(TARGET_ARCH),arm)
    LOCAL_MODULE    := zmaster2_pie
else
    ifeq ($(TARGET_ARCH),mips)
        LOCAL_MODULE    := zmaster2_mips_pie
    else
        ifeq ($(TARGET_ARCH),x86)
            LOCAL_MODULE    := zmaster2_x86_pie
        endif
    endif
endif
LOCAL_LDLIBS := -lz
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
LOCAL_STATIC_LIBRARIES := zmaster_lib
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := test
LOCAL_SRC_FILES := test.cpp
include $(BUILD_EXECUTABLE)
