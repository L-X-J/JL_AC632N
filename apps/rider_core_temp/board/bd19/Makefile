# Independent AC632N Rider CoreTemp firmware target.
# The source list intentionally contains no SPP/EDR/demo application files.

ifeq ($(OS), Windows_NT)
TOOL_DIR := C:/JL/pi32/bin
CC := clang.exe
CXX := clang.exe
LD := q32s-lto-wrapper.exe
AR := llvm-ar.exe
MKDIR := mkdir_win -p
RM := rm -rf
SYS_LIB_DIR := C:/JL/pi32/q32s-lib
SYS_INC_DIR := C:/JL/pi32/q32s-include
EXT_CFLAGS :=
FIXBAT := ..\..\..\..\tools\utils\fixbat.exe
POST_SCRIPT := ../../../../cpu/bd19/tools/download.bat
RUN_POST_SCRIPT := ..\..\..\..\cpu\bd19\tools\download.bat
else
TOOL_DIR := /opt/jieli/q32s/bin
CC := clang
CXX := clang
LD := lto-wrapper
AR := lto-ar
MKDIR := mkdir -p
RM := rm -rf
SYS_LIB_DIR := $(TOOL_DIR)/../lib
SYS_INC_DIR := $(TOOL_DIR)/../include
EXT_CFLAGS := -D__SHELL__
export OBJDUMP := $(TOOL_DIR)/objdump
export OBJCOPY := $(TOOL_DIR)/objcopy
export OBJSIZEDUMP := $(TOOL_DIR)/objsizedump
FIXBAT := touch
POST_SCRIPT := ../../../../cpu/bd19/tools/download.sh
RUN_POST_SCRIPT := bash $(POST_SCRIPT)
endif

CC := $(TOOL_DIR)/$(CC)
CXX := $(TOOL_DIR)/$(CXX)
LD := $(TOOL_DIR)/$(LD)
AR := $(TOOL_DIR)/$(AR)

OUT_ELF := ../../../../cpu/bd19/tools/sdk.elf
OBJ_FILE := $(OUT_ELF).objs.txt
BUILD_DIR := objs
ROOT_PREFIX := ../../../..

CFLAGS := \
	-flto -target q32s -integrated-as -fno-builtin \
	-mllvm -inline-threshold=5 -Oz -g -Os \
	-Wcast-align -w -fallow-pointer-null -Wincompatible-pointer-types \
	-Wundef -fprefer-gnu-section -Wframe-larger-than=256 \
	-Wreturn-type -Wimplicit-function-declaration -Werror=jl-arg-struct-check \
	-fms-extensions
CXXFLAGS :=

DEFINES := \
	-DSUPPORT_MS_EXTENSIONS \
	-DCONFIG_RELEASE_ENABLE \
	-DCONFIG_CPU_BD19 \
	-DCONFIG_FREE_RTOS_ENABLE \
	-DCONFIG_EQ_SUPPORT_ASYNC \
	-DEQ_DESIGN_IN_MASK \
	-DSINE_MAKE_IN_MASK \
	-DCONFIG_MMU_ENABLE \
	-DCONFIG_AEC_M=256 \
	-DCONFIG_AUDIO_ONCHIP \
	-DCONFIG_MEDIA_NEW_ENABLE \
	-D__GCC_Q32S__ -D__Q32S__ \
	-DCONFIG_NEW_ECC_ENABLE \
	-DCONFIG_CRYPTO_TOOLBOX_OSIZE_IN_MASKROM \
	-DEVENT_HANDLER_NUM_CONFIG=2 \
	-DEVENT_TOUCH_ENABLE_CONFIG=0 \
	-DEVENT_POOL_SIZE_CONFIG=256 \
	-DCONFIG_EVENT_KEY_MAP_ENABLE=0 \
	-DTIMER_POOL_NUM_CONFIG=10 \
	-DAPP_ASYNC_POOL_NUM_CONFIG=0 \
	-DSDK_VERSION_CFG_DEFINE=0x240001 \
	-DSDK_VERSION_DATE_DEFINE=20241231 \
	-DVFS_ENABLE=1 -DUSE_SDFILE_NEW=1 \
	-DVFS_FILE_POOL_NUM_CONFIG=0 -DSDFILE_VERSION=0x020000 \
	-DVM_MAX_SIZE_CONFIG=16*1024 -DVM_ITEM_MAX_NUM=256 \
	-DCONFIG_TWS_ENABLE \
	-DCONFIG_LITE_AEC_ENABLE=0 -DCONFIG_LITE_AUDIO \
	-DCONFIG_LMP_RESET_ENABLE \
	-DCONFIG_LMP_REFRESH_ENCRYPTION_KEY_ENABLE \
	-DCONFIG_LINK_DISTURB_SCAN_ENABLE=0 \
	-DCONFIG_BTCTRLER_TASK_DEL_ENABLE \
	-DCONFIG_UPDATA_ENABLE -DCONFIG_OTA_UPDATA_ENABLE \
	-DCONFIG_ITEM_FORMAT_VM -DTCFG_APP_BT_EN=1 \
	-DUSB_PC_NO_APP_MODE $(EXT_CFLAGS)

INCLUDES := \
	-I../../../../include_lib \
	-I../../../../include_lib/driver \
	-I../../../../include_lib/driver/device \
	-I../../../../include_lib/driver/cpu/bd19 \
	-I../../../../include_lib/system \
	-I../../../../include_lib/system/generic \
	-I../../../../include_lib/system/device \
	-I../../../../include_lib/system/fs \
	-I../../../../include_lib/system/math/cpu/bd19 \
	-I../../../../include_lib/btctrler \
	-I../../../../include_lib/btctrler/port/bd19 \
	-I../../../../include_lib/update \
	-I../../../../include_lib/agreement \
	-I../../../../include_lib/btstack \
	-I../../../../include_lib/btstack/third_party/common \
	-I../../../../include_lib/btstack/third_party/rcsp \
	-I../../../../include_lib/media/media_new \
	-I../../../../include_lib/media/media_new/media \
	-I../../../../include_lib/media/media_new/media/cpu/bd19 \
	-I../../../../include_lib/media/media_new/media/cpu/bd19/asm \
	-I../../../../apps/rider_core_temp/include \
	-I../../../../apps/rider_core_temp/board/bd19 \
	-I../../../../apps/common \
	-I../../../../apps/common/config/include \
	-I../../../../apps/common/include \
	-I../../../../apps/common/device \
	-I../../../../apps/common/third_party_profile/common \
	-I../../../../apps/common/third_party_profile/jieli \
	-I../../../../apps/common/third_party_profile/jieli/JL_rcsp \
	-I../../../../apps/common/third_party_profile/jieli/gatt_common \
	-I../../../../cpu/bd19 \
	-I$(SYS_INC_DIR)

# Common infrastructure and the two GATT server files are the only shared
# product code in this target.  All protocol/product behavior stays in Rider.
c_SRC_FILES := \
	../../../../apps/common/debug/debug.c \
	../../../../apps/common/bt_common/bt_test_api.c \
	../../../../apps/common/device/norflash/norflash.c \
	../../../../apps/common/temp_trim/dtemp_pll_trim.c \
	../../../../apps/common/third_party_profile/common/custom_cfg.c \
	../../../../apps/common/third_party_profile/jieli/gatt_common/le_gatt_common.c \
	../../../../apps/common/third_party_profile/jieli/gatt_common/le_gatt_server.c \
	../../../../apps/common/update/testbox_update.c \
	../../../../apps/common/update/update.c \
	../../../../apps/rider_core_temp/app_main.c \
	../../../../apps/rider_core_temp/version.c \
	../../../../apps/rider_core_temp/config/lib_btctrler_config.c \
	../../../../apps/rider_core_temp/config/lib_btstack_config.c \
	../../../../apps/rider_core_temp/config/lib_driver_config.c \
	../../../../apps/rider_core_temp/config/lib_media_config.c \
	../../../../apps/rider_core_temp/config/lib_profile_config.c \
	../../../../apps/rider_core_temp/config/lib_system_config.c \
	../../../../apps/rider_core_temp/config/lib_update_config.c \
	../../../../apps/rider_core_temp/config/log_config.c \
	../../../../apps/rider_core_temp/modules/bt/core_temp_gatt.c \
	../../../../apps/rider_core_temp/modules/bt/rider_ble_lifecycle.c \
	../../../../apps/rider_core_temp/modules/main/rider_core_temp_app.c \
	../../../../apps/rider_core_temp/modules/system/rider_user_cfg.c \
	../../../../apps/rider_core_temp/modules/temp/m601_1wire.c \
	../../../../apps/rider_core_temp/modules/temp/rider_core_estimator.c \
	../../../../apps/rider_core_temp/board/bd19/board_ac632n_rider.c \
	../../../../cpu/bd19/adc_api.c \
	../../../../cpu/bd19/charge.c \
	../../../../cpu/bd19/chargestore.c \
	../../../../cpu/bd19/handshake_timer.c \
	../../../../cpu/bd19/iic_hw.c \
	../../../../cpu/bd19/iic_soft.c \
	../../../../cpu/bd19/irflt.c \
	../../../../cpu/bd19/ledc.c \
	../../../../cpu/bd19/mcpwm.c \
	../../../../cpu/bd19/plcnt.c \
	../../../../cpu/bd19/pwm_led.c \
	../../../../cpu/bd19/setup.c \
	../../../../cpu/bd19/spi.c \
	../../../../cpu/bd19/uart_dev.c

S_SRC_FILES :=
s_SRC_FILES :=
cpp_SRC_FILES :=
cc_SRC_FILES :=
cxx_SRC_FILES :=

LFLAGS := \
	--plugin-opt=-inline-threshold=5 \
	--plugin-opt=save-temps \
	--plugin-opt=-inline-normal-into-special-section=true \
	--plugin-opt=-dont-used-symbol-list=malloc,free,sprintf,printf,puts,putchar \
	--plugin-opt=-warn-stack-size=256 \
	--sort-common --dont-complain-call-overflow --gc-sections \
	--start-group \
	../../../../cpu/bd19/liba/cpu.a \
	../../../../cpu/bd19/liba/system.a \
	../../../../cpu/bd19/liba/btstack.a \
	../../../../cpu/bd19/liba/btctrler.a \
	../../../../cpu/bd19/liba/agreement.a \
	../../../../cpu/bd19/liba/rcsp_stack.a \
	../../../../cpu/bd19/liba/media.a \
	../../../../cpu/bd19/liba/lc3_codec_lib.a \
	../../../../cpu/bd19/liba/usbc_codec_lib.a \
	../../../../cpu/bd19/liba/lib_adpcm_ima_code.a \
	../../../../cpu/bd19/liba/wav_dec_lib.a \
	../../../../cpu/bd19/liba/wtgv2_dec_lib.a \
	../../../../cpu/bd19/liba/opus_enc_lib.a \
	../../../../cpu/bd19/liba/msbc_codec_lib.a \
	../../../../cpu/bd19/liba/update.a \
	--end-group \
	-T../../../../cpu/bd19/sdk.ld \
	-M=../../../../cpu/bd19/tools/sdk.map \
	-flto --plugin-opt=-inline-threshold=5

LIBPATHS := -L$(SYS_LIB_DIR)
LIBS := $(SYS_LIB_DIR)/libm.a $(SYS_LIB_DIR)/libc.a \
	$(SYS_LIB_DIR)/libm.a $(SYS_LIB_DIR)/libcompiler-rt.a

c_OBJS := $(c_SRC_FILES:%.c=%.c.o)
S_OBJS := $(S_SRC_FILES:%.S=%.S.o)
s_OBJS := $(s_SRC_FILES:%.s=%.s.o)
cpp_OBJS := $(cpp_SRC_FILES:%.cpp=%.cpp.o)
cxx_OBJS := $(cxx_SRC_FILES:%.cxx=%.cxx.o)
cc_OBJS := $(cc_SRC_FILES:%.cc=%.cc.o)
OBJS := $(c_OBJS) $(S_OBJS) $(s_OBJS) $(cpp_OBJS) $(cxx_OBJS) $(cc_OBJS)
DEP_FILES := $(OBJS:%.o=%.d)
OBJS := $(addprefix $(BUILD_DIR)/, $(OBJS:$(ROOT_PREFIX)/%=%))
DEP_FILES := $(addprefix $(BUILD_DIR)/, $(DEP_FILES:$(ROOT_PREFIX)/%=%))

VERBOSE ?= 0
ifeq ($(VERBOSE),1)
QUIET :=
else
QUIET := @
endif
LINK_AT ?= 1

.PHONY: all clean pre_build
.SUFFIXES:

all: pre_build $(OUT_ELF)
	$(info +POST-BUILD)
	$(QUIET) $(RUN_POST_SCRIPT) sdk

pre_build:
	$(info +PRE-BUILD)
	$(QUIET) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P ../../../../cpu/bd19/sdk_used_list.c -o ../../../../cpu/bd19/sdk_used_list.used
	$(QUIET) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P ../../../../cpu/bd19/sdk_ld.c -o ../../../../cpu/bd19/sdk.ld
	$(QUIET) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P ../../../../cpu/bd19/tools/download.c -o $(POST_SCRIPT)
	$(QUIET) $(FIXBAT) $(POST_SCRIPT)
	$(QUIET) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P ../../../../cpu/bd19/tools/isd_config_rule.c -o ../../../../cpu/bd19/tools/isd_config.ini

clean:
	$(QUIET) $(RM) $(OUT_ELF) $(BUILD_DIR)

ifeq ($(LINK_AT),1)
$(OUT_ELF): $(OBJS)
	$(info +LINK $@)
	$(shell $(MKDIR) $(@D))
	$(file >$(OBJ_FILE), $(OBJS))
	$(QUIET) $(LD) -o $(OUT_ELF) @$(OBJ_FILE) $(LFLAGS) $(LIBPATHS) $(LIBS)
else
$(OUT_ELF): $(OBJS)
	$(info +LINK $@)
	$(shell $(MKDIR) $(@D))
	$(QUIET) $(LD) -o $(OUT_ELF) $(OBJS) $(LFLAGS) $(LIBPATHS) $(LIBS)
endif

$(BUILD_DIR)/%.c.o: $(ROOT_PREFIX)/%.c
	$(info +CC $<)
	$(QUIET) $(MKDIR) $(@D)
	$(QUIET) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUIET) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.S.o: $(ROOT_PREFIX)/%.S
	$(info +AS $<)
	$(QUIET) $(MKDIR) $(@D)
	$(QUIET) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUIET) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

-include $(DEP_FILES)
