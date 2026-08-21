#include "system/includes.h"
#include "generic/log.h"

extern char __VERSION_BEGIN[];
extern char __VERSION_END[];

/** Print the application version records emitted by the linker. */
_INLINE_ int app_version_check(void)
{
    char *version;

    puts("=================Version===============\n");
    for (version = __VERSION_BEGIN; version < __VERSION_END;) {
        version += 4;
        printf("%s\n", version);
        version += strlen(version) + 1;
    }
    puts("=======================================\n");
    return 0;
}

/** Rider has no separately versioned media library. */
int lib_media_version(void)
{
    return 0;
}

/** Rider uses the SDK BLE stack without a product-side version shim. */
int lib_btstack_version(void)
{
    return 0;
}
