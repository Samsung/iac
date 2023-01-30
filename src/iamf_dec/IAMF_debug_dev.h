#ifndef IAMF_DEBUG_DEV_H
#define IAMF_DEBUG_DEV_H

#ifdef IA_DBG_LEVEL
#undef IA_DBG_LEVEL
#endif

// #define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W)
// #define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W|IA_DBG_I)
#define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W|IA_DBG_I|IA_DBG_D)
// #define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W|IA_DBG_D|IA_DBG_I|IA_DBG_T)

#define DFN_SIZE 32


#define OBU2F(data, size, type) \
    do { \
            static int  g_obu_count = 0; \
            static char g_dump[DFN_SIZE]; \
            snprintf(g_dump, DFN_SIZE, "/tmp/obu/obu_%06d_%02d.dat", g_obu_count, type); \
            FILE *f = fopen(g_dump, "w"); \
            fwrite(data, 1, size, f); \
            fclose(f); \
            ++g_obu_count; \
    } while(0)



#endif /* IAMF_DEBUG_DEV_H */
