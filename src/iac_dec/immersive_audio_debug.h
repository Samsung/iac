#ifndef __IMMERSIVE_AUDIO_DEBUG_
#define __IMMERSIVE_AUDIO_DEBUG_

#ifdef IA_DBG

#include <stdio.h>
#include <string.h>

#ifndef IA_TAG
#define IA_TAG "IA"
#endif

#define IA_DBG_E 0x01
#define IA_DBG_W 0x02
#define IA_DBG_I 0x04
#define IA_DBG_D 0x08
#define IA_DBG_T 0x10

// #define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W)
// #define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W|IA_DBG_I)
#define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W|IA_DBG_I|IA_DBG_D)
// #define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W|IA_DBG_D|IA_DBG_I|IA_DBG_T)

#ifndef __MODULE__
#define __MODULE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define ia_log(level, slevel, fmt, arg...) \
    do { \
        if (IA_DBG_LEVEL & level) { \
            fprintf(stderr, "[%6s:%s:%s(%d):%s]>" fmt "\n", IA_TAG, slevel, __MODULE__, __LINE__, __FUNCTION__, ##arg); \
        } \
    } while (0)


#define ia_loge(fmt, arg...) ia_log(IA_DBG_E, "ERROR", fmt, ##arg)
#define ia_logw(fmt, arg...) ia_log(IA_DBG_W, "WARN ", fmt, ##arg)
#define ia_logi(fmt, arg...) ia_log(IA_DBG_I, "INFO ", fmt, ##arg)
#define ia_logd(fmt, arg...) ia_log(IA_DBG_D, "DEBUG", fmt, ##arg)
#define ia_logt(fmt, arg...) ia_log(IA_DBG_T, "TRACE", fmt, ##arg)

#define D2F(ch, buf, len, name) \
    do { \
        static int idx = 0; \
        if (idx < 10) { \
            char dump[32]; \
            snprintf(dump, 32, "/tmp/fdump%d_%s_%d.dat", ch, name, idx); \
            FILE *fd = fopen(dump, "w"); \
            fwrite(buf, 1, len, fd); \
            fclose(fd); \
            ++idx; \
        } \
    } while(0)


#else
# ifdef WIN32
#define ia_loge(fmt,...)
#define ia_logw(fmt,...)
#define ia_logi(fmt,...)
#define ia_logd(fmt,...)
#define ia_logt(fmt,...)
#else
#define ia_loge(fmt,arg...)
#define ia_logw(fmt,arg...)
#define ia_logi(fmt,arg...)
#define ia_logd(fmt,arg...)
#define ia_logt(fmt,arg...)
#endif

#define D2F(ch, buf, len, name)
#endif

#endif /*  __IMMERSIVE_AUDIO_DEBUG_ */
