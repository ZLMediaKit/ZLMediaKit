#ifndef _SPSPARSER_H_
#define _SPSPARSER_H_

#if defined (__cplusplus)
    extern "C" {
#endif

#define QP_MAX_NUM (51 + 6*6)           // The maximum supported qp

#define HEVC_MAX_SHORT_TERM_RPS_COUNT 64

#define T_PROFILE_HEVC_MAIN                        1
#define T_PROFILE_HEVC_MAIN_10                     2
#define T_PROFILE_HEVC_MAIN_STILL_PICTURE          3
#define T_PROFILE_HEVC_REXT                        4



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


enum {
    // 7.4.3.1: vps_max_layers_minus1 is in [0, 62].
    HEVC_MAX_LAYERS     = 63,
    // 7.4.3.1: vps_max_sub_layers_minus1 is in [0, 6].
    HEVC_MAX_SUB_LAYERS = 7,
    // 7.4.3.1: vps_num_layer_sets_minus1 is in [0, 1023].
    HEVC_MAX_LAYER_SETS = 1024,

    // 7.4.2.1: vps_video_parameter_set_id is u(4).
    HEVC_MAX_VPS_COUNT = 16,
    // 7.4.3.2.1: sps_seq_parameter_set_id is in [0, 15].
    HEVC_MAX_SPS_COUNT = 16,
    // 7.4.3.3.1: pps_pic_parameter_set_id is in [0, 63].
    HEVC_MAX_PPS_COUNT = 64,

    // A.4.2: MaxDpbSize is bounded above by 16.
    HEVC_MAX_DPB_SIZE = 16,
    // 7.4.3.1: vps_max_dec_pic_buffering_minus1[i] is in [0, MaxDpbSize - 1].
    HEVC_MAX_REFS     = HEVC_MAX_DPB_SIZE,

    // 7.4.3.2.1: num_short_term_ref_pic_sets is in [0, 64].
    HEVC_MAX_SHORT_TERM_REF_PIC_SETS = 64,
    // 7.4.3.2.1: num_long_term_ref_pics_sps is in [0, 32].
    HEVC_MAX_LONG_TERM_REF_PICS      = 32,

    // A.3: all profiles require that CtbLog2SizeY is in [4, 6].
    HEVC_MIN_LOG2_CTB_SIZE = 4,
    HEVC_MAX_LOG2_CTB_SIZE = 6,

    // E.3.2: cpb_cnt_minus1[i] is in [0, 31].
    HEVC_MAX_CPB_CNT = 32,

    // A.4.1: in table A.6 the highest level allows a MaxLumaPs of 35 651 584.
    HEVC_MAX_LUMA_PS = 35651584,
    // A.4.1: pic_width_in_luma_samples and pic_height_in_luma_samples are
    // constrained to be not greater than sqrt(MaxLumaPs * 8).  Hence height/
    // width are bounded above by sqrt(8 * 35651584) = 16888.2 samples.
    HEVC_MAX_WIDTH  = 16888,
    HEVC_MAX_HEIGHT = 16888,

    // A.4.1: table A.6 allows at most 22 tile rows for any level.
    HEVC_MAX_TILE_ROWS    = 22,
    // A.4.1: table A.6 allows at most 20 tile columns for any level.
    HEVC_MAX_TILE_COLUMNS = 20,

    // 7.4.7.1: in the worst case (tiles_enabled_flag and
    // entropy_coding_sync_enabled_flag are both set), entry points can be
    // placed at the beginning of every Ctb row in every tile, giving an
    // upper bound of (num_tile_columns_minus1 + 1) * PicHeightInCtbsY - 1.
    // Only a stream with very high resolution and perverse parameters could
    // get near that, though, so set a lower limit here with the maximum
    // possible value for 4K video (at most 135 16x16 Ctb rows).
    HEVC_MAX_ENTRY_POINT_OFFSETS = HEVC_MAX_TILE_COLUMNS * 135,
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


typedef struct T_HEVCWindow {
    unsigned int uiLeftOffset;
    unsigned int uiRightOffset;
    unsigned int uiTopOffset;
    unsigned int uiBottomOffset;
} T_HEVCWindow;


typedef struct T_VUI {
    T_AVRational tSar;

    int iOverscanInfoPresentFlag;
    int iOverscanAppropriateFlag;

    int iVideoSignalTypePresentFlag;
    int iVideoFormat;
    int iVideoFullRangeFlag;
    int iColourDescriptionPresentFlag;
    uint8_t u8ColourPrimaries;
    uint8_t u8TransferCharacteristic;
    uint8_t u8MatrixCoeffs;

    int iChromaLocInfoPresentFlag;
    int iChromaSampleLocTypeTopField;
    int iChromaSampleLocTypeBottomField;
    int iNeutraChromaIndicationFlag;

    int iFieldSeqFlag;
    int iFrameFieldInfoPresentFlag;

    int iDefaultDisplayWindowFlag;
    T_HEVCWindow tDefDispWin;

    int iVuiTimingInfoPresentFlag;
    uint32_t u32VuiNumUnitsInTick;
    uint32_t u32VuiTimeScale;
    int iVuiPocProportionalToTimingFlag;
    int iVuiNumTicksPocDiffOneMinus1;
    int iVuiHrdParametersPresentFlag;

    int iBitstreamRestrictionFlag;
    int iTilesFixedStructureFlag;
    int iMotionVectorsOverPicBoundariesFlag;
    int iRestrictedRefPicListsFlag;
    int iMinSpatialSegmentationIdc;
    int iMaxBytesPerPicDenom;
    int iMaxBitsPerMinCuDenom;
    int iLog2MaxMvLengthHorizontal;
    int iLog2MaxMvLengthVertical;
} T_VUI;

typedef struct T_PTLCommon {
    uint8_t u8ProfileSpace;
    uint8_t u8TierFlag;
    uint8_t u8ProfileIdc;
    uint8_t au8ProfileCompatibilityFlag[32];
    uint8_t u8LevelIdc;
    uint8_t u8ProgressiveSourceFlag;
    uint8_t u8InterlacedSourceFlag;
    uint8_t u8NonPackedConstraintFlag;
    uint8_t u8FrameOnlyConstraintFlag;
} T_PTLCommon;

typedef struct T_PTL {
    T_PTLCommon tGeneralPtl;
    T_PTLCommon atSubLayerPtl[HEVC_MAX_SUB_LAYERS];

    uint8_t au8SubLayerProfilePresentFlag[HEVC_MAX_SUB_LAYERS];
    uint8_t au8SubLayerLevelPresentFlag[HEVC_MAX_SUB_LAYERS];
} T_PTL;

typedef struct T_ScalingList {
    /* This is a little wasteful, since sizeID 0 only needs 8 coeffs,
     * and size ID 3 only has 2 arrays, not 6. */
    uint8_t aaau8Sl[4][6][64];
    uint8_t aau8SlDc[2][6];
} T_ScalingList;

typedef struct T_ShortTermRPS {
    unsigned int uiNumNegativePics;
    int iNumDeltaPocs;
    int iRpsIdxNumDeltaPocs;
    int32_t au32DeltaPoc[32];
    uint8_t au8Used[32];
} T_ShortTermRPS;


typedef struct T_HEVCVPS {
    uint8_t u8VpsTemporalIdNestingFlag;
    int iVpsMaxLayers;
    int iVpsMaxSubLayers; ///< vps_max_temporal_layers_minus1 + 1

    T_PTL tPtl;
    int iVpsSubLayerOrderingInfoPresentFlag;
    unsigned int uiVpsMaxDecPicBuffering[HEVC_MAX_SUB_LAYERS];
    unsigned int auiVpsNumReorderPics[HEVC_MAX_SUB_LAYERS];
    unsigned int auiVpsMaxLatencyIncrease[HEVC_MAX_SUB_LAYERS];
    int iVpsMaxLayerId;
    int iVpsNumLayerSets; ///< vps_num_layer_sets_minus1 + 1
    uint8_t u8VpsTimingInfoPresentFlag;
    uint32_t u32VpsNumUnitsInTick;
    uint32_t u32VpsTimeScale;
    uint8_t u8VpsPocProportionalToTimingFlag;
    int iVpsNumTicksPocDiffOne; ///< vps_num_ticks_poc_diff_one_minus1 + 1
    int iVpsNumHrdParameters;

} T_HEVCVPS;

typedef struct T_HEVCSPS {
    unsigned int  uiVpsId;
    int iChromaFormatIdc;
    uint8_t u8SeparateColourPlaneFlag;

    ///< output (i.e. cropped) values
    int iIutputWidth, iOutputHeight;
    T_HEVCWindow tOutputWindow;

    T_HEVCWindow tPicConfWin;

    int iBitDepth;	
    int iBitDepthChroma;
    int iPixelShift;

    unsigned int uiLog2MaxPocLsb;
    int iPcmEnabledFlag;

    int iMaxSubLayers;
    struct {
        int iMaxDecPicBuffering;
        int iNumReorderPics;
        int iMaxLatencyIncrease;
    } stTemporalLayer[HEVC_MAX_SUB_LAYERS];
    uint8_t u8temporalIdNestingFlag;

    T_VUI tVui;
    T_PTL tPtl;

    uint8_t u8ScalingListEnableFlag;
    T_ScalingList tScalingList;

    unsigned int uiNbStRps;
    T_ShortTermRPS atStRps[HEVC_MAX_SHORT_TERM_RPS_COUNT];

    uint8_t u8AmpEnabledFlag;
    uint8_t u8SaoEnabled;

    uint8_t u8LongTermRefPicsPresentFlag;
    uint16_t au16LtRefPicPocLsbSps[32];
    uint8_t au8UsedByCurrPicLtSpsFlag[32];
    uint8_t u8NumLongTermRefPicsSps;

    struct {
        uint8_t u8BitDepth;
        uint8_t u8BitDepthChroma;
        unsigned int uiLog2MinPcmCbSize;
        unsigned int uiLog2MaxPcmCbSize;
        uint8_t u8LoopFilterDisableFlag;
    } pcm;
    uint8_t u8SpsTemporalMvpEnabledFlag;
    uint8_t u8SpsStrongIntraMmoothingEnableFlag;

    unsigned int uiLog2MinCbSize;
    unsigned int uiLog2DiffMaxMinCodingBlockSize;
    unsigned int uiLog2MinTbSize;
    unsigned int uiLog2MaxTrafoSize;
    unsigned int uiLog2CtbSize;
    unsigned int uiLog2MinPuSize;

    int iMaxTransformHierarchyDepthInter;
    int iMaxTransformHierarchyDepthIntra;

    int iTransformSkipRotationEnabledFlag;
    int iTransformSkipContextEnabledFlag;
    int iImplicitRdpcmEnabledFlag;
    int iExplicitRdpcmEnabledFlag;
    int iIntraSmoothingDisabledFlag;
    int iHighPrecisionOffsetsEnabledFlag;
    int iPersistentRiceAdaptationEnabledFlag;

    ///< coded frame dimension in various units
    int iWidth;
    int iHeight;
    int iCtbWidth;
    int iCtbHeight;
    int iCtbSize;
    int iMinCbWidth;
    int iMinCbHeight;
    int iMinTbWidth;
    int iMinTbHeight;
    int iMinPuWidth;
    int iMinPuHeight;
    int iTbMask;

    int aiHshift[3];
    int aiVshift[3];

    int iQpBdOffset;

    int iVuiPresent;
}T_HEVCSPS;


typedef struct T_GetBitContext{
    uint8_t *pu8Buf;         /*Ö¸ÏòSPS start*/
    int     iBufSize;     /*SPS ³¤¶È*/
    int     iBitPos;      /*bitÒÑ¶ÁÈ¡Î»ÖÃ*/
    int     iTotalBit;    /*bit×Ü³¤¶È*/
    int     iCurBitPos;  /*µ±Ç°¶ÁÈ¡Î»ÖÃ*/
}T_GetBitContext;


int h264DecSeqParameterSet(void *pvBuf, T_SPS *ptSps);
int h265DecSeqParameterSet( void *pvBufSrc, T_HEVCSPS *ptSps );
int h265DecVideoParameterSet( void *pvBufSrc, T_HEVCVPS *ptVps );


void h264GetWidthHeight(T_SPS *ptSps, int *piWidth, int *piHeight);
void h265GetWidthHeight(T_HEVCSPS *ptSps, int *piWidth, int *piHeight);

void h264GeFramerate(T_SPS *ptSps, float *pfFramerate);
void h265GeFramerate(T_HEVCVPS *ptVps, T_HEVCSPS *ptSps,float *pfFramerate);

#if defined (__cplusplus)
}
#endif

#endif //_SPS_PPS_H_
