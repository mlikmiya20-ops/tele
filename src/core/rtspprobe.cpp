/**
 * @file rtspprobe.cpp
 * @brief Implementation of RTSPProbe class
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 */

#include "rtspprobe.h"
#include "networkscanner.h"
#include "errorlogger.h"
#include <QDebug>
#include <QThread>
#include <QFuture>
#include <QtConcurrent>

namespace telecloud {

// ============================================================================
// RTSPProbe Implementation
// ============================================================================

RTSPProbe::RTSPProbe(QObject* parent)
    : QObject(parent)
{
    // Initialize FFmpeg network
    avformat_network_init();
    
    // Set default options for RTSP
    av_dict_set(&m_options, "rtsp_transport", "tcp", 0);
    av_dict_set(&m_options, "stimeout", QString::number(m_timeoutMs * 1000).toUtf8(), 0);
    av_dict_set(&m_options, "max_delay", "500000", 0);
}

RTSPProbe::~RTSPProbe() {
    if (m_options) {
        av_dict_free(&m_options);
    }
}

RTSPProbeResult RTSPProbe::probe(const QString& url) {
    RTSPProbeResult result;
    result.rtspUrl = url;
    
    AVFormatContext* formatCtx = nullptr;
    
    // Open the stream
    if (!openStream(url, &formatCtx)) {
        result.success = false;
        return result;
    }
    
    // Extract stream information
    result.streamInfo = extractStreamInfo(formatCtx);
    result.success = result.streamInfo.isValid;
    
    // Clean up
    avformat_close_input(&formatCtx);
    
    return result;
}

void RTSPProbe::probeAsync(const QString& url) {
    QFuture<RTSPProbeResult> future = QtConcurrent::run([this, url]() {
        return probe(url);
    });
    
    QFutureWatcher<RTSPProbeResult>* watcher = new QFutureWatcher<RTSPProbeResult>(this);
    connect(watcher, &QFutureWatcher<RTSPProbeResult>::finished, this, [this, watcher, url]() {
        RTSPProbeResult result = watcher->result();
        emit probeCompleted(url, result);
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

QMap<int, RTSPProbeResult> RTSPProbe::probeChannels(
    const QString& ipAddress,
    int port,
    const QString& username,
    const QString& password,
    DVRBrand brand,
    int maxChannels)
{
    QMap<int, RTSPProbeResult> results;
    int totalChannels = qMin(maxChannels, 64);
    
    ErrorLogger::instance().logInfo("RTSP001", 
        QString("Probing channels 1-%1 for %2:%3").arg(totalChannels).arg(ipAddress).arg(port));
    
    for (int channel = 1; channel <= totalChannels; ++channel) {
        emit channelProgress(channel, totalChannels);
        
        QStringList urls = generateRTSPUrls(ipAddress, port, username, password, brand, channel);
        bool found = false;
        
        for (const QString& url : urls) {
            RTSPProbeResult result = probe(url);
            
            if (result.success && result.streamInfo.hasVideo) {
                results[channel] = result;
                found = true;
                
                ErrorLogger::instance().logInfo("RTSP002", 
                    QString("Channel %1 detected: %2x%3, codec: %4")
                        .arg(channel)
                        .arg(result.streamInfo.resolution.width())
                        .arg(result.streamInfo.resolution.height())
                        .arg(videoCodecToString(result.streamInfo.videoCodec)));
                break;
            }
        }
        
        // If no stream found after trying all URLs, assume this channel doesn't exist
        if (!found) {
            // Stop scanning if we've had 3 consecutive failures (DVR has fewer channels)
            if (channel >= 3) {
                bool allPreviousFailed = true;
                for (int prev = channel - 2; prev < channel; ++prev) {
                    if (results.contains(prev)) {
                        allPreviousFailed = false;
                        break;
                    }
                }
                if (allPreviousFailed) {
                    ErrorLogger::instance().logInfo("RTSP003", 
                        QString("No more channels after %1, stopping probe").arg(channel - 3));
                    break;
                }
            }
        }
    }
    
    ErrorLogger::instance().logInfo("RTSP004", 
        QString("Channel probing complete: %1 channels found").arg(results.size()));
    
    return results;
}

QString RTSPProbe::getRecommendedContainer(VideoCodec videoCodec, AudioCodec audioCodec) {
    // H.266 (VVC) - Best support in MP4 and MKV
    if (videoCodec == VideoCodec::H266) {
        return "mp4"; // MP4 has better tooling support for VVC
    }
    
    // H.265+ and H.265 - MP4 or MKV
    if (videoCodec == VideoCodec::H265Plus || videoCodec == VideoCodec::H265) {
        // TS container is safest for H.265+ proprietary extensions
        if (videoCodec == VideoCodec::H265Plus) {
            return "ts";
        }
        return "mp4";
    }
    
    // H.264 - Universal support in MP4
    if (videoCodec == VideoCodec::H264) {
        return "mp4";
    }
    
    // MJPEG - AVI or MP4
    if (videoCodec == VideoCodec::MJPEG) {
        return "avi";
    }
    
    // VP8/VP9 - MKV is native container
    if (videoCodec == VideoCodec::VP8 || videoCodec == VideoCodec::VP9) {
        return "mkv";
    }
    
    // Default to MP4 for unknown codecs
    return "mp4";
}

QString RTSPProbe::getFileExtension(const QString& container) {
    static QMap<QString, QString> extensions = {
        {"mp4", "mp4"},
        {"mkv", "mkv"},
        {"avi", "avi"},
        {"ts", "ts"},
        {"mpegts", "ts"},
        {"mov", "mov"},
        {"flv", "flv"}
    };
    
    return extensions.value(container.toLower(), "mp4");
}

QStringList RTSPProbe::generateRTSPUrls(
    const QString& ipAddress,
    int port,
    const QString& username,
    const QString& password,
    DVRBrand brand,
    int channel)
{
    QStringList urls;
    QString auth;
    
    if (!username.isEmpty()) {
        auth = QString("%1:%2@").arg(username, password);
    }
    
    QString base = QString("rtsp://%1%2:%3").arg(auth, ipAddress).arg(port);
    
    switch (brand) {
        case DVRBrand::Hikvision:
            // Hikvision typical paths
            urls << QString("%1/Streaming/Channels/%2").arg(base).arg(channel * 100 + 1);
            urls << QString("%1/h264/ch%2/main/av_stream").arg(base).arg(channel);
            urls << QString("%1/ISAPI/Streaming/Channels/%2").arg(base).arg(channel * 100 + 1);
            urls << QString("%1/Streaming/Channels/%2").arg(base).arg(channel);
            break;
            
        case DVRBrand::Dahua:
            // Dahua typical paths
            urls << QString("%1/cam/realmonitor?channel=%2&subtype=0").arg(base).arg(channel);
            urls << QString("%1/cam/realmonitor?channel=%2&subtype=1").arg(base).arg(channel);
            urls << QString("%1/live/ch%2").arg(base).arg(channel);
            break;
            
        case DVRBrand::Axis:
            // Axis typical paths
            urls << QString("%1/axis-media/media.amp?camera=%2").arg(base).arg(channel);
            urls << QString("%1/axis-media/media.amp").arg(base);
            urls << QString("%1/mpeg4/media.amp").arg(base);
            break;
            
        case DVRBrand::Bosch:
            // Bosch typical paths
            urls << QString("%1/video?inst=%2").arg(base).arg(channel);
            urls << QString("%1/video").arg(base);
            break;
            
        case DVRBrand::Reolink:
            // Reolink typical paths
            urls << QString("%1/h264Preview%2_main").arg(base).arg(channel);
            urls << QString("%1/h264Preview%2_sub").arg(base).arg(channel);
            urls << QString("%1/Preview/%2").arg(base).arg(channel);
            break;
            
        case DVRBrand::TPLinkTapo:
            // TP-Link/Tapo typical paths
            urls << QString("%1/stream%2").arg(base).arg(channel);
            urls << QString("%1/live").arg(base);
            break;
            
        case DVRBrand::Uniview:
            // Uniview typical paths
            urls << QString("%1/video%2").arg(base).arg(channel);
            urls << QString("%1/tracks/%2").arg(base).arg(channel);
            break;
            
        case DVRBrand::Sony:
            // Sony typical paths
            urls << QString("%1/media/video%2").arg(base).arg(channel);
            urls << QString("%1/media/video1").arg(base);
            break;
            
        case DVRBrand::Samsung:
        case DVRBrand::Hanwha:
            // Samsung/Hanwha typical paths
            urls << QString("%1/profile%2").arg(base).arg(channel);
            urls << QString("%1/live/ch%2").arg(base).arg(channel);
            break;
            
        case DVRBrand::Panasonic:
            // Panasonic typical paths
            urls << QString("%1/nphMpeg4/g726-%2").arg(base).arg(channel);
            urls << QString("%1/SrvMedia").arg(base);
            break;
            
        case DVRBrand::Vivotek:
            // Vivotek typical paths
            urls << QString("%1/live%2.sdp").arg(base).arg(channel);
            urls << QString("%1/video.h264").arg(base);
            break;
            
        case DVRBrand::Honeywell:
            // Honeywell typical paths
            urls << QString("%1/h264/ch%2/main/av_stream").arg(base).arg(channel);
            urls << QString("%1/live/ch%2").arg(base).arg(channel);
            break;
            
        default:
            // Generic DVR - try common patterns
            urls << QString("%1/stream%2").arg(base).arg(channel);
            urls << QString("%1/live/ch%2").arg(base).arg(channel);
            urls << QString("%1/ch%2").arg(base).arg(channel);
            urls << QString("%1/cam%2").arg(base).arg(channel);
            urls << QString("%1/video%2").arg(base).arg(channel);
            urls << QString("%1/h264/ch%2").arg(base).arg(channel);
            urls << QString("%1/track%2").arg(base).arg(channel);
            // Try without channel for single-channel devices
            if (channel == 1) {
                urls << QString("%1/stream").arg(base);
                urls << QString("%1/live").arg(base);
                urls << QString("%1").arg(base);
            }
            break;
    }
    
    return urls;
}

bool RTSPProbe::openStream(const QString& url, AVFormatContext** formatCtx) {
    // Create new options for this specific probe
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", QString::number(m_timeoutMs * 1000).toUtf8(), 0);
    av_dict_set(&options, "max_delay", "500000", 0);
    av_dict_set(&options, "analyzeduration", QString::number(m_timeoutMs * 1000).toUtf8(), 0);
    av_dict_set(&options, "probesize", "5000000", 0);
    
    // Open input
    int ret = avformat_open_input(formatCtx, url.toUtf8().constData(), nullptr, &options);
    
    av_dict_free(&options);
    
    if (ret != 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        
        ErrorLogger::instance().logWarning("RTSP005", 
            QString("Failed to open stream: %1").arg(errBuf), url);
        return false;
    }
    
    // Get stream info
    ret = avformat_find_stream_info(*formatCtx, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        
        ErrorLogger::instance().logWarning("RTSP006", 
            QString("Failed to get stream info: %1").arg(errBuf), url);
        avformat_close_input(formatCtx);
        return false;
    }
    
    return true;
}

StreamInfo RTSPProbe::extractStreamInfo(AVFormatContext* formatCtx) {
    StreamInfo info;
    
    if (!formatCtx) {
        info.errorMessage = "Null format context";
        return info;
    }
    
    // Get container format
    if (formatCtx->iformat) {
        info.containerFormat = formatCtx->iformat->name;
    }
    
    // Get duration
    if (formatCtx->duration != AV_NOPTS_VALUE) {
        info.duration = static_cast<double>(formatCtx->duration) / AV_TIME_BASE;
    }
    
    // Get bit rate
    if (formatCtx->bit_rate > 0) {
        info.bitRate = formatCtx->bit_rate;
    }
    
    // Find video and audio streams
    for (unsigned int i = 0; i < formatCtx->nb_streams; ++i) {
        AVStream* stream = formatCtx->streams[i];
        AVCodecParameters* codecPar = stream->codecpar;
        
        if (codecPar->codec_type == AVMEDIA_TYPE_VIDEO && !info.hasVideo) {
            info.hasVideo = true;
            info.videoCodec = mapVideoCodec(codecPar->codec_id, codecPar);
            info.videoCodecName = avcodec_get_name(codecPar->codec_id);
            
            // Resolution
            info.resolution = QSize(codecPar->width, codecPar->height);
            
            // Frame rate
            if (stream->avg_frame_rate.den != 0) {
                info.frameRateNum = stream->avg_frame_rate.num;
                info.frameRateDen = stream->avg_frame_rate.den;
            } else if (stream->r_frame_rate.den != 0) {
                info.frameRateNum = stream->r_frame_rate.num;
                info.frameRateDen = stream->r_frame_rate.den;
            }
            
            // Profile and level
            info.profile = QString::number(codecPar->profile);
            info.level = QString::number(codecPar->level);
            
            // Bit rate from stream
            if (codecPar->bit_rate > 0) {
                info.bitRate = codecPar->bit_rate;
            }
            
            // Check for H.265+
            if (info.videoCodec == VideoCodec::H265) {
                if (detectH265Plus(formatCtx, i)) {
                    info.videoCodec = VideoCodec::H265Plus;
                    info.videoCodecName = "hevc+";
                }
            }
            
        } else if (codecPar->codec_type == AVMEDIA_TYPE_AUDIO && !info.hasAudio) {
            info.hasAudio = true;
            info.audioCodec = mapAudioCodec(codecPar->codec_id);
            info.audioCodecName = avcodec_get_name(codecPar->codec_id);
            
            // Audio properties
            info.audioSampleRate = codecPar->sample_rate;
            info.audioChannels = codecPar->ch_layout.nb_channels;
        }
    }
    
    info.isValid = info.hasVideo;
    if (!info.isValid) {
        info.errorMessage = "No video stream found";
    }
    
    return info;
}

VideoCodec RTSPProbe::mapVideoCodec(AVCodecID codecId, const AVCodecParameters* codecPar) {
    switch (codecId) {
        case AV_CODEC_ID_H264:
            return VideoCodec::H264;
            
        case AV_CODEC_ID_HEVC:
#if AV_CODEC_ID_H265 != AV_CODEC_ID_HEVC
        case AV_CODEC_ID_H265:
#endif
            // H.265+ detection is done separately
            return VideoCodec::H265;
            
        case AV_CODEC_ID_VVC:
            return VideoCodec::H266;
            
        case AV_CODEC_ID_MJPEG:
            return VideoCodec::MJPEG;
            
        case AV_CODEC_ID_MPEG4:
            return VideoCodec::MPEG4;
            
        case AV_CODEC_ID_VP8:
            return VideoCodec::VP8;
            
        case AV_CODEC_ID_VP9:
            return VideoCodec::VP9;
            
        default:
            return VideoCodec::Unknown;
    }
}

AudioCodec RTSPProbe::mapAudioCodec(AVCodecID codecId) {
    switch (codecId) {
        case AV_CODEC_ID_AAC:
            return AudioCodec::AAC;
            
        case AV_CODEC_ID_PCM_S16LE:
        case AV_CODEC_ID_PCM_S16BE:
        case AV_CODEC_ID_PCM_U16LE:
        case AV_CODEC_ID_PCM_U16BE:
        case AV_CODEC_ID_PCM_S24LE:
        case AV_CODEC_ID_PCM_S24BE:
        case AV_CODEC_ID_PCM_S32LE:
        case AV_CODEC_ID_PCM_S32BE:
        case AV_CODEC_ID_PCM_U8:
            return AudioCodec::PCM;
            
        case AV_CODEC_ID_PCM_ALAW:
            return AudioCodec::G711A;
            
        case AV_CODEC_ID_PCM_MULAW:
            return AudioCodec::G711U;
            
        case AV_CODEC_ID_ADPCM_G726:
            return AudioCodec::G726;
            
        case AV_CODEC_ID_MP3:
            return AudioCodec::MP3;
            
        case AV_CODEC_ID_OPUS:
            return AudioCodec::Opus;
            
        default:
            return AudioCodec::Unknown;
    }
}

bool RTSPProbe::detectH265Plus(AVFormatContext* formatCtx, int streamIndex) {
    // H.265+ is a proprietary extension by Hikvision/Dahua
    // Detection is heuristic based on:
    // 1. Metadata tags indicating smart codec
    // 2. Stream properties
    
    if (!formatCtx || streamIndex < 0 || 
        static_cast<unsigned int>(streamIndex) >= formatCtx->nb_streams) {
        return false;
    }
    
    AVStream* stream = formatCtx->streams[streamIndex];
    
    // Check metadata for H.265+ indicators
    if (stream->metadata) {
        AVDictionaryEntry* entry = nullptr;
        while ((entry = av_dict_get(stream->metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            QString key = entry->key;
            QString value = entry->value;
            
            if (key.toLower().contains("smart") || 
                key.toLower().contains("hevcplus") ||
                key.toLower().contains("h265plus") ||
                value.toLower().contains("smart") ||
                value.toLower().contains("hevc+")) {
                return true;
            }
        }
    }
    
    // Check format metadata
    if (formatCtx->metadata) {
        AVDictionaryEntry* entry = nullptr;
        while ((entry = av_dict_get(formatCtx->metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            QString key = entry->key;
            QString value = entry->value;
            
            if ((key.toLower().contains("codec") && value.toLower().contains("smart")) ||
                (key.toLower().contains("encoding") && value.toLower().contains("plus"))) {
                return true;
            }
        }
    }
    
    return false;
}

} // namespace telecloud
