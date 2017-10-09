/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef ENABLE_X264
#include "H264Encoder.h"

#include "Util/TimeTicker.h"
using namespace ZL::Util;

namespace ZL {
namespace Codec {

H264Encoder::H264Encoder() {

}

H264Encoder::~H264Encoder() {
	//* 清除图像区域
	if (m_pPicIn) {
		delete m_pPicIn;
		m_pPicIn = nullptr;
	}
	if (m_pPicOut) {
		delete m_pPicOut;
		m_pPicOut = nullptr;
	}

	//* 关闭编码器句柄
	if (m_pX264Handle) {
		x264_encoder_close(m_pX264Handle);
		m_pX264Handle = nullptr;
	}
}


/*typedef struct x264_param_t
{
   CPU 标志位
  unsigned int cpu;
  int i_threads;  并行编码多帧
  int b_deterministic; 是否允许非确定性时线程优化
  int i_sync_lookahead;  线程超前缓冲

   视频属性
  int i_width;  宽度
  int i_height;  高度
  int i_csp;  编码比特流的CSP,仅支持i420，色彩空间设置
  int i_level_idc;  level值的设置
  int i_frame_total;  编码帧的总数, 默认 0
Vui参数集视频可用性信息视频标准化选项
  struct
  {
   they will be reduced to be 0 < x <= 65535 and prime
  int i_sar_height;
  int i_sar_width;  设置长宽比

  int i_overscan;  0=undef, 1=no overscan, 2=overscan 过扫描线，默认"undef"(不设置)，可选项:show(观看)/crop(去除)

  见以下的值h264附件E
  Int i_vidformat; 视频格式，默认"undef"，component/pal/ntsc/secam/mac/undef
  int b_fullrange; Specify full range samples setting，默认"off"，可选项:off/on
  int i_colorprim; 原始色度格式，默认"undef"，可选项:undef/bt709/bt470m/bt470bg，smpte170m/smpte240m/film
  int i_transfer; 转换方式，默认"undef"，可选项:undef/bt709/bt470m/bt470bg/linear,log100/log316/smpte170m/smpte240m
  int i_colmatrix; 色度矩阵设置，默认"undef",undef/bt709/fcc/bt470bg,smpte170m/smpte240m/GBR/YCgCo
  int i_chroma_loc;  both top & bottom色度样本指定，范围0~5，默认0
  } vui;

  int i_fps_num;
  int i_fps_den;
这两个参数是由fps帧率确定的，赋值的过程见下:
{ float fps;
if( sscanf( value, "%d/%d", &p->i_fps_num, &p->i_fps_den ) == 2 )
  ;
  else if( sscanf( value, "%f", &fps ) )
  {
  p->i_fps_num = (int)(fps * 1000 + .5);
  p->i_fps_den = 1000;
  }
  else
  b_error = 1;
  }
Value的值就是fps。

  流参数
  int i_frame_reference;  参考帧最大数目
  int i_keyint_max;  在此间隔设置IDR关键帧
  int i_keyint_min;  场景切换少于次值编码位I, 而不是 IDR.
  int i_scenecut_threshold; 如何积极地插入额外的I帧
  int i_bframe; 两个相关图像间P帧的数目
  int i_bframe_adaptive; 自适应B帧判定
  int i_bframe_bias; 控制插入B帧判定，范围-100~+100，越高越容易插入B帧，默认0
  int b_bframe_pyramid; 允许部分B为参考帧
去块滤波器需要的参数
  int b_deblocking_filter;
  int i_deblocking_filter_alphac0;  [-6, 6] -6 light filter, 6 strong
  int i_deblocking_filter_beta;  [-6, 6] idem
  熵编码
  int b_cabac;
  int i_cabac_init_idc;

  int b_interlaced;  隔行扫描
  量化
  int i_cqm_preset; 自定义量化矩阵(CQM),初始化量化模式为flat
  char *psz_cqm_file;  JM format读取JM格式的外部量化矩阵文件，自动忽略其他—cqm 选项
  uint8_t cqm_4iy[16];  used only if i_cqm_preset == X264_CQM_CUSTOM
  uint8_t cqm_4ic[16];
  uint8_t cqm_4py[16];
  uint8_t cqm_4pc[16];
  uint8_t cqm_8iy[64];
  uint8_t cqm_8py[64];

   日志
  void (*pf_log)( void *, int i_level, const char *psz, va_list );
  void *p_log_private;
  int i_log_level;
  int b_visualize;
  char *psz_dump_yuv;  重建帧的名字

   编码分析参数
  struct
  {
  unsigned int intra;  帧间分区
  unsigned int inter;  帧内分区

  int b_transform_8x8;  帧间分区
  int b_weighted_bipred; 为b帧隐式加权
  int i_direct_mv_pred; 时间空间队运动预测
  int i_chroma_qp_offset; 色度量化步长偏移量

  int i_me_method;  运动估计算法 (X264_ME_*)
  int i_me_range;  整像素运动估计搜索范围 (from predicted mv)
  int i_mv_range;  运动矢量最大长度(in pixels). -1 = auto, based on level
  int i_mv_range_thread;  线程之间的最小空间. -1 = auto, based on number of threads.
  int i_subpel_refine;  亚像素运动估计质量
  int b_chroma_me;  亚像素色度运动估计和P帧的模式选择
  int b_mixed_references; 允许每个宏块的分区在P帧有它自己的参考号
  int i_trellis;  Trellis量化，对每个8x8的块寻找合适的量化值，需要CABAC，默认0 0:关闭1:只在最后编码时使用2:一直使用
  int b_fast_pskip; 快速P帧跳过检测
  int b_dct_decimate;  在P-frames转换参数域
  int i_noise_reduction; 自适应伪盲区
  float f_psy_rd;  Psy RD strength
  float f_psy_trellis;  Psy trellis strength
  int b_psy;  Toggle all psy optimizations

  ，亮度量化中使用的无效区大小
  int i_luma_deadzone[2];  {帧间, 帧内}

  int b_psnr;  计算和打印PSNR信息
  int b_ssim; 计算和打印SSIM信息
  } analyse;

   码率控制参数
  struct
  {
  int i_rc_method;  X264_RC_*

  int i_qp_constant;  0-51
  int i_qp_min; 允许的最小量化值
  int i_qp_max; 允许的最大量化值
  int i_qp_step; 帧间最大量化步长

  int i_bitrate; 设置平均码率
  float f_rf_constant;  1pass VBR, nominal QP
  float f_rate_tolerance;
  int i_vbv_max_bitrate; 平均码率模式下，最大瞬时码率，默认0(与-B设置相同)
  int i_vbv_buffer_size; 码率控制缓冲区的大小，单位kbit，默认0
  float f_vbv_buffer_init;  <=1: fraction of buffer_size. >1: kbit码率控制缓冲区数据保留的最大数据量与缓冲区大小之比，范围0~1.0，默认0.9
  float f_ip_factor;
  float f_pb_factor;

  int i_aq_mode;  psy adaptive QP. (X264_AQ_*)
  float f_aq_strength;
  int b_mb_tree;  Macroblock-tree ratecontrol.
  int i_lookahead;

   2pass 多次压缩码率控制
  int b_stat_write;  Enable stat writing in psz_stat_out
  char *psz_stat_out;
  int b_stat_read;  Read stat from psz_stat_in and use it
  char *psz_stat_in;

   2pass params (same as ffmpeg ones)
  float f_qcompress;  0.0 => cbr, 1.0 => constant qp
  float f_qblur; 时间上模糊量化
  float f_complexity_blur;  时间上模糊复杂性
  x264_zone_t *zones;  码率控制覆盖
  int i_zones;  number of zone_t's
  char *psz_zones; 指定区的另一种方法
  } rc;

   Muxing parameters
  int b_aud; 生成访问单元分隔符
  int b_repeat_headers;  在每个关键帧前放置SPS/PPS
  int i_sps_id;  SPS 和 PPS id 号

  切片（像条）参数
  int i_slice_max_size;  每片字节的最大数，包括预计的NAL开销.
  int i_slice_max_mbs;  每片宏块的最大数，重写 i_slice_count
  int i_slice_count;  每帧的像条数目: 设置矩形像条.

   Optional callback for freeing this x264_param_t when it is done being used.
  * Only used when the x264_param_t sits in memory for an indefinite period of time,
  * i.e. when an x264_param_t is passed to x264_t in an x264_picture_t or in zones.
  * Not used when x264_encoder_reconfig is called directly.
  void (*param_free)( void* );
} x264_param_t;*/

bool H264Encoder::init(int iWidth, int iHeight, int iFps) {
	if (m_pX264Handle) {
		return true;
	}
	x264_param_t X264Param, *pX264Param = &X264Param;
	//* 配置参数
	//* 使用默认参数
	x264_param_default_preset(pX264Param, "ultrafast", "zerolatency");

	//* cpuFlags
	pX264Param->i_threads = X264_SYNC_LOOKAHEAD_AUTO;		//* 取空缓冲区继续使用不死锁的保证.
	//* video Properties
	pX264Param->i_width = iWidth; //* 宽度.
	pX264Param->i_height = iHeight; //* 高度
	pX264Param->i_frame_total = 0; //* 编码总帧数.不知道用0.
	pX264Param->i_keyint_max = iFps * 3; //ffmpeg:gop_size 关键帧最大间隔
	pX264Param->i_keyint_min = iFps * 1; //ffmpeg:keyint_min 关键帧最小间隔
	//* Rate control Parameters
	pX264Param->rc.i_bitrate = 5000;		//* 码率(比特率,单位Kbps)
	pX264Param->rc.i_qp_step = 1;	//最大的在帧与帧之间进行切变的量化因子的变化量。ffmpeg:max_qdiff
	pX264Param->rc.i_qp_min = 10;	//ffmpeg:qmin;最小的量化因子。取值范围1-51。建议在10-30之间。
	pX264Param->rc.i_qp_max = 41;	//ffmpeg:qmax;最大的量化因子。取值范围1-51。建议在10-30之间。
	pX264Param->rc.f_qcompress = 0.6;//ffmpeg:qcompress 量化器压缩比率0-1.越小则比特率越区域固定，但是越高越使量化器参数越固定
	pX264Param->analyse.i_me_range = 16;		//ffmpeg:me_range 运动侦测的半径
	pX264Param->i_frame_reference = 3;		//ffmpeg:refsB和P帧向前预测参考的帧数。取值范围1-16。
											//该值不影响解码的速度，但是越大解码
											//所需的内存越大。这个值在一般情况下
											//越大效果越好，但是超过6以后效果就
											//不明显了。

	pX264Param->analyse.i_trellis = 1;							//ffmpeg:trellis
	//pX264Param->analyse.i_me_method=X264_ME_DIA;//ffmpeg:me_method ME_ZERO 运动侦测的方式
	pX264Param->rc.f_qblur = 0.5;		//ffmpeg:qblur

	//* bitstream parameters
	/*open-GOP
	 码流里面包含B帧的时候才会出现open-GOP。
	 一个GOP里面的某一帧在解码时要依赖于前一个GOP的某些帧，
	 这个GOP就称为open-GOP。
	 有些解码器不能完全支持open-GOP码流，
	 例如蓝光解码器，因此在x264里面open-GOP是默认关闭的。
	 对于解码端，接收到的码流如果如下:I0 B0 B1 P0 B2 B3...这就是一个open-GOP码流（I帧后面紧跟B帧）。
	 因此B0 B1的解码需要用到I0前面一个GOP的数据，B0 B1的dts是小于I0的。
	 如果码流如下: I0 P0 B0 B1 P1 B2 B3...这就是一个close-GOP码流，
	 I0后面所有帧的解码不依赖于I0前面的帧，I0后面所有帧的dts都比I0的大。
	 如果码流是IDR0 B0 B1 P0 B2 B3...那个这个GOP是close-GOP，B0,B1虽然dst比IDR0小，
	 但编解码端都刷新了参考缓冲，B0,B1参考不到前向GOP帧。
	 对于编码端，如果编码帧类型决定如下: ...P0 B1 B2 P3 B4 B5 I6这就会输出open-Gop码流 （P0 P3 B1 B2 I6 B4 B5...），
	 B4 B5的解码依赖P3。
	 如果编码帧类型决定如下...P0 B1 B2 P3 B4 P5 I6这样就不会输出open-GOP码流（P0 P3 B1 B2 P5 B4 I6...）。
	 两者区别在于I6前面的第5帧是设置为B帧还是P帧，
	 如果一个GOP的最后一帧（上例中是第5帧）设置为B帧，
	 这个码流就是open-GOP,设置为P帧就是close-GOP。
	 由于B帧压缩性能好于P帧，因此open-GOP在编码性能上稍微优于close-GOP，
	 但为了兼容性和少一些麻烦，还是把opne-GOP关闭的好。*/
	pX264Param->b_open_gop = 0;
	pX264Param->i_bframe = 0;		//最大B帧数.
	pX264Param->i_bframe_pyramid = 0;
	pX264Param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;
	//* Log
	pX264Param->i_log_level = X264_LOG_ERROR;

	//* muxing parameters
	pX264Param->i_fps_den = 1; //* 帧率分母
	pX264Param->i_fps_num = iFps; //* 帧率分子
	pX264Param->i_timebase_den = pX264Param->i_fps_num;
	pX264Param->i_timebase_num = pX264Param->i_fps_den;

	pX264Param->analyse.i_subpel_refine = 1; //这个参数控制在运动估算过程中质量和速度的权衡。Subq=5可以压缩>10%于subq=1。1-7
	pX264Param->analyse.b_fast_pskip = 1; //在P帧内执行早期快速跳跃探测。这个经常在没有任何损失的前提下提高了速度。

	pX264Param->b_annexb = 1; //1前面为0x00000001,0为nal长度
	pX264Param->b_repeat_headers = 1; //关键帧前面是否放sps跟pps帧，0 否 1，放

	//* 设置Profile.使用baseline
	x264_param_apply_profile(pX264Param, "high");

	//* 打开编码器句柄,通过x264_encoder_parameters得到设置给X264
	//* 的参数.通过x264_encoder_reconfig更新X264的参数
	m_pX264Handle = x264_encoder_open(pX264Param);
	if (!m_pX264Handle) {
		return false;
	}
	m_pPicIn = new x264_picture_t;
	m_pPicOut = new x264_picture_t;
	x264_picture_init(m_pPicIn);
	x264_picture_init(m_pPicOut);
	m_pPicIn->img.i_csp = X264_CSP_I420;
	m_pPicIn->img.i_plane = 3;
	return true;
}

int H264Encoder::inputData(char* apcYuv[3], int aiYuvLen[3], int64_t i64Pts, H264Frame** ppFrame) {
	//TimeTicker1(5);
	m_pPicIn->img.i_stride[0] = aiYuvLen[0];
	m_pPicIn->img.i_stride[1] = aiYuvLen[1];
	m_pPicIn->img.i_stride[2] = aiYuvLen[2];
	m_pPicIn->img.plane[0] = (uint8_t *) apcYuv[0];
	m_pPicIn->img.plane[1] = (uint8_t *) apcYuv[1];
	m_pPicIn->img.plane[2] = (uint8_t *) apcYuv[2];
	m_pPicIn->i_pts = i64Pts;
	int iNal;
	x264_nal_t* pNals;

	int iResult = x264_encoder_encode(m_pX264Handle, &pNals, &iNal, m_pPicIn,
			m_pPicOut);
	if (iResult <= 0) {
		return 0;
	}
	for (int i = 0; i < iNal; i++) {
		x264_nal_t pNal = pNals[i];
		m_aFrames[i].iType = pNal.i_type;
		m_aFrames[i].iLength = pNal.i_payload;
		m_aFrames[i].pucData = pNal.p_payload;
	}
	*ppFrame = m_aFrames;
	return iNal;
}

} /* namespace Codec */
} /* namespace ZL */

#endif //ENABLE_X264
















