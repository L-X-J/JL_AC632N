cmake_minimum_required(VERSION 3.20)

project(JL_AC632N LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The firmware remains Makefile-based; this target exists only for CLion indexing.
set(JL_EXTERNAL_INCLUDE_DIR "" CACHE PATH
    "Optional q32s toolchain include directory")
if(NOT JL_EXTERNAL_INCLUDE_DIR)
    foreach(candidate
        "/opt/jieli/q32s/include"
        "C:/JL/pi32/q32s-include"
    )
        if(IS_DIRECTORY "${candidate}")
            set(JL_EXTERNAL_INCLUDE_DIR "${candidate}" CACHE PATH
                "Optional q32s toolchain include directory" FORCE)
            break()
        endif()
    endforeach()
endif()

set(JL_INDEX_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/driver"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/driver/device"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/driver/cpu/bd19"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/system"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/system/generic"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/system/device"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/system/fs"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/system/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/system/math/cpu/bd19"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/btctrler"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/btctrler/port/bd19"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/update"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/agreement"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/btstack/third_party/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/btstack/third_party/rcsp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/media/media_new"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/media/media_new/media"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/media/media_new/media/cpu/bd19"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/media/media_new/media/cpu/bd19/asm"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/board/bd19"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/device"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/power_manage"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/bd19"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/jieli"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/jieli/JL_rcsp"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/jieli/online_db"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/ai_stack/JL_rcsp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include_lib/btstack"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/jieli/JL_rcsp/rcsp_updata"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/jieli/gatt_common"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/Tecent_LL/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/Tecent_LL/tecent_ll_demo"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/examples/ll_sync"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/cJSON"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/tuya_protocol"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/tuya_protocol/app/demo"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/tuya_protocol/app/product_test"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/tuya_protocol/app/uart_common"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/tuya_protocol/extern_components/mbedtls"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/tuya_protocol/port"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/tuya_protocol/sdk/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/tuya_protocol/sdk/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/examples/tuya"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/hilink_protocol"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/hilink_protocol/mbedtls_protocol"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/hilink_protocol/mbedtls_protocol/mbedtls"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/hilink_protocol/mbedtls_protocol/mbedtls/configs"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/third_party_profile/hilink_protocol/mbedtls_protocol/mbedtls/psa"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/examples/hilink"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/bd19/audio_encode"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/bd19/audio_decode"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/device/usb"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/device/usb/device"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/device/usb/host"
)

if(IS_DIRECTORY "${JL_EXTERNAL_INCLUDE_DIR}")
    list(APPEND JL_INDEX_INCLUDE_DIRS "${JL_EXTERNAL_INCLUDE_DIR}")
endif()

file(GLOB_RECURSE JL_INDEX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/common/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/config/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/config/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/examples/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/examples/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/modules/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/modules/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/board/bd19/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/board/bd19/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/app_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/spp_and_le/version.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/bd19/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/bd19/*.h"
)

if(NOT JL_INDEX_SOURCES)
    message(FATAL_ERROR "No AC632N SPP_AND_LE sources found for CLion indexing")
endif()

add_library(ac632n_spp_and_le_indexing STATIC EXCLUDE_FROM_ALL ${JL_INDEX_SOURCES})
target_include_directories(ac632n_spp_and_le_indexing PRIVATE ${JL_INDEX_INCLUDE_DIRS})
target_compile_definitions(ac632n_spp_and_le_indexing PRIVATE
    JL_CLION_INDEX
    SUPPORT_MS_EXTENSIONS
    CONFIG_RELEASE_ENABLE
    CONFIG_CPU_BD19
    CONFIG_NEW_BREDR_ENABLE
    CONFIG_FREE_RTOS_ENABLE
    CONFIG_EQ_SUPPORT_ASYNC
    EQ_DESIGN_IN_MASK
    SINE_MAKE_IN_MASK
    CONFIG_MMU_ENABLE
    CONFIG_AEC_M=256
    CONFIG_AUDIO_ONCHIP
    CONFIG_MEDIA_NEW_ENABLE
    __GCC_Q32S__
    __Q32S__
    CONFIG_NEW_ECC_ENABLE
    CONFIG_CRYPTO_TOOLBOX_OSIZE_IN_MASKROM
    EVENT_HANDLER_NUM_CONFIG=2
    EVENT_TOUCH_ENABLE_CONFIG=0
    EVENT_POOL_SIZE_CONFIG=256
    CONFIG_EVENT_KEY_MAP_ENABLE=0
    TIMER_POOL_NUM_CONFIG=10
    APP_ASYNC_POOL_NUM_CONFIG=0
    SDK_VERSION_CFG_DEFINE=0x240001
    SDK_VERSION_DATE_DEFINE=20241231
    VFS_ENABLE=1
    USE_SDFILE_NEW=1
    VFS_FILE_POOL_NUM_CONFIG=0
    SDFILE_VERSION=0x020000
    VM_MAX_SIZE_CONFIG=16*1024
    VM_ITEM_MAX_NUM=256
    CONFIG_TWS_ENABLE
    CONFIG_SPP_AND_LE_CASE_ENABLE
    CONFIG_TRANSFER_ENABLE
    CONFIG_LITE_AEC_ENABLE=0
    CONFIG_LITE_AUDIO
    CONFIG_LMP_RESET_ENABLE
    CONFIG_LMP_REFRESH_ENCRYPTION_KEY_ENABLE
    CONFIG_LINK_DISTURB_SCAN_ENABLE=0
    CONFIG_BTCTRLER_TASK_DEL_ENABLE
    CONFIG_UPDATA_ENABLE
    CONFIG_OTA_UPDATA_ENABLE
    CONFIG_ITEM_FORMAT_VM
    TCFG_APP_BT_EN=1
    USB_PC_NO_APP_MODE
)
target_compile_options(ac632n_spp_and_le_indexing PRIVATE -fms-extensions)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${JL_INDEX_SOURCES})

set(JL_MAKE_COMMAND "make" CACHE STRING "Make command used by firmware targets")
add_custom_target(firmware_ac632n_spp_and_le
    COMMAND "${JL_MAKE_COMMAND}" ac632n_spp_and_le -j
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
)
add_custom_target(firmware_clean_ac632n_spp_and_le
    COMMAND "${JL_MAKE_COMMAND}" clean_ac632n_spp_and_le -j
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
)
