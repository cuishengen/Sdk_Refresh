################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
F:/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver_api/src/otg/host/otg_host_standard_enum.c \
F:/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver_api/src/otg/host/otg_host_udisk.c 

OBJS += \
./driver/driver_api/src/otg/host/otg_host_standard_enum.o \
./driver/driver_api/src/otg/host/otg_host_udisk.o 

C_DEPS += \
./driver/driver_api/src/otg/host/otg_host_standard_enum.d \
./driver/driver_api/src/otg/host/otg_host_udisk.d 


# Each subdirectory must supply rules for building sources it contributes
driver/driver_api/src/otg/host/otg_host_standard_enum.o: /cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver_api/src/otg/host/otg_host_standard_enum.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -DFUNC_OS_EN=1 -DCFG_APP_CONFIG -DHAVE_CONFIG_H -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/audio" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/md5" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/speex/include/speex" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/speex/include" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/speex/libspeex" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/speex" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver_api/inc/otg" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/rtc/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/user/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/bluetooth/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/lrc/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/flashfs/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/display" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/user" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/cec/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '

driver/driver_api/src/otg/host/otg_host_udisk.o: /cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver_api/src/otg/host/otg_host_udisk.c
	@echo '正在构建文件： $<'
	@echo '正在调用： Andes C Compiler'
	$(CROSS_COMPILE)gcc -DFUNC_OS_EN=1 -DCFG_APP_CONFIG -DHAVE_CONFIG_H -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/audio" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/md5" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/speex/include/speex" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/speex/include" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/speex/libspeex" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai/speex" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/ai" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver_api/inc/otg" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/rtc/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/fatfs/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/user/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/bluetooth/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/audio/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/driver/driver_api/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/mv_utils/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/lrc/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/rtos/rtos_api" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/rtos/freertos/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/flashfs/inc" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/display" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/BT_Audio_APP/bt_audio_app_src/user" -I"/cygdrive/F/Soft_sdk/Mv_BP10X/MVsB1_Base_SDK/middleware/cec/inc" -Og -mcmodel=medium -g3 -Wall -mcpu=d1088-spu -c -fmessage-length=0 -ldsp -mext-dsp -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo '已结束构建： $<'
	@echo ' '


