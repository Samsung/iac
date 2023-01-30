#ifndef IAMF_DEBUG_H
#define IAMF_DEBUG_H

#ifdef IA_DBG

#include <stdio.h>
#include <string.h>

#ifndef IA_TAG
#define IA_TAG "IAMF"
#endif

#define IA_DBG_E 0x01
#define IA_DBG_W 0x02
#define IA_DBG_I 0x04
#define IA_DBG_D 0x08
#define IA_DBG_T 0x10

#define IA_DBG_LEVEL 0

#ifdef IA_DEV
#include "IAMF_debug_dev.h"
#define obu_dump(a, b, c) OBU2F(a, b, c)
#else
#define obu_dump(a, b, c)
#endif

#ifndef __MODULE__
#define __MODULE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define ia_log(level, slevel, fmt, arg...) \
    do { \
        if (IA_DBG_LEVEL & level) { \
            fprintf(stderr, "[%-9s:%s:%s(%d):%s]>" fmt "\n", IA_TAG, slevel, __MODULE__, __LINE__, __FUNCTION__, ##arg); \
        } \
    } while (0)


#define ia_loge(fmt, arg...) ia_log(IA_DBG_E, "ERROR", fmt, ##arg)
#define ia_logw(fmt, arg...) ia_log(IA_DBG_W, "WARN ", fmt, ##arg)
#define ia_logi(fmt, arg...) ia_log(IA_DBG_I, "INFO ", fmt, ##arg)
#define ia_logd(fmt, arg...) ia_log(IA_DBG_D, "DEBUG", fmt, ##arg)
#define ia_logt(fmt, arg...) ia_log(IA_DBG_T, "TRACE", fmt, ##arg)


#else
#ifdef WIN32
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
#define obu_dump(a, b, c)

#endif

#endif /*  IAMF_DEBUG_H */
