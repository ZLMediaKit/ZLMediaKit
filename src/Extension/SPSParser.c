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



