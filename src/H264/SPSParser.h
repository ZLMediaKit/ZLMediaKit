#ifndef _SPSPARSER_H_
#define _SPSPARSER_H_

#if defined (__cplusplus)
    extern "C" {
#endif

#define QP_MAX_NUM (51 + 6*6)           // The maximum supported qp

/**
  * Chromaticity coordinates of the source primaries.
  */
enum T_AVColorPrimaries {
    AVCOL_PRI_RESERVED0   = 0,
    AVCOL_PRI_BT709       = 1, ///< also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B
    AVCOL_PRI_UNSPECIFIED = 2,
    AVCOL_PRI_RESERVED    = 3,
    AVCOL_PRI_BT470M      = 4, ///< also FCC Title 47 Code of Federal Regulations 73.682 (a)(20)

    AVCOL_PRI_BT470BG     = 5, ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM
    AVCOL_PRI_SMPTE170M   = 6, ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
    AVCOL_PRI_SMPTE240M   = 7, ///< functionally identical to above
    AVCOL_PRI_FILM        = 8, ///< colour filters using Illuminant C
    AVCOL_PRI_BT2020      = 9, ///< ITU-R BT2020
    AVCOL_PRI_NB,              ///< Not part of ABI
};

/**
 * Color Transfer Characteristic.
 */
enum T_AVColorTransferCharacteristic {
    AVCOL_TRC_RESERVED0    = 0,
    AVCOL_TRC_BT709        = 1,  ///< also ITU-R BT1361
    AVCOL_TRC_UNSPECIFIED  = 2,
    AVCOL_TRC_RESERVED     = 3,
    AVCOL_TRC_GAMMA22      = 4,  ///< also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM
    AVCOL_TRC_GAMMA28      = 5,  ///< also ITU-R BT470BG
    AVCOL_TRC_SMPTE170M    = 6,  ///< also ITU-R BT601-6 525 or 625 / ITU-R BT1358 525 or 625 / ITU-R BT1700 NTSC
    AVCOL_TRC_SMPTE240M    = 7,
    AVCOL_TRC_LINEAR       = 8,  ///< "Linear transfer characteristics"
    AVCOL_TRC_LOG          = 9,  ///< "Logarithmic transfer characteristic (100:1 range)"
    AVCOL_TRC_LOG_SQRT     = 10, ///< "Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range)"
    AVCOL_TRC_IEC61966_2_4 = 11, ///< IEC 61966-2-4
    AVCOL_TRC_BT1361_ECG   = 12, ///< ITU-R BT1361 Extended Colour Gamut
    AVCOL_TRC_IEC61966_2_1 = 13, ///< IEC 61966-2-1 (sRGB or sYCC)
    AVCOL_TRC_BT2020_10    = 14, ///< ITU-R BT2020 for 10 bit system
    AVCOL_TRC_BT2020_12    = 15, ///< ITU-R BT2020 for 12 bit system
    AVCOL_TRC_NB,                ///< Not part of ABI
};

/**
 * YUV tColorspace type.
 */
enum T_AVColorSpace {
    AVCOL_SPC_RGB         = 0,  ///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
    AVCOL_SPC_BT709       = 1,  ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
    AVCOL_SPC_UNSPECIFIED = 2,
    AVCOL_SPC_RESERVED    = 3,
    AVCOL_SPC_FCC         = 4,  ///< FCC Title 47 Code of Federal Regulations 73.682 (a)(20)
    AVCOL_SPC_BT470BG     = 5,  ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
    AVCOL_SPC_SMPTE170M   = 6,  ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC / functionally identical to above
    AVCOL_SPC_SMPTE240M   = 7,
    AVCOL_SPC_YCOCG       = 8,  ///< Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16
    AVCOL_SPC_BT2020_NCL  = 9,  ///< ITU-R BT2020 non-constant luminance system
    AVCOL_SPC_BT2020_CL   = 10, ///< ITU-R BT2020 constant luminance system
    AVCOL_SPC_NB,               ///< Not part of ABI
};


/**
 * rational number numerator/denominator
 */
typedef struct T_AVRational{
    int num; ///< numerator
    int den; ///< denominator
} T_AVRational;


/***
 * Sequence parameter set
 * ¿É²Î¿¼H264±ê×¼µÚ7½ÚºÍ¸½Â¼D E
 */
#define Extended_SAR 255

/**
 * Sequence parameter set
 */
typedef struct T_SPS {
    unsigned int uiSpsId;
    int iProfileIdc;
    int iLevelIdc;
    int iChromaFormatIdc;
    int iTransformBypass;              ///< qpprime_y_zero_transform_bypass_flag
    int iLog2MaxFrameNum;            ///< log2_max_frame_num_minus4 + 4
    int iPocType;                      ///< pic_order_cnt_type
    int iLog2MaxPocLsb;              ///< log2_max_pic_order_cnt_lsb_minus4
    int iDeltaPicOrderAlwaysZeroFlag;
    int iOffsetForNonRefPic;
    int iOffsetForTopToBottomField;
    int iPocCycleLength;              ///< num_ref_frames_in_pic_order_cnt_cycle
    int iRefFrameCount;               ///< num_ref_frames
    int iGapsInFrameNumAllowedFlag;
    int iMbWidth;                      ///< pic_width_in_mbs_minus1 + 1
    int iMbHeight;                     ///< pic_height_in_map_units_minus1 + 1
    int iFrameMbsOnlyFlag;
    int iMbAff;                        ///< mb_adaptive_frame_field_flag
    int iDirect8x8InferenceFlag;
    int iCrop;                          ///< frame_cropping_flag

    /* those 4 are already in luma samples */
    unsigned int uiCropLeft;            ///< frame_cropping_rect_left_offset
    unsigned int uiCropRight;           ///< frame_cropping_rect_right_offset
    unsigned int uiCropTop;             ///< frame_cropping_rect_top_offset
    unsigned int uiCropBottom;          ///< frame_cropping_rect_bottom_offset
    int iVuiParametersPresentFlag;
    T_AVRational tSar;
    int iVideoSignalTypePresentFlag;
    int iFullRange;
    int iColourDescriptionPresentFlag;
    enum T_AVColorPrimaries tColorPrimaries;
    enum T_AVColorTransferCharacteristic tColorTrc;
    enum T_AVColorSpace tColorspace;
    int iTimingInfoPresentFlag;
    uint32_t u32NumUnitsInTick;
    uint32_t u32TimeScale;
    int iFixedFrameRateFlag;
    short asOffsetForRefFrame[256]; // FIXME dyn aloc?
    int iBitstreamRestrictionFlag;
    int iNumReorderFrames;
    int iScalingMatrixPresent;
    uint8_t aau8ScalingMatrix4[6][16];
    uint8_t aau8ScalingMatrix8[6][64];
    int iNalHrdParametersPresentFlag;
    int iVclHrdParametersPresentFlag;
    int iPicStructPresentFlag;
    int iTimeOffsetLength;
    int iCpbCnt;                          ///< See H.264 E.1.2
    int iInitialCpbRemovalDelayLength; ///< initial_cpb_removal_delay_length_minus1 + 1
    int iCpbRemovalDelayLength;         ///< cpb_removal_delay_length_minus1 + 1
    int iDpbOutputDelayLength;          ///< dpb_output_delay_length_minus1 + 1
    int iBitDepthLuma;                   ///< bit_depth_luma_minus8 + 8
    int iBitDepthChroma;                 ///< bit_depth_chroma_minus8 + 8
    int iResidualColorTransformFlag;    ///< residual_colour_transform_flag
    int iConstraintSetFlags;             ///< constraint_set[0-3]_flag
    int iNew;                              ///< flag to keep track if the decoder context needs re-init due to changed SPS
} T_SPS;

/**
 * Picture parameter set
 */
typedef struct T_PPS {
    unsigned int uiSpsId;
    int iCabac;                  ///< entropy_coding_mode_flag
    int iPicOrderPresent;      ///< pic_order_present_flag
    int iSliceGroupCount;      ///< num_slice_groups_minus1 + 1
    int iMbSliceGroupMapType;
    unsigned int auiRefCount[2];  ///< num_ref_idx_l0/1_active_minus1 + 1
    int iWeightedPred;          ///< weighted_pred_flag
    int iWeightedBipredIdc;
    int iInitQp;                ///< pic_init_qp_minus26 + 26
    int iInitQs;                ///< pic_init_qs_minus26 + 26
    int aiChromaQpIndexOffset[2];
    int iDeblockingFilterParametersPresent; ///< deblocking_filter_parameters_present_flag
    int iConstrainedIntraPred;     ///< constrained_intra_pred_flag
    int iRedundantPicCntPresent;  ///< redundant_pic_cnt_present_flag
    int iTransform8x8Mode;         ///< transform_8x8_mode_flag
    uint8_t aau8ScalingMatrix4[6][16];
    uint8_t aau8ScalingMatrix8[6][64];
    uint8_t u8ChromaQpTable[2][QP_MAX_NUM+1];  ///< pre-scaled (with aiChromaQpIndexOffset) version of qp_table
    int iChromaQpDiff;
} T_PPS;

typedef struct T_GetBitContext{
    uint8_t *pu8Buf;         /*Ö¸ÏòSPS start*/
    int     iBufSize;     /*SPS ³¤¶È*/
    int     iBitPos;      /*bitÒÑ¶ÁÈ¡Î»ÖÃ*/
    int     iTotalBit;    /*bit×Ü³¤¶È*/
    int     iCurBitPos;  /*µ±Ç°¶ÁÈ¡Î»ÖÃ*/
}T_GetBitContext;


int h264DecSeqParameterSet(void *pvBuf, T_SPS *ptSps);
void h264GetWidthHeight(T_SPS *ptSps, int *piWidth, int *piHeight);
void h264GeFramerate(T_SPS *ptSps, float *pfFramerate);

#if defined (__cplusplus)
}
#endif

#endif //_SPS_PPS_H_
