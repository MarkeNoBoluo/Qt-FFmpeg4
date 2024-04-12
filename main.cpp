#include <QCoreApplication>
#include <QDebug>
#include <QtCore>
#include <iostream>
#include <WinSock2.h>
#include <windows.h>
#include <stdio.h>
#include<conio.h>
#include <qdatetime.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include "libavutil/avutil.h"
#include <libavutil/rational.h>
}
qreal rationalToDouble(AVRational* rational)
{
    qreal frameRate = (rational->den == 0) ? 0 : (qreal(rational->num) / rational->den);
    return frameRate;
}


int main()
{
    const char * localFile = "D:/EXE/demo.flv";
    const char * url = "rtsp://127.0.0.1:554/test";

    //推流部分
    int m_videoIndex = -1;
    int m_audioIndex = -1;

    int width = 0;
    int height = 0;
    int frameRate = 0;
    int totalFrames = 0;

    AVFormatContext* pInputFormatContext = NULL;// 解封装上下文
    AVCodecContext* pInputCodecContex = NULL;   // 解码器上下文
    const AVCodec* pInputCodec = NULL;                // 解码器
    AVStream* videoStream = NULL;
    AVFormatContext* pOutputFormatContext = NULL;
    AVCodecContext* pOutCodecContext = NULL;
    const AVCodec* pOutCodec = NULL;
    AVStream* pOutStream = NULL;

    //初始化
    avformat_network_init();
    pInputFormatContext = avformat_alloc_context();
    pOutputFormatContext = avformat_alloc_context();

    //参数配置
    AVDictionary* dict = NULL;
    av_dict_set(&dict, "rtsp_transport", "tcp", 0);      // 设置rtsp流使用tcp打开，如果打开失败错误信息为【Error number -135 occurred】可以切换（UDP、tcp、udp_multicast、http），比如vlc推流就需要使用udp打开
    av_dict_set(&dict, "max_delay", "3", 0);             // 设置最大复用或解复用延迟（以微秒为单位）。当通过【UDP】 接收数据时，解复用器尝试重新排序接收到的数据包（因为它们可能无序到达，或者数据包可能完全丢失）。这可以通过将最大解复用延迟设置为零（通过max_delayAVFormatContext 字段）来禁用。
    av_dict_set(&dict, "timeout", "1000000", 0);         // 以微秒为单位设置套接字 TCP I/O 超时，如果等待时间过短，也可能会还没连接就返回了。

    // 打开输入流并返回解封装上下文
    int ret = avformat_open_input(&pInputFormatContext,          // 返回解封装上下文
                                  localFile,  // 打开视频地址
                                  nullptr,                   // 如果非null，此参数强制使用特定的输入格式。自动选择解封装器（文件格式）
                                  &dict);                    // 参数设置
    // 释放参数字典
    if(dict)
    {
        av_dict_free(&dict);
    }
    // 打开视频失败
    if(ret < 0)
    {
        printf("Couldn't open input stream.\n");
        getchar();
        return -1;
    }

    // 读取媒体文件的数据包以获取流信息。
    ret = avformat_find_stream_info(pInputFormatContext, nullptr);
    if(ret < 0)
    {
        printf("Failed to retrieve input stream information");
        getchar();
        return -1;
    }

    int totalTime = pInputFormatContext->duration / (AV_TIME_BASE / 1000); // 计算视频总时长（毫秒）
    qDebug() << QString("视频总时长：%1 ms，[%2]").arg(totalTime).arg(QTime::fromMSecsSinceStartOfDay(int(totalTime)).toString("HH:mm:ss zzz"));

    // 通过AVMediaType枚举查询视频流ID（也可以通过遍历查找），最后一个参数无用
    m_videoIndex = av_find_best_stream(pInputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(m_videoIndex < 0)
    {
        printf("Failed to find index");
        getchar();
        return -1;
    }

    videoStream = pInputFormatContext->streams[m_videoIndex];  // 通过查询到的索引获取视频流

    // 获取视频图像分辨率（AVStream中的AVCodecContext在新版本中弃用，改为使用AVCodecParameters）
    width = videoStream->codecpar->width;
    height = videoStream->codecpar->height;
    frameRate = rationalToDouble(&videoStream->avg_frame_rate);  // 视频帧率


    // 通过解码器ID获取视频解码器（新版本返回值必须使用const）
    pInputCodec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    totalFrames = videoStream->nb_frames;

    qDebug() << QString("分辨率：[w:%1,h:%2] 帧率：%3  总帧数：%4  解码器：%5")
                    .arg(width).arg(height).arg(frameRate).arg(totalFrames).arg(codec->name);

    //输入流解码器初始化
    // 分配AVCodecContext并将其字段设置为默认值。
    pInputCodecContex = avcodec_alloc_context3(codec);
    pInputCodecContex->flags |= 0x00080000;
    if(!pInputCodecContex){
        qDebug() << "create AVCodecContext error";
        return -1;
    }

    // 使用视频流的codecpar为解码器上下文赋值
    ret = avcodec_parameters_to_context(pInputCodecContex, videoStream->codecpar);
    if(ret < 0)
    {
        printf("Codec not found.\n");
        getchar();
        return -1;
    }
    pInputCodecContex->flags2 |= AV_CODEC_FLAG2_FAST;    // 允许不符合规范的加速技巧。
    pInputCodecContex->thread_count = 8;                 // 使用8线程解码

    // 初始化解码器上下文，如果之前avcodec_alloc_context3传入了解码器，这里设置NULL就可以
    ret = avcodec_open2(pInputCodecContex, nullptr, nullptr);
    if(ret < 0)
    {
        printf("avcodec_open2 error.\n");
        getchar();
        return -1;
    }

    //为一帧图像分配内存
    AVFrame* pFrame;
    AVFrame* pFrameYUV;
    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();//为转换来申请一帧的内存(把原始帧->YUV)
    pFrameYUV->format = AV_PIX_FMT_YUV420P;
    pFrameYUV->width = pInputCodecContex->width;
    pFrameYUV->height = pInputCodecContex->height;
    unsigned char* out_buffer = (unsigned char*)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pInputCodecContex->width, pInputCodecContex->height));
    //现在我们使用avpicture_fill来把帧和我们新申请的内存来结合
    av_image_fill_arrays(pFrameYUV->data,pFrameYUV->linesize ,out_buffer, AV_PIX_FMT_YUV420P, pInputCodecContex->width, pInputCodecContex->height,1);
    struct SwsContext* img_convert_ctx;
    img_convert_ctx = sws_getContext(pInputCodecContex->width, pInputCodecContex->height, pInputCodecContex->pix_fmt, pInputCodecContex->width, pInputCodecContex->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    //=============================================================================================
    //输出流配置
    ret = avformat_alloc_output_context2(&pOutputFormatContext, NULL, "rtsp", url); //RTMP  定义一个输出流信息的结构体
    if(ret < 0)
    {
        printf("avformat_alloc_output_context2 error.\n");
        getchar();
        return -1;
    }

    av_opt_set(pOutputFormatContext->priv_data, "rtsp_transport", "tcp", 0);
    //输出流编码器配置
    pOutCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!pOutCodec) {
        printf("Can not find encoder! \n");
        getchar();
        return -1;
    }
    pOutCodecContext = avcodec_alloc_context3(pOutCodec);
    //像素格式,
    pOutCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    //size
    pOutCodecContext->width = pInputCodecContex->width;
    pOutCodecContext->height = pInputCodecContex->height;
    //目标码率
    pOutCodecContext->bit_rate = 4000000;
    //每250帧插入一个I帧,I帧越小视频越小
    pOutCodecContext->gop_size = 10;
    //Optional Param B帧
    pOutCodecContext->max_b_frames = 0;  //设置B帧为0，则DTS与PTS一致

    pOutCodecContext->time_base.num = 1;
    pOutCodecContext->time_base.den = 25;

    //最大和最小量化系数
    pOutCodecContext->qmin = 10;
    pOutCodecContext->qmax = 51;

    return 0;
}
