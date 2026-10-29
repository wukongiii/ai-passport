/* Match application fonts/pool. This host display does not prove device timing. */
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_MEM_SIZE (32 * 1024U)
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_USE_LOG 1
#define LV_LOG_PRINTF 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_NULL 1
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();
