LOCAL_PATH := $(call my-dir)

################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE    := libnonblockpp

LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/../../src \

LOCAL_SRC_FILES +=  ../../src/nonblock.cpp

#LOCAL_SHARED_LIBRARIES := -lz
# LOCAL_STATIC_LIBRARIES := 
# LOCAL_EXPORT_LDLIBS := -lz
# LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/.

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_STATIC_LIBRARY)