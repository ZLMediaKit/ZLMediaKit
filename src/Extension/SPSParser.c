#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> /* for uint32_t, etc */
#include "SPSParser.h"

/********************************************
*define here
********************************************/
#define SPS_PPS_DEBUG
//#undef  SPS_PPS_DEBUG


#define MAX_LEN 32
#define EXTENDED_SAR       255
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_SPS_COUNT    			32
#define MAX_LOG2_MAX_FRAME_NUM    	(12 + 4)
#define MIN_LOG2_MAX_FRAME_NUM    	4
#define H264_MAX_PICTURE_COUNT 		36
#define CODEC_FLAG2_IGNORE_CROP   0x00010000 ///< Discard cropping information from SPS.
#ifndef INT_MAX
#define INT_MAX						65535
#endif //INT_MAX

#ifndef FFMIN
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#endif
#ifndef FFMAX
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* report level */
#define RPT_ERR (1) // error, system error
#define RPT_WRN (2) // warning, maybe wrong, maybe OK
#define RPT_INF (3) // important information
#define RPT_DBG (4) // debug information

static int rpt_lvl = RPT_WRN; /* report level: ERR, WRN, INF, DBG */

/* report micro */
#define RPT(lvl, ...) \
    do { \
        if(lvl <= rpt_lvl) { \
            switch(lvl) { \
                case RPT_ERR: \
                    fprintf(stderr, "\"%s\" line %d [err]: ", __FILE__, __LINE__); \
                    break; \
                case RPT_WRN: \
                    fprintf(stderr, "\"%s\" line %d [wrn]: ", __FILE__, __LINE__); \
                    break; \
                case RPT_INF: \
                    fprintf(stderr, "\"%s\" line %d [inf]: ", __FILE__, __LINE__); \
                    break; \
                case RPT_DBG: \
                    fprintf(stderr, "\"%s\" line %d [dbg]: ", __FILE__, __LINE__); \
                    break; \
                default: \
                    fprintf(stderr, "\"%s\" line %d [???]: ", __FILE__, __LINE__); \
                    break; \
                } \
                fprintf(stderr, __VA_ARGS__); \
                fprintf(stderr, "\n"); \
        } \
    } while(0)

static const uint8_t sg_aau8DefaultScaling4[2][16] = {
    {  6, 13, 20, 28, 13, 20, 28, 32,
      20, 28, 32, 37, 28, 32, 37, 42 },
    { 10, 14, 20, 24, 14, 20, 24, 27,
      20, 24, 27, 30, 24, 27, 30, 34 }
};

static const uint8_t sg_aau8DefaultScaling8[2][64] = {
    {  6, 10, 13, 16, 18, 23, 25, 27,
      10, 11, 16, 18, 23, 25, 27, 29,
      13, 16, 18, 23, 25, 27, 29, 31,
      16, 18, 23, 25, 27, 29, 31, 33,
      18, 23, 25, 27, 29, 31, 33, 36,
      23, 25, 27, 29, 31, 33, 36, 38,
      25, 27, 29, 31, 33, 36, 38, 40,
      27, 29, 31, 33, 36, 38, 40, 42 },
    {  9, 13, 15, 17, 19, 21, 22, 24,
      13, 13, 17, 19, 21, 22, 24, 25,
      15, 17, 19, 21, 22, 24, 25, 27,
      17, 19, 21, 22, 24, 25, 27, 28,
      19, 21, 22, 24, 25, 27, 28, 30,
      21, 22, 24, 25, 27, 28, 30, 32,
      22, 24, 25, 27, 28, 30, 32, 33,
      24, 25, 27, 28, 30, 32, 33, 35 }
};

static const T_AVRational sg_atFfH264PixelSspect[17] = {
    {   0,  1 },
    {   1,  1 },
    {  12, 11 },
    {  10, 11 },
    {  16, 11 },
    {  40, 33 },
    {  24, 11 },
    {  20, 11 },
    {  32, 11 },
    {  80, 33 },
    {  18, 11 },
    {  15, 11 },
    {  64, 33 },
    { 160, 99 },
    {   4,  3 },
    {   3,  2 },
    {   2,  1 },
};

static const uint8_t sg_au8ZigzagScan[16+1] = {
    0 + 0 * 4, 1 + 0 * 4, 0 + 1 * 4, 0 + 2 * 4,
    1 + 1 * 4, 2 + 0 * 4, 3 + 0 * 4, 2 + 1 * 4,
    1 + 2 * 4, 0 + 3 * 4, 1 + 3 * 4, 2 + 2 * 4,
    3 + 1 * 4, 3 + 2 * 4, 2 + 3 * 4, 3 + 3 * 4,
};

const uint8_t g_au8FfZigzagDirect[64] = {
    0,   1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};


static const uint8_t hevc_sub_width_c[] = {
    1, 2, 2, 1
};

static const uint8_t hevc_sub_height_c[] = {
    1, 2, 1, 1
};

static const uint8_t default_scaling_list_intra[] = {
    16, 16, 16, 16, 17, 18, 21, 24,
    16, 16, 16, 16, 17, 19, 22, 25,
    16, 16, 17, 18, 20, 22, 25, 29,
    16, 16, 18, 21, 24, 27, 31, 36,
    17, 17, 20, 24, 30, 35, 41, 47,
    18, 19, 22, 27, 35, 44, 54, 65,
    21, 22, 25, 31, 41, 54, 70, 88,
    24, 25, 29, 36, 47, 65, 88, 115
};

static const uint8_t default_scaling_list_inter[] = {
    16, 16, 16, 16, 17, 18, 20, 24,
    16, 16, 16, 17, 18, 20, 24, 25,
    16, 16, 17, 18, 20, 24, 25, 28,
    16, 17, 18, 20, 24, 25, 28, 33,
    17, 18, 20, 24, 25, 28, 33, 41,
    18, 20, 24, 25, 28, 33, 41, 54,
    20, 24, 25, 28, 33, 41, 54, 71,
    24, 25, 28, 33, 41, 54, 71, 91
};


const uint8_t ff_hevc_diag_scan4x4_x[16] = {
    0, 0, 1, 0,
    1, 2, 0, 1,
    2, 3, 1, 2,
    3, 2, 3, 3,
};

const uint8_t ff_hevc_diag_scan4x4_y[16] = {
    0, 1, 0, 2,
    1, 0, 3, 2,
    1, 0, 3, 2,
    1, 3, 2, 3,
};

const uint8_t ff_hevc_diag_scan8x8_x[64] = {
    0, 0, 1, 0,
    1, 2, 0, 1,
    2, 3, 0, 1,
    2, 3, 4, 0,
    1, 2, 3, 4,
    5, 0, 1, 2,
    3, 4, 5, 6,
    0, 1, 2, 3,
    4, 5, 6, 7,
    1, 2, 3, 4,
    5, 6, 7, 2,
    3, 4, 5, 6,
    7, 3, 4, 5,
    6, 7, 4, 5,
    6, 7, 5, 6,
    7, 6, 7, 7,
};

const uint8_t ff_hevc_diag_scan8x8_y[64] = {
    0, 1, 0, 2,
    1, 0, 3, 2,
    1, 0, 4, 3,
    2, 1, 0, 5,
    4, 3, 2, 1,
    0, 6, 5, 4,
    3, 2, 1, 0,
    7, 6, 5, 4,
    3, 2, 1, 0,
    7, 6, 5, 4,
    3, 2, 1, 7,
    6, 5, 4, 3,
    2, 7, 6, 5,
    4, 3, 7, 6,
    5, 4, 7, 6,
    5, 7, 6, 7,
};

static const T_AVRational vui_sar[] = {
    {  0,   1 },
    {  1,   1 },
    { 12,  11 },
    { 10,  11 },
    { 16,  11 },
    { 40,  33 },
    { 24,  11 },
    { 20,  11 },
    { 32,  11 },
    { 80,  33 },
    { 18,  11 },
    { 15,  11 },
    { 64,  33 },
    { 160, 99 },
    {  4,   3 },
    {  3,   2 },
    {  2,   1 },
};

static inline int getBitsLeft(void *pvHandle)
{
	int iResLen = 0;
	T_GetBitContext *ptPtr = (T_GetBitContext *)pvHandle;
	if(ptPtr->iBufSize <= 0 || ptPtr->iTotalBit <= 0)
	{
		RPT(RPT_WRN, "buffer size is zero");
		return 0;
	}


	iResLen = ptPtr->iTotalBit - ptPtr->iBitPos;
    return iResLen;
}


/********************************************
*functions
********************************************/
/**
 *  @brief Function getOneBit()   ¶Á1¸öbit
 *  @param[in]     h     T_GetBitContext structrue
 *  @retval        0: success, -1 : failure
 *  @pre
 *  @post
 */
static int getOneBit(void *pvHandle)
{
    T_GetBitContext *ptPtr = (T_GetBitContext *)pvHandle;
    int iRet = 0;
    uint8_t *pu8CurChar = NULL;
    uint8_t u8Shift;
	int iResoLen = 0;

    if(NULL == ptPtr)
    {
        RPT(RPT_ERR, "NULL pointer");
        iRet = -1;
        goto exit;
    }
	iResoLen = getBitsLeft(ptPtr);
	if(iResoLen < 1)
	{
        iRet = -1;
        goto exit;
	}

    pu8CurChar = ptPtr->pu8Buf + (ptPtr->iBitPos >> 3);
    u8Shift = 7 - (ptPtr->iCurBitPos);
    ptPtr->iBitPos++;
    ptPtr->iCurBitPos = ptPtr->iBitPos & 0x7;
    iRet = ((*pu8CurChar) >> u8Shift) & 0x01;

exit:
    return iRet;
}


/**
 *  @brief Function getBits()  ¶Án¸öbits£¬n²»ÄÜ³¬¹ý32
 *  @param[in]     h     T_GetBitContext structrue
 *  @param[in]     n     how many bits you want?
 *  @retval        0: success, -1 : failure
 *  @pre
 *  @post
 */
static int getBits(void *pvHandle, int iN)
{
    T_GetBitContext *ptPtr = (T_GetBitContext *)pvHandle;
    uint8_t au8Temp[5] = {0};
    uint8_t *pu8CurChar = NULL;
    uint8_t u8Nbyte;
    uint8_t u8Shift;
    uint32_t u32Result = 0;
    int iRet = 0;
	int iResoLen = 0;

    if(NULL == ptPtr)
    {
        RPT(RPT_ERR, "NULL pointer");
        iRet = -1;
        goto exit;
    }

    if(iN > MAX_LEN)
    {
        iN = MAX_LEN;
    }

	iResoLen = getBitsLeft(ptPtr);
	if(iResoLen < iN)
	{
        iRet = -1;
        goto exit;
	}


    if((ptPtr->iBitPos + iN) > ptPtr->iTotalBit)
    {
        iN = ptPtr->iTotalBit- ptPtr->iBitPos;
    }

    pu8CurChar = ptPtr->pu8Buf+ (ptPtr->iBitPos>>3);
    u8Nbyte = (ptPtr->iCurBitPos + iN + 7) >> 3;
    u8Shift = (8 - (ptPtr->iCurBitPos + iN))& 0x07;

    if(iN == MAX_LEN)
    {
        RPT(RPT_DBG, "12(ptPtr->iBitPos(:%d) + iN(:%d)) > ptPtr->iTotalBit(:%d)!!! ",\
                ptPtr->iBitPos, iN, ptPtr->iTotalBit);
        RPT(RPT_DBG, "0x%x 0x%x 0x%x 0x%x", (*pu8CurChar), *(pu8CurChar+1),*(pu8CurChar+2),*(pu8CurChar+3));
    }

    memcpy(&au8Temp[5-u8Nbyte], pu8CurChar, u8Nbyte);
    iRet = (uint32_t)au8Temp[0] << 24;
    iRet = iRet << 8;
    iRet = ((uint32_t)au8Temp[1]<<24)|((uint32_t)au8Temp[2] << 16)\
                        |((uint32_t)au8Temp[3] << 8)|au8Temp[4];

    iRet = (iRet >> u8Shift) & (((uint64_t)1<<iN) - 1);

    u32Result = iRet;
    ptPtr->iBitPos += iN;
    ptPtr->iCurBitPos = ptPtr->iBitPos & 0x7;

exit:
    return u32Result;
}


/**
 * Show 1-25 bits.
 */
static inline unsigned int showBits(void *pvHandle, int iN)
{
    T_GetBitContext *ptPtr = (T_GetBitContext *)pvHandle;
    uint8_t au8Temp[5] = {0};
    uint8_t *pu8CurChar = NULL;
    uint8_t u8Nbyte;
    uint8_t u8Shift;
    uint32_t u32Result = 0;
    int iRet = 0;
	int iResoLen = 0;

    if(NULL == ptPtr)
    {
        RPT(RPT_ERR, "NULL pointer");
        iRet = -1;
        goto exit;
    }

    if(iN > MAX_LEN)
    {
        iN = MAX_LEN;
    }

	iResoLen = getBitsLeft(ptPtr);
	if(iResoLen < iN)
	{
        iRet = -1;
        goto exit;
	}


    if((ptPtr->iBitPos + iN) > ptPtr->iTotalBit)
    {
        iN = ptPtr->iTotalBit- ptPtr->iBitPos;
    }

    pu8CurChar = ptPtr->pu8Buf+ (ptPtr->iBitPos>>3);
    u8Nbyte = (ptPtr->iCurBitPos + iN + 7) >> 3;
    u8Shift = (8 - (ptPtr->iCurBitPos + iN))& 0x07;

    if(iN == MAX_LEN)
    {
        RPT(RPT_DBG, "12(ptPtr->iBitPos(:%d) + iN(:%d)) > ptPtr->iTotalBit(:%d)!!! ",\
                ptPtr->iBitPos, iN, ptPtr->iTotalBit);
        RPT(RPT_DBG, "0x%x 0x%x 0x%x 0x%x", (*pu8CurChar), *(pu8CurChar+1),*(pu8CurChar+2),*(pu8CurChar+3));
    }

    memcpy(&au8Temp[5-u8Nbyte], pu8CurChar, u8Nbyte);
    iRet = (uint32_t)au8Temp[0] << 24;
    iRet = iRet << 8;
    iRet = ((uint32_t)au8Temp[1]<<24)|((uint32_t)au8Temp[2] << 16)\
                        |((uint32_t)au8Temp[3] << 8)|au8Temp[4];

    iRet = (iRet >> u8Shift) & (((uint64_t)1<<iN) - 1);

    u32Result = iRet;
//    ptPtr->iBitPos += iN;
//    ptPtr->iCurBitPos = ptPtr->iBitPos & 0x7;

exit:
    return u32Result;
}



/**
 * Show 0-32 bits.
 */
static inline unsigned int showBitsLong(void *pvHandle, int iN)
{
	T_GetBitContext *ptPtr = (T_GetBitContext *)pvHandle;

    if (iN <= 32) {
        return showBits(ptPtr, iN);
    } 
}



/**
 *  @brief Function parseCodenum() Ö¸Êý¸çÂ×²¼±àÂë½âÎö£¬²Î¿¼h264±ê×¼µÚ9½Ú
 *  @param[in]     buf
 *  @retval        u32CodeNum
 *  @pre
 *  @post
 */
static int parseCodenum(void *pvBuf)
{
    uint8_t u8LeadingZeroBits = -1;
    uint8_t u8B;
    uint32_t u32CodeNum = 0;

    for(u8B=0; !u8B; u8LeadingZeroBits++)
    {
        u8B = getOneBit(pvBuf);
    }

    u32CodeNum = ((uint32_t)1 << u8LeadingZeroBits) - 1 + getBits(pvBuf, u8LeadingZeroBits);

    return u32CodeNum;
}

/**
 *  @brief Function parseUe() Ö¸Êý¸çÂ×²¼±àÂë½âÎö ue(),²Î¿¼h264±ê×¼µÚ9½Ú
 *  @param[in]     buf       sps_pps parse buf
 *  @retval        u32CodeNum
 *  @pre
 *  @post
 */
static int parseUe(void *pvBuf)
{
    return parseCodenum(pvBuf);
}


/**
 *  @brief Function parseSe() Ö¸Êý¸çÂ×²¼±àÂë½âÎö se(), ²Î¿¼h264±ê×¼µÚ9½Ú
 *  @param[in]     buf       sps_pps parse buf
 *  @retval        u32CodeNum
 *  @pre
 *  @post
 */
static int parseSe(void *pvBuf)
{
    int iRet = 0;
    int u32CodeNum;

    u32CodeNum = parseCodenum(pvBuf);
    iRet = (u32CodeNum + 1) >> 1;
    iRet = (u32CodeNum & 0x01)? iRet : -iRet;

    return iRet;
}


/**
 *  @brief Function getBitContextFree()  ÉêÇëµÄget_bit_context½á¹¹ÄÚ´æÊÍ·Å
 *  @param[in]     buf       T_GetBitContext buf
 *  @retval        none
 *  @pre
 *  @post
 */
static void getBitContextFree(void *pvBuf)
{
    T_GetBitContext *ptPtr = (T_GetBitContext *)pvBuf;

    if(ptPtr)
    {
        if(ptPtr->pu8Buf)
        {
            free(ptPtr->pu8Buf);
        }

        free(ptPtr);
    }
}




/**
 *  @brief Function deEmulationPrevention()  ½â¾ºÕù´úÂë
 *  @param[in]     buf       T_GetBitContext buf
 *  @retval        none
 *  @pre
 *  @post
 *  @note:
 *      µ÷ÊÔÊ±×ÜÊÇ·¢ÏÖvui.time_scaleÖµÌØ±ðÆæ¹Ö£¬×ÜÊÇ16777216£¬ºóÀ´²éÑ¯Ô­ÒòÈçÏÂ:
 *      http://www.cnblogs.com/eustoma/archive/2012/02/13/2415764.html
 *      H.264±àÂëÊ±£¬ÔÚÃ¿¸öNALÇ°Ìí¼ÓÆðÊ¼Âë 0x000001£¬½âÂëÆ÷ÔÚÂëÁ÷ÖÐ¼ì²âµ½ÆðÊ¼Âë£¬µ±Ç°NAL½áÊø¡£
 *      ÎªÁË·ÀÖ¹NALÄÚ²¿³öÏÖ0x000001µÄÊý¾Ý£¬h.264ÓÖÌá³ö'·ÀÖ¹¾ºÕù emulation prevention"»úÖÆ£¬
 *      ÔÚ±àÂëÍêÒ»¸öNALÊ±£¬Èç¹û¼ì²â³öÓÐÁ¬ÐøÁ½¸ö0x00×Ö½Ú£¬¾ÍÔÚºóÃæ²åÈëÒ»¸ö0x03¡£
 *      µ±½âÂëÆ÷ÔÚNALÄÚ²¿¼ì²âµ½0x000003µÄÊý¾Ý£¬¾Í°Ñ0x03Å×Æú£¬»Ö¸´Ô­Ê¼Êý¾Ý¡£
 *      0x000000  >>>>>>  0x00000300
 *      0x000001  >>>>>>  0x00000301
 *      0x000002  >>>>>>  0x00000302
 *      0x000003  >>>>>>  0x00000303
 */
static void *deEmulationPrevention(void *pvBuf)
{
    T_GetBitContext *ptPtr = NULL;
    T_GetBitContext *ptBufPtr = (T_GetBitContext *)pvBuf;
    int i = 0, j = 0;
    uint8_t *pu8TmpPtr = NULL;
    int tmp_buf_size = 0;
    int iVal = 0;

    if(NULL == ptBufPtr)
    {
        RPT(RPT_ERR, "NULL ptPtr");
        goto exit;
    }

    ptPtr = (T_GetBitContext *)malloc(sizeof(T_GetBitContext));
    if(NULL == ptPtr)
    {
        RPT(RPT_ERR, "NULL ptPtr");
        goto exit;
    }

    memcpy(ptPtr, ptBufPtr, sizeof(T_GetBitContext));

    ptPtr->pu8Buf = (uint8_t *)malloc(ptPtr->iBufSize);
    if(NULL == ptPtr->pu8Buf)
    {
        RPT(RPT_ERR, "NULL ptPtr");
        goto exit;
    }

    memcpy(ptPtr->pu8Buf, ptBufPtr->pu8Buf, ptBufPtr->iBufSize);

    pu8TmpPtr = ptPtr->pu8Buf;
    tmp_buf_size = ptPtr->iBufSize;
    for(i=0; i<(tmp_buf_size-2); i++)
    {
        /*¼ì²â0x000003*/
        iVal = (pu8TmpPtr[i]^0x00) + (pu8TmpPtr[i+1]^0x00) + (pu8TmpPtr[i+2]^0x03);
        if(iVal == 0)
        {
            /*ÌÞ³ý0x03*/
            for(j=i+2; j<tmp_buf_size-1; j++)
            {
                pu8TmpPtr[j] = pu8TmpPtr[j+1];
            }

            /*ÏàÓ¦µÄbufsizeÒª¼õÐ¡*/
            ptPtr->iBufSize--;
        }
    }

    /*ÖØÐÂ¼ÆËãtotal_bit*/
    ptPtr->iTotalBit = ptPtr->iBufSize << 3;

    return (void *)ptPtr;

exit:
    getBitContextFree(ptPtr);
    return NULL;
}

static void decodeScalingList(void *pvBuf, uint8_t *pu8Factors, int iSize,
                                const uint8_t *pu8JvtList,
                                const uint8_t *pu8FallbackList)
{
    int i;
	int iLast = 8;
	int iNext = 8;
    const uint8_t *pu8Scan = iSize == 16 ? sg_au8ZigzagScan : g_au8FfZigzagDirect;

    if (!getOneBit(pvBuf)) /* matrix not written, we use the predicted one */
        memcpy(pu8Factors, pu8FallbackList, iSize * sizeof(uint8_t));
    else
        for (i = 0; i < iSize; i++)
		{
            if (iNext)
            {

                iNext = (iLast + parseSe(pvBuf)) & 0xff;
            }
            if (!i && !iNext)
			{
				/* matrix not written, we use the preset one */
                memcpy(pu8Factors, pu8JvtList, iSize * sizeof(uint8_t));
                break;
            }
            iLast = pu8Factors[pu8Scan[i]] = iNext ? iNext : iLast;
        }
}


 int decodeScalingMatrices(void *pvBuf, T_SPS *ptSps,
                                    T_PPS *ptPps, int iIsSps,
                                    uint8_t(*pau8ScalingMatrix4)[16],
                                    uint8_t(*pau8ScalingMatrix8)[64])
{
    int iFallbackSps = !iIsSps && ptSps->iScalingMatrixPresent;
    const uint8_t *pau8Fallback[4] = {
        iFallbackSps ? ptSps->aau8ScalingMatrix4[0] : sg_aau8DefaultScaling4[0],
        iFallbackSps ? ptSps->aau8ScalingMatrix4[3] : sg_aau8DefaultScaling4[1],
        iFallbackSps ? ptSps->aau8ScalingMatrix8[0] : sg_aau8DefaultScaling8[0],
        iFallbackSps ? ptSps->aau8ScalingMatrix8[3] : sg_aau8DefaultScaling8[1]
    };
    if (getOneBit(pvBuf)) {
        ptSps->iScalingMatrixPresent |= iIsSps;
        decodeScalingList(pvBuf, pau8ScalingMatrix4[0], 16, sg_aau8DefaultScaling4[0], pau8Fallback[0]);        // Intra, Y
        decodeScalingList(pvBuf, pau8ScalingMatrix4[1], 16, sg_aau8DefaultScaling4[0], pau8ScalingMatrix4[0]); // Intra, Cr
		decodeScalingList(pvBuf, pau8ScalingMatrix4[2], 16, sg_aau8DefaultScaling4[0], pau8ScalingMatrix4[1]); // Intra, Cb

		decodeScalingList(pvBuf, pau8ScalingMatrix4[3], 16, sg_aau8DefaultScaling4[1], pau8Fallback[1]);        // Inter, Y

		decodeScalingList(pvBuf, pau8ScalingMatrix4[4], 16, sg_aau8DefaultScaling4[1], pau8ScalingMatrix4[3]); // Inter, Cr

		decodeScalingList(pvBuf, pau8ScalingMatrix4[5], 16, sg_aau8DefaultScaling4[1], pau8ScalingMatrix4[4]); // Inter, Cb


        if (iIsSps || ptPps->iTransform8x8Mode)
		{
            decodeScalingList(pvBuf, pau8ScalingMatrix8[0], 64, sg_aau8DefaultScaling8[0], pau8Fallback[2]); // Intra, Y
            decodeScalingList(pvBuf, pau8ScalingMatrix8[3], 64, sg_aau8DefaultScaling8[1], pau8Fallback[3]); // Inter, Y
            if (ptSps->iChromaFormatIdc == 3) {
                decodeScalingList(pvBuf, pau8ScalingMatrix8[1], 64, sg_aau8DefaultScaling8[0], pau8ScalingMatrix8[0]); // Intra, Cr
				decodeScalingList(pvBuf, pau8ScalingMatrix8[4], 64, sg_aau8DefaultScaling8[1], pau8ScalingMatrix8[3]); // Inter, Cr
				decodeScalingList(pvBuf, pau8ScalingMatrix8[2], 64, sg_aau8DefaultScaling8[0], pau8ScalingMatrix8[1]); // Intra, Cb
				decodeScalingList(pvBuf, pau8ScalingMatrix8[5], 64, sg_aau8DefaultScaling8[1], pau8ScalingMatrix8[4]); // Inter, Cb
			}
        }
    }
	return 0;
}

static  int decodeHrdPAarameters(void *pvBuf, T_SPS *ptSps)
{
    int iCpbCount = 0;
	int i;
    iCpbCount = parseUe(pvBuf);

    if (iCpbCount > 32U)
	{
		RPT(RPT_ERR,"iCpbCount %d invalid\n", iCpbCount);
		return -1;

    }

    getBits(pvBuf, 4); /* bit_rate_scale */
    getBits(pvBuf, 4); /* cpb_size_scale */
    for (i = 0; i < iCpbCount; i++)
	{
		parseUe(pvBuf);
		parseUe(pvBuf);
        //get_ue_golomb_long(&h->gb); /* bit_rate_value_minus1 */
        //get_ue_golomb_long(&h->gb); /* cpb_size_value_minus1 */
        getOneBit(pvBuf);          /* cbr_flag */
    }
    ptSps->iInitialCpbRemovalDelayLength = getBits(pvBuf, 5) + 1;
    ptSps->iCpbRemovalDelayLength         = getBits(pvBuf, 5) + 1;
    ptSps->iDpbOutputDelayLength          = getBits(pvBuf, 5) + 1;
    ptSps->iTimeOffsetLength               = getBits(pvBuf, 5);
    ptSps->iCpbCnt                          = iCpbCount;
    return 0;
}


static inline int decodeVuiParameters(void *pvBuf, T_SPS *ptSps)
{
    int iAspectRatioInfoPresentFlag;
    unsigned int uiAspectRatioIdc;
	int iChromaSampleLocation;

    iAspectRatioInfoPresentFlag = getOneBit(pvBuf);

    if (iAspectRatioInfoPresentFlag) {
        uiAspectRatioIdc = getBits(pvBuf, 8);
        if (uiAspectRatioIdc == EXTENDED_SAR) {
            ptSps->tSar.num = getBits(pvBuf, 16);
            ptSps->tSar.den = getBits(pvBuf, 16);
        } else if (uiAspectRatioIdc < FF_ARRAY_ELEMS(sg_atFfH264PixelSspect)) {
            ptSps->tSar = sg_atFfH264PixelSspect[uiAspectRatioIdc];
        } else
        {
        	RPT(RPT_ERR,"illegal aspect ratio\n");
            return -1;
        }
    } else
    {
        ptSps->tSar.num =
        ptSps->tSar.den = 0;
    }

    if (getOneBit(pvBuf))      /* overscan_info_present_flag */
        getOneBit(pvBuf);      /* overscan_appropriate_flag */

    ptSps->iVideoSignalTypePresentFlag = getOneBit(pvBuf);
    if (ptSps->iVideoSignalTypePresentFlag) {
        getBits(pvBuf, 3);                 /* video_format */
        ptSps->iFullRange = getOneBit(pvBuf); /* video_full_range_flag */

        ptSps->iColourDescriptionPresentFlag = getOneBit(pvBuf);
        if (ptSps->iColourDescriptionPresentFlag) {
            ptSps->tColorPrimaries = getBits(pvBuf, 8); /* colour_primaries */
            ptSps->tColorTrc       = getBits(pvBuf, 8); /* transfer_characteristics */
            ptSps->tColorspace      = getBits(pvBuf, 8); /* matrix_coefficients */
            if (ptSps->tColorPrimaries >= AVCOL_PRI_NB)
                ptSps->tColorPrimaries = AVCOL_PRI_UNSPECIFIED;
            if (ptSps->tColorTrc >= AVCOL_TRC_NB)
                ptSps->tColorTrc = AVCOL_TRC_UNSPECIFIED;
            if (ptSps->tColorspace >= AVCOL_SPC_NB)
                ptSps->tColorspace = AVCOL_SPC_UNSPECIFIED;
        }
    }

    /* chroma_location_info_present_flag */
    if (getOneBit(pvBuf))
	{
        /* chroma_sample_location_type_top_field */
        iChromaSampleLocation = parseUe(pvBuf);
        parseUe(pvBuf);  /* chroma_sample_location_type_bottom_field */
    }
	if(getBitsLeft(pvBuf) < 10)
	{
		return 0;
	}


    ptSps->iTimingInfoPresentFlag = getOneBit(pvBuf);
    if (ptSps->iTimingInfoPresentFlag) {
        unsigned u32NumUnitsInTick = getBits(pvBuf, 32);
        unsigned u32TimeScale        = getBits(pvBuf, 32);
        if (!u32NumUnitsInTick || !u32TimeScale) {

			RPT(RPT_ERR,"u32TimeScale/u32NumUnitsInTick invalid or unsupported (%u/%u)\n",u32TimeScale, u32NumUnitsInTick);
            ptSps->iTimingInfoPresentFlag = 0;
        } else {
            ptSps->u32NumUnitsInTick = u32NumUnitsInTick;
            ptSps->u32TimeScale = u32TimeScale;
        }
        ptSps->iFixedFrameRateFlag = getOneBit(pvBuf);
    }

    ptSps->iNalHrdParametersPresentFlag = getOneBit(pvBuf);
    if (ptSps->iNalHrdParametersPresentFlag)
        if (decodeHrdPAarameters(pvBuf, ptSps) < 0)
            return -1;
    ptSps->iVclHrdParametersPresentFlag = getOneBit(pvBuf);
    if (ptSps->iVclHrdParametersPresentFlag)
        if (decodeHrdPAarameters(pvBuf, ptSps) < 0)
            return -1;
    if (ptSps->iNalHrdParametersPresentFlag ||
        ptSps->iVclHrdParametersPresentFlag)
        getOneBit(pvBuf);     /* low_delay_hrd_flag */
    ptSps->iPicStructPresentFlag = getOneBit(pvBuf);

	if(getBitsLeft(pvBuf) == 0)
		return 0;
    ptSps->iBitstreamRestrictionFlag = getOneBit(pvBuf);
    if (ptSps->iBitstreamRestrictionFlag) {
        getOneBit(pvBuf);     /* motion_vectors_over_pic_boundaries_flag */
		parseUe(pvBuf);
        //get_ue_golomb(&h->gb); /* max_bytes_per_pic_denom */
        parseUe(pvBuf);
        //get_ue_golomb(&h->gb); /* max_bits_per_mb_denom */
        parseUe(pvBuf);
        //get_ue_golomb(&h->gb); /* log2_max_mv_length_horizontal */
        parseUe(pvBuf);
        //get_ue_golomb(&h->gb); /* log2_max_mv_length_vertical */
        ptSps->iNumReorderFrames = parseUe(pvBuf);

		parseUe(pvBuf);
        //get_ue_golomb(&h->gb); /*max_dec_frame_buffering*/

        if (getBitsLeft(pvBuf) < 0)
		{
            ptSps->iNumReorderFrames         = 0;
            ptSps->iBitstreamRestrictionFlag = 0;
        }

        if (ptSps->iNumReorderFrames > 16U
            /* max_dec_frame_buffering || max_dec_frame_buffering > 16 */)
        {
			RPT(RPT_DBG, "Clipping illegal iNumReorderFrames %d\n",
                   ptSps->iNumReorderFrames);
            ptSps->iNumReorderFrames = 16;
            return -1;
        }
    }

    return 0;
}


int h264DecSeqParameterSet(void *pvBufSrc, T_SPS *ptSps)
{
    int iLevelIdc;
	int iConstraintSetFlags = 0;
    unsigned int uiSpsId;
    int i;
	int iLog2MaxFrameNumMinus4;

    int iRet = 0;
    int iProfileIdc = 0;
    void *pvBuf = NULL;



    if(NULL == pvBufSrc || NULL == ptSps)
    {
        RPT(RPT_ERR,"ERR null pointer\n");
        iRet = -1;
        goto exit;
    }

    memset((void *)ptSps, 0, sizeof(T_SPS));

    pvBuf = deEmulationPrevention(pvBufSrc);
    if(NULL == pvBuf)
    {
        RPT(RPT_ERR,"ERR null pointer\n");
        iRet = -1;
        goto exit;
    }

    iProfileIdc           = getBits(pvBuf, 8);
    iConstraintSetFlags |= getOneBit(pvBuf) << 0;   // constraint_set0_flag
    iConstraintSetFlags |= getOneBit(pvBuf) << 1;   // constraint_set1_flag
    iConstraintSetFlags |= getOneBit(pvBuf) << 2;   // constraint_set2_flag
    iConstraintSetFlags |= getOneBit(pvBuf) << 3;   // constraint_set3_flag
    iConstraintSetFlags |= getOneBit(pvBuf) << 4;   // constraint_set4_flag
    iConstraintSetFlags |= getOneBit(pvBuf) << 5;   // constraint_set5_flag
    getBits(pvBuf, 2);                            // reserved_zero_2bits
    iLevelIdc = getBits(pvBuf, 8);
    uiSpsId    = parseUe(pvBuf);

    if (uiSpsId >= MAX_SPS_COUNT) {
        RPT(RPT_ERR, "uiSpsId %u out of range\n", uiSpsId);
        iRet = -1;
        goto exit;

    }


    ptSps->uiSpsId               = uiSpsId;
    ptSps->iTimeOffsetLength   = 24;
    ptSps->iProfileIdc          = iProfileIdc;
    ptSps->iConstraintSetFlags = iConstraintSetFlags;
    ptSps->iLevelIdc            = iLevelIdc;
    ptSps->iFullRange           = -1;

    memset(ptSps->aau8ScalingMatrix4, 16, sizeof(ptSps->aau8ScalingMatrix4));
    memset(ptSps->aau8ScalingMatrix8, 16, sizeof(ptSps->aau8ScalingMatrix8));
    ptSps->iScalingMatrixPresent = 0;
    ptSps->tColorspace = 2; //AVCOL_SPC_UNSPECIFIED

    if (ptSps->iProfileIdc == 100 ||  // High profile
        ptSps->iProfileIdc == 110 ||  // High10 profile
        ptSps->iProfileIdc == 122 ||  // High422 profile
        ptSps->iProfileIdc == 244 ||  // High444 Predictive profile
        ptSps->iProfileIdc ==  44 ||  // Cavlc444 profile
        ptSps->iProfileIdc ==  83 ||  // Scalable Constrained High profile (SVC)
        ptSps->iProfileIdc ==  86 ||  // Scalable High Intra profile (SVC)
        ptSps->iProfileIdc == 118 ||  // Stereo High profile (MVC)
        ptSps->iProfileIdc == 128 ||  // Multiview High profile (MVC)
        ptSps->iProfileIdc == 138 ||  // Multiview Depth High profile (MVCD)
        ptSps->iProfileIdc == 144) {  // old High444 profile
        ptSps->iChromaFormatIdc = parseUe(pvBuf);
        if (ptSps->iChromaFormatIdc > 3U)
		{
            RPT(RPT_ERR, "iChromaFormatIdc %u",ptSps->iChromaFormatIdc);
        	iRet = -1;
        	goto exit;
        }
		else if (ptSps->iChromaFormatIdc == 3)
		{
            ptSps->iResidualColorTransformFlag = getOneBit(pvBuf);
            if (ptSps->iResidualColorTransformFlag)
			{
				RPT(RPT_ERR, "separate color planes are not supported\n");
				iRet = -1;
				goto exit;

            }
        }
        ptSps->iBitDepthLuma   = parseUe(pvBuf) + 8;
        ptSps->iBitDepthChroma = parseUe(pvBuf) + 8;
        if (ptSps->iBitDepthChroma != ptSps->iBitDepthLuma)
		{
			RPT(RPT_ERR, "Different chroma and luma bit depth");
			iRet = -1;
			goto exit;

        }
        if (ptSps->iBitDepthLuma   < 8 || ptSps->iBitDepthLuma   > 14 ||
            ptSps->iBitDepthChroma < 8 || ptSps->iBitDepthChroma > 14)
        {
			RPT(RPT_ERR, "illegal bit depth value (%d, %d)\n",ptSps->iBitDepthLuma, ptSps->iBitDepthChroma);
			iRet = -1;
			goto exit;
        }
        ptSps->iTransformBypass = getOneBit(pvBuf);
        decodeScalingMatrices(pvBuf, ptSps, NULL, 1,
                                ptSps->aau8ScalingMatrix4, ptSps->aau8ScalingMatrix8);
    }
	else
	{

        ptSps->iChromaFormatIdc = 1;
        ptSps->iBitDepthLuma    = 8;
        ptSps->iBitDepthChroma  = 8;
    }

    iLog2MaxFrameNumMinus4 = parseUe(pvBuf);

    if (iLog2MaxFrameNumMinus4 < MIN_LOG2_MAX_FRAME_NUM - 4 ||
        iLog2MaxFrameNumMinus4 > MAX_LOG2_MAX_FRAME_NUM - 4)
    {
		RPT(RPT_ERR, "iLog2MaxFrameNumMinus4 out of range (0-12): %d\n", iLog2MaxFrameNumMinus4);
		iRet = -1;
		goto exit;

    }
    ptSps->iLog2MaxFrameNum = iLog2MaxFrameNumMinus4 + 4;

    ptSps->iPocType = parseUe(pvBuf);

    if (ptSps->iPocType == 0)
	{
		// FIXME #define
        unsigned t = parseUe(pvBuf);
        if (t>12)
		{
			RPT(RPT_ERR, "iLog2MaxPocLsb (%d) is out of range\n", t);
			iRet = -1;
			goto exit;

        }
        ptSps->iLog2MaxPocLsb = t + 4;
    }
	else if (ptSps->iPocType == 1)
	{
		// FIXME #define
        ptSps->iDeltaPicOrderAlwaysZeroFlag = getOneBit(pvBuf);
        ptSps->iOffsetForNonRefPic           = parseSe(pvBuf);
        ptSps->iOffsetForTopToBottomField   = parseSe(pvBuf);
        ptSps->iPocCycleLength                 = parseUe(pvBuf);

        if ((unsigned)ptSps->iPocCycleLength >= FF_ARRAY_ELEMS(ptSps->asOffsetForRefFrame))
        {
			RPT(RPT_ERR, "iPocCycleLength overflow %d\n", ptSps->iPocCycleLength);
			iRet = -1;
			goto exit;

        }

        for (i = 0; i < ptSps->iPocCycleLength; i++)
            ptSps->asOffsetForRefFrame[i] = parseSe(pvBuf);
    }
	else if (ptSps->iPocType != 2)
	{
		RPT(RPT_ERR, "illegal POC type %d\n", ptSps->iPocType);
		iRet = -1;
		goto exit;

    }

    ptSps->iRefFrameCount = parseUe(pvBuf);
    if (ptSps->iRefFrameCount > H264_MAX_PICTURE_COUNT - 2 ||
        ptSps->iRefFrameCount > 16U)
    {
		RPT(RPT_ERR, "too many reference frames %d\n", ptSps->iRefFrameCount);
		iRet = -1;
		goto exit;

    }
    ptSps->iGapsInFrameNumAllowedFlag = getOneBit(pvBuf);
    ptSps->iMbWidth                       = parseUe(pvBuf) + 1;
    ptSps->iMbHeight                      = parseUe(pvBuf) + 1;


    ptSps->iFrameMbsOnlyFlag = getOneBit(pvBuf);
    if (!ptSps->iFrameMbsOnlyFlag)
        ptSps->iMbAff = getOneBit(pvBuf);
    else
        ptSps->iMbAff = 0;

    ptSps->iDirect8x8InferenceFlag = getOneBit(pvBuf);

    ptSps->iCrop = getOneBit(pvBuf);
    if (ptSps->iCrop) {
        unsigned int uiCropLeft   = parseUe(pvBuf);
        unsigned int uiCropRight  = parseUe(pvBuf);
        unsigned int uiCropTop    = parseUe(pvBuf);
        unsigned int uiCropBottom = parseUe(pvBuf);
        int width  = 16 * ptSps->iMbWidth;
        int height = 16 * ptSps->iMbHeight * (2 - ptSps->iFrameMbsOnlyFlag);

		if(1)
		{
            int vsub   = (ptSps->iChromaFormatIdc == 1) ? 1 : 0;
            int hsub   = (ptSps->iChromaFormatIdc == 1 ||
                          ptSps->iChromaFormatIdc == 2) ? 1 : 0;
            int step_x = 1 << hsub;
            int step_y = (2 - ptSps->iFrameMbsOnlyFlag) << vsub;

            if (uiCropLeft & (0x1F >> (ptSps->iBitDepthLuma > 8)))
			{
                uiCropLeft &= ~(0x1F >> (ptSps->iBitDepthLuma > 8));
            }

            if (uiCropLeft  > (unsigned)INT_MAX / 4 / step_x ||
                uiCropRight > (unsigned)INT_MAX / 4 / step_x ||
                uiCropTop   > (unsigned)INT_MAX / 4 / step_y ||
                uiCropBottom> (unsigned)INT_MAX / 4 / step_y ||
                (uiCropLeft + uiCropRight ) * step_x >= width ||
                (uiCropTop  + uiCropBottom) * step_y >= height
            )
            {
				RPT(RPT_ERR, "crop values invalid %d %d %d %d / %d %d\n", uiCropLeft, uiCropRight, uiCropTop, uiCropBottom, width, height);
				iRet = -1;
				goto exit;
            }

            ptSps->uiCropLeft   = uiCropLeft   * step_x;
            ptSps->uiCropRight  = uiCropRight  * step_x;
            ptSps->uiCropTop    = uiCropTop    * step_y;
            ptSps->uiCropBottom = uiCropBottom * step_y;
        }
    }
	else
	{
        ptSps->uiCropLeft   =
        ptSps->uiCropRight  =
        ptSps->uiCropTop    =
        ptSps->uiCropBottom =
        ptSps->iCrop        = 0;
    }

    ptSps->iVuiParametersPresentFlag = getOneBit(pvBuf);
    if (ptSps->iVuiParametersPresentFlag) {
        int ret = decodeVuiParameters(pvBuf, ptSps);
        if (ret < 0)
            goto exit;
    }

    if (getBitsLeft(pvBuf) < 0)
	{
		RPT(RPT_ERR, "Overread %s by %d bits\n", ptSps->iVuiParametersPresentFlag ? "VUI" : "SPS", -getBitsLeft(pvBuf));
		iRet = -1;
    }

    if (!ptSps->tSar.den)
        ptSps->tSar.den = 1;

    ptSps->iNew = 1;

exit:
#ifdef SPS_PPS_DEBUG

    if (1)
	{
        static const char csp[4][5] = { "Gray", "420", "422", "444" };
        RPT(RPT_DBG,
               "ptSps:%u profile:%d/%d poc:%d ref:%d %dx%d %s %s crop:%u/%u/%u/%u %s %s %d/%d b%d reo:%d\n",
               uiSpsId, ptSps->iProfileIdc, ptSps->iLevelIdc,
               ptSps->iPocType,
               ptSps->iRefFrameCount,
               ptSps->iMbWidth, ptSps->iMbHeight,
               ptSps->iFrameMbsOnlyFlag ? "FRM" : (ptSps->iMbAff ? "MB-AFF" : "PIC-AFF"),
               ptSps->iDirect8x8InferenceFlag ? "8B8" : "",
               ptSps->uiCropLeft, ptSps->uiCropRight,
               ptSps->uiCropTop, ptSps->uiCropBottom,
               ptSps->iVuiParametersPresentFlag ? "VUI" : "",
               csp[ptSps->iChromaFormatIdc],
               ptSps->iTimingInfoPresentFlag ? ptSps->u32NumUnitsInTick : 0,
               ptSps->iTimingInfoPresentFlag ? ptSps->u32TimeScale : 0,
               ptSps->iBitDepthLuma,
               ptSps->iBitstreamRestrictionFlag ? ptSps->iNumReorderFrames : -1
               );
    }

#endif
    getBitContextFree(pvBuf);

    return iRet;
}


static int decodeProfileTierLevel(T_GetBitContext *pvBuf, T_PTLCommon *ptl)
{
    int i;

    if (getBitsLeft(pvBuf) < 2+1+5 + 32 + 4 + 16 + 16 + 12)
        return -1;

    ptl->profile_space = getBits(pvBuf, 2);
    ptl->tier_flag     = getOneBit(pvBuf);
    ptl->profile_idc   = getBits(pvBuf, 5);
    if (ptl->profile_idc == T_PROFILE_HEVC_MAIN)
        RPT(RPT_DBG, "Main profile bitstream\n");
    else if (ptl->profile_idc == T_PROFILE_HEVC_MAIN_10)
        RPT(RPT_DBG, "Main 10 profile bitstream\n");
    else if (ptl->profile_idc == T_PROFILE_HEVC_MAIN_STILL_PICTURE)
        RPT(RPT_DBG, "Main Still Picture profile bitstream\n");
    else if (ptl->profile_idc == T_PROFILE_HEVC_REXT)
        RPT(RPT_DBG, "Range Extension profile bitstream\n");
    else
        RPT(RPT_WRN, "Unknown HEVC profile: %d\n", ptl->profile_idc);

    for (i = 0; i < 32; i++) {
        ptl->profile_compatibility_flag[i] = getOneBit(pvBuf);

        if (ptl->profile_idc == 0 && i > 0 && ptl->profile_compatibility_flag[i])
            ptl->profile_idc = i;
    }
    ptl->progressive_source_flag    = getOneBit(pvBuf);
    ptl->interlaced_source_flag     = getOneBit(pvBuf);
    ptl->non_packed_constraint_flag = getOneBit(pvBuf);
    ptl->frame_only_constraint_flag = getOneBit(pvBuf);

    getBits(pvBuf, 16); // XXX_reserved_zero_44bits[0..15]
    getBits(pvBuf, 16); // XXX_reserved_zero_44bits[16..31]
    getBits(pvBuf, 12); // XXX_reserved_zero_44bits[32..43]

    return 0;
}
									  

static int parsePtl(T_GetBitContext *pvBuf, T_PTL *ptl, int max_num_sub_layers)
{
    int i;
    if (decodeProfileTierLevel(pvBuf, &ptl->general_ptl) < 0 ||
        getBitsLeft(pvBuf) < 8 + (8*2 * (max_num_sub_layers - 1 > 0))) {
        RPT(RPT_ERR, "PTL information too short\n");
        return -1;
    }

    ptl->general_ptl.level_idc = getBits(pvBuf, 8);

    for (i = 0; i < max_num_sub_layers - 1; i++) {
        ptl->sub_layer_profile_present_flag[i] = getOneBit(pvBuf);
        ptl->sub_layer_level_present_flag[i]   = getOneBit(pvBuf);
    }

    if (max_num_sub_layers - 1> 0)
        for (i = max_num_sub_layers - 1; i < 8; i++)
            getBits(pvBuf, 2); // reserved_zero_2bits[i]
    for (i = 0; i < max_num_sub_layers - 1; i++) {
        if (ptl->sub_layer_profile_present_flag[i] &&
            decodeProfileTierLevel(pvBuf, &ptl->sub_layer_ptl[i]) < 0) {
            RPT(RPT_ERR,
                   "PTL information for sublayer %i too short\n", i);
            return -1;
        }
        if (ptl->sub_layer_level_present_flag[i]) {
            if (getBitsLeft(pvBuf) < 8) {
                RPT(RPT_ERR,
                       "Not enough data for sublayer %i level_idc\n", i);
                return -1;
            } else
                ptl->sub_layer_ptl[i].level_idc = getBits(pvBuf, 8);
        }
    }

    return 0;
}
					  

static void setDefaultScalingListData(T_ScalingList *sl)
{
    int matrixId;

    for (matrixId = 0; matrixId < 6; matrixId++) {
        // 4x4 default is 16
        memset(sl->sl[0][matrixId], 16, 16);
        sl->sl_dc[0][matrixId] = 16; // default for 16x16
        sl->sl_dc[1][matrixId] = 16; // default for 32x32
    }
    memcpy(sl->sl[1][0], default_scaling_list_intra, 64);
    memcpy(sl->sl[1][1], default_scaling_list_intra, 64);
    memcpy(sl->sl[1][2], default_scaling_list_intra, 64);
    memcpy(sl->sl[1][3], default_scaling_list_inter, 64);
    memcpy(sl->sl[1][4], default_scaling_list_inter, 64);
    memcpy(sl->sl[1][5], default_scaling_list_inter, 64);
    memcpy(sl->sl[2][0], default_scaling_list_intra, 64);
    memcpy(sl->sl[2][1], default_scaling_list_intra, 64);
    memcpy(sl->sl[2][2], default_scaling_list_intra, 64);
    memcpy(sl->sl[2][3], default_scaling_list_inter, 64);
    memcpy(sl->sl[2][4], default_scaling_list_inter, 64);
    memcpy(sl->sl[2][5], default_scaling_list_inter, 64);
    memcpy(sl->sl[3][0], default_scaling_list_intra, 64);
    memcpy(sl->sl[3][1], default_scaling_list_intra, 64);
    memcpy(sl->sl[3][2], default_scaling_list_intra, 64);
    memcpy(sl->sl[3][3], default_scaling_list_inter, 64);
    memcpy(sl->sl[3][4], default_scaling_list_inter, 64);
    memcpy(sl->sl[3][5], default_scaling_list_inter, 64);
}

static int scalingListData(T_GetBitContext *pvBuf, T_ScalingList *sl, T_HEVCSPS *sps)
{
    uint8_t scaling_list_pred_mode_flag;
    int32_t scaling_list_dc_coef[2][6];
    int size_id, matrix_id, pos;
    int i;

    for (size_id = 0; size_id < 4; size_id++)
        for (matrix_id = 0; matrix_id < 6; matrix_id += ((size_id == 3) ? 3 : 1)) {
            scaling_list_pred_mode_flag = getOneBit(pvBuf);
            if (!scaling_list_pred_mode_flag) {
                unsigned int delta = parseUe(pvBuf);
                /* Only need to handle non-zero delta. Zero means default,
                 * which should already be in the arrays. */
                if (delta) {
                    // Copy from previous array.
                    delta *= (size_id == 3) ? 3 : 1;
                    if (matrix_id < delta) {
                        RPT(RPT_ERR,
                               "Invalid delta in scaling list data: %d.\n", delta);
                        return -1;
                    }

                    memcpy(sl->sl[size_id][matrix_id],
                           sl->sl[size_id][matrix_id - delta],
                           size_id > 0 ? 64 : 16);
                    if (size_id > 1)
                        sl->sl_dc[size_id - 2][matrix_id] = sl->sl_dc[size_id - 2][matrix_id - delta];
                }
            } else {
                int next_coef, coef_num;
                int32_t scaling_list_delta_coef;

                next_coef = 8;
                coef_num  = FFMIN(64, 1 << (4 + (size_id << 1)));
                if (size_id > 1) {
                    scaling_list_dc_coef[size_id - 2][matrix_id] = parseSe(pvBuf) + 8;
                    next_coef = scaling_list_dc_coef[size_id - 2][matrix_id];
                    sl->sl_dc[size_id - 2][matrix_id] = next_coef;
                }
                for (i = 0; i < coef_num; i++) {
                    if (size_id == 0)
                        pos = 4 * ff_hevc_diag_scan4x4_y[i] +
                                  ff_hevc_diag_scan4x4_x[i];
                    else
                        pos = 8 * ff_hevc_diag_scan8x8_y[i] +
                                  ff_hevc_diag_scan8x8_x[i];

                    scaling_list_delta_coef = parseSe(pvBuf);
                    next_coef = (next_coef + 256U + scaling_list_delta_coef) % 256;
                    sl->sl[size_id][matrix_id][pos] = next_coef;
                }
            }
        }

    if (sps->chroma_format_idc == 3) {
        for (i = 0; i < 64; i++) {
            sl->sl[3][1][i] = sl->sl[2][1][i];
            sl->sl[3][2][i] = sl->sl[2][2][i];
            sl->sl[3][4][i] = sl->sl[2][4][i];
            sl->sl[3][5][i] = sl->sl[2][5][i];
        }
        sl->sl_dc[1][1] = sl->sl_dc[0][1];
        sl->sl_dc[1][2] = sl->sl_dc[0][2];
        sl->sl_dc[1][4] = sl->sl_dc[0][4];
        sl->sl_dc[1][5] = sl->sl_dc[0][5];
    }


    return 0;
}

int hevcDecodeShortTermRps(T_GetBitContext *pvBuf,
                                  T_ShortTermRPS *rps, const T_HEVCSPS *sps, int is_slice_header)
{
    uint8_t rps_predict = 0;
    int delta_poc;
    int k0 = 0;
    int k1 = 0;
    int k  = 0;
    int i;

    if (rps != sps->st_rps && sps->nb_st_rps)
        rps_predict = getOneBit(pvBuf);

    if (rps_predict) {
        const T_ShortTermRPS *rps_ridx;
        int delta_rps;
        unsigned abs_delta_rps;
        uint8_t use_delta_flag = 0;
        uint8_t delta_rps_sign;

        if (is_slice_header) {
            unsigned int delta_idx = parseUe(pvBuf) + 1;
            if (delta_idx > sps->nb_st_rps) {
                RPT(RPT_ERR,
                       "Invalid value of delta_idx in slice header RPS: %d > %d.\n",
                       delta_idx, sps->nb_st_rps);
                return -1;
            }
            rps_ridx = &sps->st_rps[sps->nb_st_rps - delta_idx];
            rps->rps_idx_num_delta_pocs = rps_ridx->num_delta_pocs;
        } else
            rps_ridx = &sps->st_rps[rps - sps->st_rps - 1];

        delta_rps_sign = getOneBit(pvBuf);
        abs_delta_rps  = parseUe(pvBuf) + 1;
        if (abs_delta_rps < 1 || abs_delta_rps > 32768) {
            RPT(RPT_ERR,
                   "Invalid value of abs_delta_rps: %d\n",
                   abs_delta_rps);
            return -1;
        }
        delta_rps      = (1 - (delta_rps_sign << 1)) * abs_delta_rps;
        for (i = 0; i <= rps_ridx->num_delta_pocs; i++) {
            int used = rps->used[k] = getOneBit(pvBuf);

            if (!used)
                use_delta_flag = getOneBit(pvBuf);

            if (used || use_delta_flag) {
                if (i < rps_ridx->num_delta_pocs)
                    delta_poc = delta_rps + rps_ridx->delta_poc[i];
                else
                    delta_poc = delta_rps;
                rps->delta_poc[k] = delta_poc;
                if (delta_poc < 0)
                    k0++;
                else
                    k1++;
                k++;
            }
        }

        if (k >= FF_ARRAY_ELEMS(rps->used)) {
            RPT(RPT_ERR,
                   "Invalid num_delta_pocs: %d\n", k);
            return -1;
        }

        rps->num_delta_pocs    = k;
        rps->num_negative_pics = k0;
        // sort in increasing order (smallest first)
        if (rps->num_delta_pocs != 0) {
            int used, tmp;
            for (i = 1; i < rps->num_delta_pocs; i++) {
                delta_poc = rps->delta_poc[i];
                used      = rps->used[i];
                for (k = i - 1; k >= 0; k--) {
                    tmp = rps->delta_poc[k];
                    if (delta_poc < tmp) {
                        rps->delta_poc[k + 1] = tmp;
                        rps->used[k + 1]      = rps->used[k];
                        rps->delta_poc[k]     = delta_poc;
                        rps->used[k]          = used;
                    }
                }
            }
        }
        if ((rps->num_negative_pics >> 1) != 0) {
            int used;
            k = rps->num_negative_pics - 1;
            // flip the negative values to largest first
            for (i = 0; i < rps->num_negative_pics >> 1; i++) {
                delta_poc         = rps->delta_poc[i];
                used              = rps->used[i];
                rps->delta_poc[i] = rps->delta_poc[k];
                rps->used[i]      = rps->used[k];
                rps->delta_poc[k] = delta_poc;
                rps->used[k]      = used;
                k--;
            }
        }
    } else {
        unsigned int prev, nb_positive_pics;
        rps->num_negative_pics = parseUe(pvBuf);
        nb_positive_pics       = parseUe(pvBuf);

        if (rps->num_negative_pics >= HEVC_MAX_REFS ||
            nb_positive_pics >= HEVC_MAX_REFS) {
            RPT(RPT_ERR, "Too many refs in a short term RPS.\n");
            return -1;
        }

        rps->num_delta_pocs = rps->num_negative_pics + nb_positive_pics;
        if (rps->num_delta_pocs) {
            prev = 0;
            for (i = 0; i < rps->num_negative_pics; i++) {
                delta_poc = parseUe(pvBuf) + 1;
                if (delta_poc < 1 || delta_poc > 32768) {
                    RPT(RPT_ERR,
                        "Invalid value of delta_poc: %d\n",
                        delta_poc);
                    return -1;
                }
                prev -= delta_poc;
                rps->delta_poc[i] = prev;
                rps->used[i]      = getOneBit(pvBuf);
            }
            prev = 0;
            for (i = 0; i < nb_positive_pics; i++) {
                delta_poc = parseUe(pvBuf) + 1;
                if (delta_poc < 1 || delta_poc > 32768) {
                    RPT(RPT_ERR,
                        "Invalid value of delta_poc: %d\n",
                        delta_poc);
                    return -1;
                }
                prev += delta_poc;
                rps->delta_poc[rps->num_negative_pics + i] = prev;
                rps->used[rps->num_negative_pics + i]      = getOneBit(pvBuf);
            }
        }
    }
    return 0;
}

static void decodeSublayerHrd(T_GetBitContext *pvBuf, unsigned int nb_cpb,
                                int subpic_params_present)
{
    int i;

    for (i = 0; i < nb_cpb; i++) {
        parseUe(pvBuf); // bit_rate_value_minus1
        parseUe(pvBuf); // cpb_size_value_minus1

        if (subpic_params_present) {
            parseUe(pvBuf); // cpb_size_du_value_minus1
            parseUe(pvBuf); // bit_rate_du_value_minus1
        }
        getOneBit(pvBuf); // cbr_flag
    }
}

static int decodeHrd(T_GetBitContext *pvBuf, int common_inf_present,
                       int max_sublayers)
{
    int nal_params_present = 0, vcl_params_present = 0;
    int subpic_params_present = 0;
    int i;

    if (common_inf_present) {
        nal_params_present = getOneBit(pvBuf);
        vcl_params_present = getOneBit(pvBuf);

        if (nal_params_present || vcl_params_present) {
            subpic_params_present = getOneBit(pvBuf);

            if (subpic_params_present) {
                getBits(pvBuf, 8); // tick_divisor_minus2
                getBits(pvBuf, 5); // du_cpb_removal_delay_increment_length_minus1
                getBits(pvBuf, 1); // sub_pic_cpb_params_in_pic_timing_sei_flag
                getBits(pvBuf, 5); // dpb_output_delay_du_length_minus1
            }

            getBits(pvBuf, 4); // bit_rate_scale
            getBits(pvBuf, 4); // cpb_size_scale

            if (subpic_params_present)
                getBits(pvBuf, 4);  // cpb_size_du_scale

            getBits(pvBuf, 5); // initial_cpb_removal_delay_length_minus1
            getBits(pvBuf, 5); // au_cpb_removal_delay_length_minus1
            getBits(pvBuf, 5); // dpb_output_delay_length_minus1
        }
    }

    for (i = 0; i < max_sublayers; i++) {
        int low_delay = 0;
        unsigned int nb_cpb = 1;
        int fixed_rate = getOneBit(pvBuf);

        if (!fixed_rate)
            fixed_rate = getOneBit(pvBuf);

        if (fixed_rate)
            parseUe(pvBuf);  // elemental_duration_in_tc_minus1
        else
            low_delay = getOneBit(pvBuf);

        if (!low_delay) {
            nb_cpb = parseUe(pvBuf) + 1;
            if (nb_cpb < 1 || nb_cpb > 32) {
                RPT(RPT_ERR, "nb_cpb %d invalid\n", nb_cpb);
                return -1;
            }
        }

        if (nal_params_present)
            decodeSublayerHrd(pvBuf, nb_cpb, subpic_params_present);
        if (vcl_params_present)
            decodeSublayerHrd(pvBuf, nb_cpb, subpic_params_present);
    }
    return 0;
}

					   

static void decodeVui(T_GetBitContext *pvBuf, T_HEVCSPS *sps)
{
    T_VUI backup_vui, *vui = &sps->vui;
    T_GetBitContext backup;
    int sar_present, alt = 0;

    RPT(RPT_DBG, "Decoding VUI\n");

    sar_present = getOneBit(pvBuf);
    if (sar_present) {
        uint8_t sar_idx = getBits(pvBuf, 8);
        if (sar_idx < FF_ARRAY_ELEMS(vui_sar))
            vui->sar = vui_sar[sar_idx];
        else if (sar_idx == 255) {
            vui->sar.num = getBits(pvBuf, 16);
            vui->sar.den = getBits(pvBuf, 16);
        } else
            RPT(RPT_WRN,
                   "Unknown SAR index: %u.\n", sar_idx);
    }

    vui->overscan_info_present_flag = getOneBit(pvBuf);
    if (vui->overscan_info_present_flag)
        vui->overscan_appropriate_flag = getOneBit(pvBuf);

    vui->video_signal_type_present_flag = getOneBit(pvBuf);
    if (vui->video_signal_type_present_flag) {
        vui->video_format                    = getBits(pvBuf, 3);
        vui->video_full_range_flag           = getOneBit(pvBuf);
        vui->colour_description_present_flag = getOneBit(pvBuf);
//        if (vui->video_full_range_flag && sps->pix_fmt == AV_PIX_FMT_YUV420P)
//            sps->pix_fmt = AV_PIX_FMT_YUVJ420P;
        if (vui->colour_description_present_flag) {
            vui->colour_primaries        = getBits(pvBuf, 8);
            vui->transfer_characteristic = getBits(pvBuf, 8);
            vui->matrix_coeffs           = getBits(pvBuf, 8);

#if 0
            // Set invalid values to "unspecified"
            if (!av_color_primaries_name(vui->colour_primaries))
                vui->colour_primaries = AVCOL_PRI_UNSPECIFIED;
            if (!av_color_transfer_name(vui->transfer_characteristic))
                vui->transfer_characteristic = AVCOL_TRC_UNSPECIFIED;
            if (!av_color_space_name(vui->matrix_coeffs))
                vui->matrix_coeffs = AVCOL_SPC_UNSPECIFIED;
            if (vui->matrix_coeffs == AVCOL_SPC_RGB) {
                switch (sps->pix_fmt) {
                case AV_PIX_FMT_YUV444P:
                    sps->pix_fmt = AV_PIX_FMT_GBRP;
                    break;
                case AV_PIX_FMT_YUV444P10:
                    sps->pix_fmt = AV_PIX_FMT_GBRP10;
                    break;
                case AV_PIX_FMT_YUV444P12:
                    sps->pix_fmt = AV_PIX_FMT_GBRP12;
                    break;
                }
            }
#endif
        }
    }

    vui->chroma_loc_info_present_flag = getOneBit(pvBuf);
    if (vui->chroma_loc_info_present_flag) {
        vui->chroma_sample_loc_type_top_field    = parseUe(pvBuf);
        vui->chroma_sample_loc_type_bottom_field = parseUe(pvBuf);
    }

    vui->neutra_chroma_indication_flag = getOneBit(pvBuf);
    vui->field_seq_flag                = getOneBit(pvBuf);
    vui->frame_field_info_present_flag = getOneBit(pvBuf);

    // Backup context in case an alternate header is detected
    memcpy(&backup, pvBuf, sizeof(backup));
    memcpy(&backup_vui, vui, sizeof(backup_vui));
    if (getBitsLeft(pvBuf) >= 68 && showBitsLong(pvBuf, 21) == 0x100000) {
        vui->default_display_window_flag = 0;
        RPT(RPT_WRN, "Invalid default display window\n");
    } else
        vui->default_display_window_flag = getOneBit(pvBuf);

    if (vui->default_display_window_flag) {
        int vert_mult  = hevc_sub_height_c[sps->chroma_format_idc];
        int horiz_mult = hevc_sub_width_c[sps->chroma_format_idc];
        vui->def_disp_win.left_offset   = parseUe(pvBuf) * horiz_mult;
        vui->def_disp_win.right_offset  = parseUe(pvBuf) * horiz_mult;
        vui->def_disp_win.top_offset    = parseUe(pvBuf) *  vert_mult;
        vui->def_disp_win.bottom_offset = parseUe(pvBuf) *  vert_mult;    
    }

timing_info:
    vui->vui_timing_info_present_flag = getOneBit(pvBuf);

    if (vui->vui_timing_info_present_flag) {
        if( getBitsLeft(pvBuf) < 66 && !alt) {
            // The alternate syntax seem to have timing info located
            // at where def_disp_win is normally located
            RPT(RPT_WRN,
                   "Strange VUI timing information, retrying...\n");
            memcpy(vui, &backup_vui, sizeof(backup_vui));
            memcpy(pvBuf, &backup, sizeof(backup));
            alt = 1;
            goto timing_info;
        }
        vui->vui_num_units_in_tick               = getBits(pvBuf, 32);
        vui->vui_time_scale                      = getBits(pvBuf, 32);
        if (alt) {
            RPT(RPT_INF, "Retry got %u/%u fps\n",
                   vui->vui_time_scale, vui->vui_num_units_in_tick);
        }
        vui->vui_poc_proportional_to_timing_flag = getOneBit(pvBuf);
        if (vui->vui_poc_proportional_to_timing_flag)
            vui->vui_num_ticks_poc_diff_one_minus1 = parseUe(pvBuf);
        vui->vui_hrd_parameters_present_flag = getOneBit(pvBuf);
        if (vui->vui_hrd_parameters_present_flag)
            decodeHrd(pvBuf, 1, sps->max_sub_layers);
    }

    vui->bitstream_restriction_flag = getOneBit(pvBuf);
    if (vui->bitstream_restriction_flag) {
        if (getBitsLeft(pvBuf) < 8 && !alt) {
            RPT(RPT_WRN,
                   "Strange VUI bitstream restriction information, retrying"
                   " from timing information...\n");
            memcpy(vui, &backup_vui, sizeof(backup_vui));
            memcpy(pvBuf, &backup, sizeof(backup));
            alt = 1;
            goto timing_info;
        }
        vui->tiles_fixed_structure_flag              = getOneBit(pvBuf);
        vui->motion_vectors_over_pic_boundaries_flag = getOneBit(pvBuf);
        vui->restricted_ref_pic_lists_flag           = getOneBit(pvBuf);
        vui->min_spatial_segmentation_idc            = parseUe(pvBuf);
        vui->max_bytes_per_pic_denom                 = parseUe(pvBuf);
        vui->max_bits_per_min_cu_denom               = parseUe(pvBuf);
        vui->log2_max_mv_length_horizontal           = parseUe(pvBuf);
        vui->log2_max_mv_length_vertical             = parseUe(pvBuf);
    }

    if (getBitsLeft(pvBuf) < 1 && !alt) {
        // XXX: Alternate syntax when sps_range_extension_flag != 0?
        RPT(RPT_WRN,
               "Overread in VUI, retrying from timing information...\n");
        memcpy(vui, &backup_vui, sizeof(backup_vui));
        memcpy(pvBuf, &backup, sizeof(backup));
        alt = 1;
        goto timing_info;
    }
}



int h265DecSeqParameterSet( void *pvBufSrc, T_HEVCSPS *sps )
{
    T_HEVCWindow *ow;
//    int ret = 0;
    int log2_diff_max_min_transform_block_size;
    int bit_depth_chroma, start, vui_present, sublayer_ordering_info;
    int i;
    int iRet = 0;


    void *pvBuf = NULL;
    if(NULL == pvBufSrc || NULL == sps)
    {
        RPT(RPT_ERR,"ERR null pointer\n");
        iRet = -1;
        goto exit;
    }

    memset((void *)sps, 0, sizeof(T_HEVCSPS));

    pvBuf = deEmulationPrevention(pvBufSrc);
    if(NULL == pvBuf)
    {
        RPT(RPT_ERR,"ERR null pointer\n");
        iRet = -1;
        goto exit;
    }
	

    // Coded parameters

    sps->vps_id = getBits(pvBuf, 4);
    if (sps->vps_id >= HEVC_MAX_VPS_COUNT) {
        RPT(RPT_ERR, "VPS id out of range: %d\n", sps->vps_id);
		iRet = -1;
		goto exit;
    }

#if 0
    if (vps_list && !vps_list[sps->vps_id]) {
        RPT(RPT_ERR, "VPS %d does not exist\n",
               sps->vps_id);
        return AVERROR_INVALIDDATA;
    }
#endif

    sps->max_sub_layers = getBits(pvBuf, 3) + 1;
    if (sps->max_sub_layers > HEVC_MAX_SUB_LAYERS) {
        RPT(RPT_ERR, "sps_max_sub_layers out of range: %d\n",
               sps->max_sub_layers);
		iRet = -1;
		goto exit;
    }

    sps->temporal_id_nesting_flag = getBits(pvBuf, 1);
    if ((iRet = parsePtl(pvBuf, &sps->ptl, sps->max_sub_layers)) < 0)
        goto exit;

    int sps_id = parseUe(pvBuf);
    if (sps_id >= HEVC_MAX_SPS_COUNT) {
        RPT(RPT_ERR, "SPS id out of range: %d\n", sps_id);
		iRet = -1;
		goto exit;
    }

    sps->chroma_format_idc = parseUe(pvBuf);
    if (sps->chroma_format_idc > 3U) {
        RPT(RPT_ERR, "chroma_format_idc %d is invalid\n", sps->chroma_format_idc);
		iRet = -1;
		goto exit;
    }

    if (sps->chroma_format_idc == 3)
        sps->separate_colour_plane_flag = getOneBit(pvBuf);

    if (sps->separate_colour_plane_flag)
        sps->chroma_format_idc = 0;

    sps->width  = parseUe(pvBuf);
    sps->height = parseUe(pvBuf);
	
//    if ((iRet = av_image_check_size(sps->width,
//                                   sps->height, 0, avctx)) < 0)
//        goto exit;

    if (getOneBit(pvBuf)) { // pic_conformance_flag
        int vert_mult  = hevc_sub_height_c[sps->chroma_format_idc];
        int horiz_mult = hevc_sub_width_c[sps->chroma_format_idc];
        sps->pic_conf_win.left_offset   = parseUe(pvBuf) * horiz_mult;
        sps->pic_conf_win.right_offset  = parseUe(pvBuf) * horiz_mult;
        sps->pic_conf_win.top_offset    = parseUe(pvBuf) *  vert_mult;
        sps->pic_conf_win.bottom_offset = parseUe(pvBuf) *  vert_mult;

        sps->output_window = sps->pic_conf_win;
    }

    sps->bit_depth   = parseUe(pvBuf) + 8;
    bit_depth_chroma = parseUe(pvBuf) + 8;
	
    if (sps->chroma_format_idc && bit_depth_chroma != sps->bit_depth) {
        RPT(RPT_ERR,
               "Luma bit depth (%d) is different from chroma bit depth (%d), "
               "this is unsupported.\n",
               sps->bit_depth, bit_depth_chroma);
		iRet = -1;
		goto exit;
    }
    sps->bit_depth_chroma = bit_depth_chroma;

#if 0
    iRet = map_pixel_format(avctx, sps);
    if (iRet < 0)
        goto exit;
#endif

#if 0

    sps->log2_max_poc_lsb = parseUe(pvBuf) + 4;
    if (sps->log2_max_poc_lsb > 16) {
        RPT(RPT_ERR, "log2_max_pic_order_cnt_lsb_minus4 out range: %d\n",
               sps->log2_max_poc_lsb - 4);
		iRet = -1;
		goto exit;
    }

    sublayer_ordering_info = getOneBit(pvBuf);
    start = sublayer_ordering_info ? 0 : sps->max_sub_layers - 1;
    for (i = start; i < sps->max_sub_layers; i++) {
        sps->temporal_layer[i].max_dec_pic_buffering = parseUe(pvBuf) + 1;
        sps->temporal_layer[i].num_reorder_pics      = parseUe(pvBuf);
        sps->temporal_layer[i].max_latency_increase  = parseUe(pvBuf) - 1;
        if (sps->temporal_layer[i].max_dec_pic_buffering > (unsigned)HEVC_MAX_DPB_SIZE) {
            RPT(RPT_ERR, "sps_max_dec_pic_buffering_minus1 out of range: %d\n",
                   sps->temporal_layer[i].max_dec_pic_buffering - 1U);
			iRet = -1;
			goto exit;
        }
        if (sps->temporal_layer[i].num_reorder_pics > sps->temporal_layer[i].max_dec_pic_buffering - 1) {
            RPT(RPT_WRN, "sps_max_num_reorder_pics out of range: %d\n",
                   sps->temporal_layer[i].num_reorder_pics);
            if (sps->temporal_layer[i].num_reorder_pics > HEVC_MAX_DPB_SIZE - 1) {
				iRet = -1;
				goto exit;
            }
            sps->temporal_layer[i].max_dec_pic_buffering = sps->temporal_layer[i].num_reorder_pics + 1;
        }
    }

    if (!sublayer_ordering_info) {
        for (i = 0; i < start; i++) {
            sps->temporal_layer[i].max_dec_pic_buffering = sps->temporal_layer[start].max_dec_pic_buffering;
            sps->temporal_layer[i].num_reorder_pics      = sps->temporal_layer[start].num_reorder_pics;
            sps->temporal_layer[i].max_latency_increase  = sps->temporal_layer[start].max_latency_increase;
        }
    }

    sps->log2_min_cb_size                    = parseUe(pvBuf) + 3;
    sps->log2_diff_max_min_coding_block_size = parseUe(pvBuf);
    sps->log2_min_tb_size                    = parseUe(pvBuf) + 2;
    log2_diff_max_min_transform_block_size   = parseUe(pvBuf);
    sps->log2_max_trafo_size                 = log2_diff_max_min_transform_block_size +
                                               sps->log2_min_tb_size;

    if (sps->log2_min_cb_size < 3 || sps->log2_min_cb_size > 30) {
        RPT(RPT_ERR, "Invalid value %d for log2_min_cb_size", sps->log2_min_cb_size);
		iRet = -1;
		goto exit;
    }

    if (sps->log2_diff_max_min_coding_block_size > 30) {
        RPT(RPT_ERR, "Invalid value %d for log2_diff_max_min_coding_block_size", sps->log2_diff_max_min_coding_block_size);
		iRet = -1;
		goto exit;
    }

    if (sps->log2_min_tb_size >= sps->log2_min_cb_size || sps->log2_min_tb_size < 2) {
        RPT(RPT_ERR, "Invalid value for log2_min_tb_size");
		iRet = -1;
		goto exit;
    }

    if (log2_diff_max_min_transform_block_size < 0 || log2_diff_max_min_transform_block_size > 30) {
        RPT(RPT_ERR, "Invalid value %d for log2_diff_max_min_transform_block_size", log2_diff_max_min_transform_block_size);
		iRet = -1;
		goto exit;
    }

    sps->max_transform_hierarchy_depth_inter = parseUe(pvBuf);
    sps->max_transform_hierarchy_depth_intra = parseUe(pvBuf);

    sps->scaling_list_enable_flag = getOneBit(pvBuf);

	printf("sps->scaling_list_enable_flag: %d\n",sps->scaling_list_enable_flag);
	
    if (sps->scaling_list_enable_flag) {
        setDefaultScalingListData(&sps->scaling_list);

        if (getOneBit(pvBuf)) {
            iRet = scalingListData(pvBuf, &sps->scaling_list, sps);
            if (iRet < 0)
                goto exit;
        }
    }

    sps->amp_enabled_flag = getOneBit(pvBuf);
    sps->sao_enabled      = getOneBit(pvBuf);

    sps->pcm_enabled_flag = getOneBit(pvBuf);
	
	printf("sps->pcm_enabled_flag: %d\n",sps->pcm_enabled_flag);
    if (sps->pcm_enabled_flag) {
        sps->pcm.bit_depth   = getBits(pvBuf, 4) + 1;
        sps->pcm.bit_depth_chroma = getBits(pvBuf, 4) + 1;
        sps->pcm.log2_min_pcm_cb_size = parseUe(pvBuf) + 3;
        sps->pcm.log2_max_pcm_cb_size = sps->pcm.log2_min_pcm_cb_size +
                                        parseUe(pvBuf);
        if (FFMAX(sps->pcm.bit_depth, sps->pcm.bit_depth_chroma) > sps->bit_depth) {
            RPT(RPT_ERR,
                   "PCM bit depth (%d, %d) is greater than normal bit depth (%d)\n",
                   sps->pcm.bit_depth, sps->pcm.bit_depth_chroma, sps->bit_depth);
			iRet = -1;
			goto exit;
        }

        sps->pcm.loop_filter_disable_flag = getOneBit(pvBuf);
    }

    sps->nb_st_rps = parseUe(pvBuf);
    if (sps->nb_st_rps > HEVC_MAX_SHORT_TERM_REF_PIC_SETS) {
        RPT(RPT_ERR, "Too many short term RPS: %d.\n",
               sps->nb_st_rps);
		iRet = -1;
		goto exit;
    }
	printf("sps->nb_st_rps: %d\n",sps->nb_st_rps);
    for (i = 0; i < sps->nb_st_rps; i++) {
        if ((iRet = hevcDecodeShortTermRps(pvBuf, &sps->st_rps[i],
                                                 sps, 0)) < 0)
            goto exit;
    }

    sps->long_term_ref_pics_present_flag = getOneBit(pvBuf);
    if (sps->long_term_ref_pics_present_flag) {
        sps->num_long_term_ref_pics_sps = parseUe(pvBuf);
        if (sps->num_long_term_ref_pics_sps > HEVC_MAX_LONG_TERM_REF_PICS) {
            RPT(RPT_ERR, "Too many long term ref pics: %d.\n",
                   sps->num_long_term_ref_pics_sps);
			iRet = -1;
			goto exit;
        }
        for (i = 0; i < sps->num_long_term_ref_pics_sps; i++) {
            sps->lt_ref_pic_poc_lsb_sps[i]       = getBits(pvBuf, sps->log2_max_poc_lsb);
            sps->used_by_curr_pic_lt_sps_flag[i] = getOneBit(pvBuf);
        }
    }

    sps->sps_temporal_mvp_enabled_flag          = getOneBit(pvBuf);
    sps->sps_strong_intra_smoothing_enable_flag = getOneBit(pvBuf);
    sps->vui.sar = (T_AVRational){0, 1};
    vui_present = getOneBit(pvBuf);
    if (vui_present==1)
        decodeVui(pvBuf, sps);
	
#endif
#if 0

    if (getOneBit(pvBuf)) { // sps_extension_flag
        int sps_range_extension_flag = getOneBit(pvBuf);
        getBits(pvBuf, 7); //sps_extension_7bits = getBits(pvBuf, 7);
        if (sps_range_extension_flag) {
            int extended_precision_processing_flag;
            int cabac_bypass_alignment_enabled_flag;

            sps->transform_skip_rotation_enabled_flag = getOneBit(pvBuf);
            sps->transform_skip_context_enabled_flag  = getOneBit(pvBuf);
            sps->implicit_rdpcm_enabled_flag = getOneBit(pvBuf);

            sps->explicit_rdpcm_enabled_flag = getOneBit(pvBuf);

            extended_precision_processing_flag = getOneBit(pvBuf);
            if (extended_precision_processing_flag)
                RPT(RPT_WRN,
                   "extended_precision_processing_flag not yet implemented\n");

            sps->intra_smoothing_disabled_flag       = getOneBit(pvBuf);
            sps->high_precision_offsets_enabled_flag = getOneBit(pvBuf);
            if (sps->high_precision_offsets_enabled_flag)
                RPT(RPT_WRN,
                   "high_precision_offsets_enabled_flag not yet implemented\n");

            sps->persistent_rice_adaptation_enabled_flag = getOneBit(pvBuf);

            cabac_bypass_alignment_enabled_flag  = getOneBit(pvBuf);
            if (cabac_bypass_alignment_enabled_flag)
                RPT(RPT_WRN,
                   "cabac_bypass_alignment_enabled_flag not yet implemented\n");
        }
    }
    if (apply_defdispwin) {
        sps->output_window.left_offset   += sps->vui.def_disp_win.left_offset;
        sps->output_window.right_offset  += sps->vui.def_disp_win.right_offset;
        sps->output_window.top_offset    += sps->vui.def_disp_win.top_offset;
        sps->output_window.bottom_offset += sps->vui.def_disp_win.bottom_offset;
    }

    ow = &sps->output_window;
    if (ow->left_offset >= INT_MAX - ow->right_offset     ||
        ow->top_offset  >= INT_MAX - ow->bottom_offset    ||
        ow->left_offset + ow->right_offset  >= sps->width ||
        ow->top_offset  + ow->bottom_offset >= sps->height) {
        RPT(RPT_WRN, "Invalid cropping offsets: %u/%u/%u/%u\n",
               ow->left_offset, ow->right_offset, ow->top_offset, ow->bottom_offset);
        if (avctx->err_recognition & AV_EF_EXPLODE) {
            return AVERROR_INVALIDDATA;
        }
        RPT(RPT_WRN,
               "Displaying the whole video surface.\n");
        memset(ow, 0, sizeof(*ow));
        memset(&sps->pic_conf_win, 0, sizeof(sps->pic_conf_win));
    }

    // Inferred parameters
    sps->log2_ctb_size = sps->log2_min_cb_size +
                         sps->log2_diff_max_min_coding_block_size;
    sps->log2_min_pu_size = sps->log2_min_cb_size - 1;

    if (sps->log2_ctb_size > HEVC_MAX_LOG2_CTB_SIZE) {
        RPT(RPT_ERR, "CTB size out of range: 2^%d\n", sps->log2_ctb_size);
        iRet = -1;
        iRet = -1;
		goto exit;
    }
    if (sps->log2_ctb_size < 4) {
        RPT(RPT_ERR,
               "log2_ctb_size %d differs from the bounds of any known profile\n",
               sps->log2_ctb_size);
        avpriv_request_sample(avctx, "log2_ctb_size %d", sps->log2_ctb_size);
        iRet = -1;
        iRet = -1;
		goto exit;
    }

    sps->ctb_width  = (sps->width  + (1 << sps->log2_ctb_size) - 1) >> sps->log2_ctb_size;
    sps->ctb_height = (sps->height + (1 << sps->log2_ctb_size) - 1) >> sps->log2_ctb_size;
    sps->ctb_size   = sps->ctb_width * sps->ctb_height;

    sps->min_cb_width  = sps->width  >> sps->log2_min_cb_size;
    sps->min_cb_height = sps->height >> sps->log2_min_cb_size;
    sps->min_tb_width  = sps->width  >> sps->log2_min_tb_size;
    sps->min_tb_height = sps->height >> sps->log2_min_tb_size;
    sps->min_pu_width  = sps->width  >> sps->log2_min_pu_size;
    sps->min_pu_height = sps->height >> sps->log2_min_pu_size;
    sps->tb_mask       = (1 << (sps->log2_ctb_size - sps->log2_min_tb_size)) - 1;

    sps->qp_bd_offset = 6 * (sps->bit_depth - 8);

    if (av_mod_uintp2(sps->width, sps->log2_min_cb_size) ||
        av_mod_uintp2(sps->height, sps->log2_min_cb_size)) {
        RPT(RPT_ERR, "Invalid coded frame dimensions.\n");
        iRet = -1;
		goto exit;
    }

    if (sps->max_transform_hierarchy_depth_inter > sps->log2_ctb_size - sps->log2_min_tb_size) {
        RPT(RPT_ERR, "max_transform_hierarchy_depth_inter out of range: %d\n",
               sps->max_transform_hierarchy_depth_inter);
        iRet = -1;
		goto exit;
    }
    if (sps->max_transform_hierarchy_depth_intra > sps->log2_ctb_size - sps->log2_min_tb_size) {
        RPT(RPT_ERR, "max_transform_hierarchy_depth_intra out of range: %d\n",
               sps->max_transform_hierarchy_depth_intra);
        iRet = -1;
		goto exit;
    }
    if (sps->log2_max_trafo_size > FFMIN(sps->log2_ctb_size, 5)) {
        RPT(RPT_ERR,
               "max transform block size out of range: %d\n",
               sps->log2_max_trafo_size);
        iRet = -1;
		goto exit;
    }

    if (getBitsLeft(pvBuf) < 0) {
        RPT(RPT_ERR,
               "Overread SPS by %d bits\n", -getBitsLeft(pvBuf));
        iRet = -1;
		goto exit;
    }
#endif

	
exit:

	getBitContextFree(pvBuf);
	return iRet;

}

	
void h264GetWidthHeight(T_SPS *ptSps, int *piWidth, int *piHeight)
{
	// ¿í¸ß¼ÆËã¹«Ê½
	int iCodeWidth = 0;
	int iCodedHeight = 0;
	iCodeWidth	= 16 * ptSps->iMbWidth;
	iCodedHeight = 16 * ptSps->iMbHeight;
	*piWidth		 = iCodeWidth  - (ptSps->uiCropRight + ptSps->uiCropLeft);
	*piHeight		 = iCodedHeight - (ptSps->uiCropTop	+ ptSps->uiCropBottom);
	 if (*piWidth <= 0 || *piHeight <= 0) {
		 *piWidth  = iCodeWidth;
		 *piHeight = iCodedHeight;
	 }

	RPT(RPT_DBG, "iCodeWidth:%d, iCodedHeight:%d\n", iCodeWidth, iCodedHeight);

	RPT(RPT_DBG, "*piWidth:%d, *piHeight:%d\n", *piWidth, *piHeight);

	RPT(RPT_DBG, "ptSps->uiCropRight:%d, ptSps->uiCropLeft:%d\n", ptSps->uiCropRight, ptSps->uiCropLeft);

	RPT(RPT_DBG, "ptSps->uiCropTop:%d, ptSps->uiCropBottom:%d\n", ptSps->uiCropTop, ptSps->uiCropBottom);

}

int h264GetFormat(T_SPS *ptSps)
{
    return ptSps->iFrameMbsOnlyFlag;
}


void h264GeFramerate(T_SPS *ptSps, float *pfFramerate)
{
    int iFrInt = 0;
    if(ptSps->iTimingInfoPresentFlag)
    {
        if(!ptSps->iFixedFrameRateFlag)
        {
            *pfFramerate = (float)ptSps->u32TimeScale / (float)ptSps->u32NumUnitsInTick;
            //iFrInt = ptSps->vui_parameters.u32TimeScale / ptSps->vui_parameters.u32NumUnitsInTick;
        }else
        {
            *pfFramerate = (float)ptSps->u32TimeScale / (float)ptSps->u32NumUnitsInTick / 2.0;
            //iFrInt = ptSps->vui_parameters.u32TimeScale / ptSps->vui_parameters.u32NumUnitsInTick / 2;
        }
        iFrInt = ptSps->u32TimeScale / ptSps->u32NumUnitsInTick / 2;
    }
    switch(iFrInt)
    {
        case 23:// 23.98
			RPT(RPT_DBG, "frame rate:23.98");
            break;
        case 24:
			RPT(RPT_DBG, "frame rate:24");
            break;
        case 25:
			RPT(RPT_DBG, "frame rate:25");
            break;
        case 29://29.97
        	RPT(RPT_DBG, "frame rate:29.97");
            break;
        case 30:
			RPT(RPT_DBG, "frame rate:30");
            break;
        case 50:
			RPT(RPT_DBG, "frame rate:50");
            break;
        case 59://59.94
        	RPT(RPT_DBG, "frame rate:59.94");
            break;
        case 60:
			RPT(RPT_DBG, "frame rate:60");
            break;
        case 6:
			RPT(RPT_DBG, "frame rate:6");
            break;
        case 8:
			RPT(RPT_DBG, "frame rate:8");
            break;
        case 12:
			RPT(RPT_DBG, "frame rate:12");
            break;
        case 15:
			RPT(RPT_DBG, "frame rate:15");
            break;
        case 10:
			RPT(RPT_DBG, "frame rate:10");
            break;

        default:
            RPT(RPT_DBG, "frame rate:0");
            break;
    }

    return;
}




void h265GetWidthHeight(T_HEVCSPS *ptSps, int *piWidth, int *piHeight)
{
#if 1
	// ¿í¸ß¼ÆËã¹«Ê½
	int iCodeWidth = 0;
	int iCodedHeight = 0;
	iCodeWidth	= ptSps->width;
	iCodedHeight = ptSps->height;
	*piWidth		 = ptSps->width  - ptSps->pic_conf_win.left_offset - ptSps->pic_conf_win.right_offset;
	*piHeight		 = ptSps->height - ptSps->pic_conf_win.top_offset  - ptSps->pic_conf_win.bottom_offset;

	RPT(RPT_DBG, "iCodeWidth:%d, iCodedHeight:%d\n", iCodeWidth, iCodedHeight);

	RPT(RPT_DBG, "*piWidth:%d, *piHeight:%d\n", *piWidth, *piHeight);

//	RPT(RPT_DBG, "ptSps->uiCropRight:%d, ptSps->uiCropLeft:%d\n", ptSps->uiCropRight, ptSps->uiCropLeft);

//	RPT(RPT_DBG, "ptSps->uiCropTop:%d, ptSps->uiCropBottom:%d\n", ptSps->uiCropTop, ptSps->uiCropBottom);
#endif

}



void h265GeFramerate(T_HEVCSPS *ptSps, float *pfFramerate)
{
}

