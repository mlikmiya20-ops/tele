/**
 * @file rtspprobe.h
 * @brief RTSP probe for stream testing and codec detection
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 * 
 * This file implements RTSP probing functionality using FFmpeg libraries
 * (libavformat, libavcodec) to:
 * - Test RTSP URL validity without full decoding
 * - Detect video codecs (H.264, H.265, H.265+, H.266)
 * - Detect audio codecs (AAC, PCM, G.711)
 * - Extract stream metadata
 */

#ifndef RTSPPROBE_H
#define RTSPPROBE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QSize>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QAtomicInt>
#include <memory>
#include "networkscanner.h"  // For DVRBrand enum

// FFmpeg headers
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
}

namespace telecloud {

/**
 * @enum VideoCodec
 * @brief Supported video codecs for stream copy
 */
enum class VideoCodec {
    Unknown = 0,
    H264,       ///< H.264/AVC - Advanced Video Coding
    H265,       ///< H.265/HEVC - High Efficiency Video Coding
    H265Plus,   ///< H.265+ - Proprietary optimized HEVC (Hikvision/Dahua)
    H266,       ///< H.266/VVC - Versatile Video Coding
    MJPEG,      ///< Motion JPEG
    MPEG4,      ///< MPEG-4 Part 2
    VP8,        ///< VP8
    VP9         ///< VP9
};

/**
 * @enum AudioCodec
 * @brief Supported audio codecs
 */
enum class AudioCodec {
    Unknown = 0,
    AAC,        ///< AAC - Advanced Audio Coding
    PCM,        ///< PCM - Pulse Code Modulation
    G711A,      ///< G.711 A-law
    G711U,      ///< G.711 μ-law
    G726,       ///< G.726 ADPCM
    MP3,        ///< MP3
    Opus        ///< Opus
};

/**
 * @brief Converts VideoCodec enum to string
 */
inline QString videoCodecToString(VideoCodec codec) {
    switch (codec) {
        case VideoCodec::H264: return "H.264";
        case VideoCodec::H265: return "H.265";
        case VideoCodec::H265Plus: return "H.265+";
        case VideoCodec::H266: return "H.266";
        case VideoCodec::MJPEG: return "MJPEG";
        case VideoCodec::MPEG4: return "MPEG-4";
        case VideoCodec::VP8: return "VP8";
        case VideoCodec::VP9: return "VP9";
        default: return "Unknown";
    }
}

/**
 * @brief Converts AudioCodec enum to string
 */
inline QString audioCodecToString(AudioCodec codec) {
    switch (codec) {
        case AudioCodec::AAC: return "AAC";
        case AudioCodec::PCM: return "PCM";
        case AudioCodec::G711A: return "G.711 A-law";
        case AudioCodec::G711U: return "G.711 μ-law";
        case AudioCodec::G726: return "G.726";
        case AudioCodec::MP3: return "MP3";
        case AudioCodec::Opus: return "Opus";
        default: return "Unknown";
    }
}

/**
 * @struct StreamInfo
 * @brief Complete information about a media stream
 */
struct StreamInfo {
    // Video information
    VideoCodec videoCodec = VideoCodec::Unknown;
    QString videoCodecName;
    QSize resolution;
    int frameRateNum = 0;
    int frameRateDen = 1;
    int bitRate = 0;
    QString profile;
    QString level;
    
    // Audio information
    AudioCodec audioCodec = AudioCodec::Unknown;
    QString audioCodecName;
    int audioSampleRate = 0;
    int audioChannels = 0;
    
    // Container information
    QString containerFormat;
    double duration = 0.0;
    
    // Stream status
    bool isValid = false;
    bool hasVideo = false;
    bool hasAudio = false;
    QString errorMessage;
    
    /**
     * @brief Converts StreamInfo to JSON object
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["videoCodec"] = static_cast<int>(videoCodec);
        obj["videoCodecName"] = videoCodecName;
        obj["width"] = resolution.width();
        obj["height"] = resolution.height();
        obj["frameRateNum"] = frameRateNum;
        obj["frameRateDen"] = frameRateDen;
        obj["bitRate"] = bitRate;
        obj["profile"] = profile;
        obj["level"] = level;
        obj["audioCodec"] = static_cast<int>(audioCodec);
        obj["audioCodecName"] = audioCodecName;
        obj["audioSampleRate"] = audioSampleRate;
        obj["audioChannels"] = audioChannels;
        obj["containerFormat"] = containerFormat;
        obj["duration"] = duration;
        obj["isValid"] = isValid;
        obj["hasVideo"] = hasVideo;
        obj["hasAudio"] = hasAudio;
        obj["errorMessage"] = errorMessage;
        return obj;
    }
    
    /**
     * @brief Creates StreamInfo from JSON object
     */
    static StreamInfo fromJson(const QJsonObject& obj) {
        StreamInfo info;
        info.videoCodec = static_cast<VideoCodec>(obj["videoCodec"].toInt());
        info.videoCodecName = obj["videoCodecName"].toString();
        info.resolution = QSize(obj["width"].toInt(), obj["height"].toInt());
        info.frameRateNum = obj["frameRateNum"].toInt();
        info.frameRateDen = obj["frameRateDen"].toInt();
        info.bitRate = obj["bitRate"].toInt();
        info.profile = obj["profile"].toString();
        info.level = obj["level"].toString();
        info.audioCodec = static_cast<AudioCodec>(obj["audioCodec"].toInt());
        info.audioCodecName = obj["audioCodecName"].toString();
        info.audioSampleRate = obj["audioSampleRate"].toInt();
        info.audioChannels = obj["audioChannels"].toInt();
        info.containerFormat = obj["containerFormat"].toString();
        info.duration = obj["duration"].toDouble();
        info.isValid = obj["isValid"].toBool();
        info.hasVideo = obj["hasVideo"].toBool();
        info.hasAudio = obj["hasAudio"].toBool();
        info.errorMessage = obj["errorMessage"].toString();
        return info;
    }
    
    /**
     * @brief Gets the frame rate as a floating point value
     */
    double getFrameRate() const {
        if (frameRateDen == 0) return 0.0;
        return static_cast<double>(frameRateNum) / frameRateDen;
    }
};

/**
 * @struct RTSPProbeResult
 * @brief Result of an RTSP probe operation
 */
struct RTSPProbeResult {
    bool success = false;
    int httpResponse = 0;
    QString rtspUrl;
    StreamInfo streamInfo;
    QString errorMessage;
    
    /**
     * @brief Converts result to JSON object
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["success"] = success;
        obj["httpResponse"] = httpResponse;
        obj["rtspUrl"] = rtspUrl;
        obj["streamInfo"] = streamInfo.toJson();
        obj["errorMessage"] = errorMessage;
        return obj;
    }
};

/**
 * @class RTSPProbe
 * @brief Probes RTSP streams for validity and codec information
 * 
 * RTSPProbe uses FFmpeg libraries to probe RTSP streams without
 * full decoding, extracting codec information and stream metadata.
 * 
 * Usage:
 * @code
 * RTSPProbe probe;
 * RTSPProbeResult result = probe.probe("rtsp://admin:pass@192.168.1.100:554/stream1");
 * if (result.success) {
 *     qDebug() << "Codec:" << videoCodecToString(result.streamInfo.videoCodec);
 * }
 * @endcode
 */
class RTSPProbe : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit RTSPProbe(QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~RTSPProbe();
    
    /**
     * @brief Sets the connection timeout for probing
     * @param timeoutMs Timeout in milliseconds
     */
    void setTimeout(int timeoutMs) { m_timeoutMs = timeoutMs; }
    
    /**
     * @brief Gets the current timeout setting
     * @return Timeout in milliseconds
     */
    int timeout() const { return m_timeoutMs; }
    
    /**
     * @brief Probes an RTSP URL for stream information
     * @param url The RTSP URL to probe
     * @return Probe result with stream information
     * 
     * This method opens the RTSP stream using libavformat and extracts
     * stream information without decoding any frames. It detects the
     * video codec (H.264, H.265, H.265+, H.266) and audio codec if present.
     */
    RTSPProbeResult probe(const QString& url);
    
    /**
     * @brief Probes an RTSP URL asynchronously
     * @param url The RTSP URL to probe
     * 
     * Results are emitted via the probeCompleted signal.
     */
    Q_INVOKABLE void probeAsync(const QString& url);
    
    /**
     * @brief Tests multiple RTSP URLs for a DVR
     * @param ipAddress DVR IP address
     * @param port RTSP port
     * @param username DVR username
     * @param password DVR password
     * @param brand DVR brand for URL pattern selection
     * @param maxChannels Maximum channels to test (1-64)
     * @return Map of channel numbers to probe results
     * 
     * Tests common RTSP URL patterns for the specified brand to detect
     * available camera channels.
     */
    QMap<int, RTSPProbeResult> probeChannels(
        const QString& ipAddress,
        int port,
        const QString& username,
        const QString& password,
        DVRBrand brand,
        int maxChannels = 64
    );
    
    /**
     * @brief Gets the recommended output container for a codec
     * @param videoCodec The video codec
     * @param audioCodec The audio codec (optional)
     * @return Recommended container format (mp4, mkv, ts, avi)
     */
    static QString getRecommendedContainer(VideoCodec videoCodec, 
                                           AudioCodec audioCodec = AudioCodec::Unknown);
    
    /**
     * @brief Gets the file extension for a container
     * @param container Container format name
     * @return File extension without dot
     */
    static QString getFileExtension(const QString& container);
    
    /**
     * @brief Generates RTSP URLs for a DVR channel
     * @param ipAddress DVR IP address
     * @param port RTSP port
     * @param username Username
     * @param password Password
     * @param brand DVR brand
     * @param channel Channel number (1-based)
     * @return List of possible RTSP URLs to try
     */
    static QStringList generateRTSPUrls(
        const QString& ipAddress,
        int port,
        const QString& username,
        const QString& password,
        DVRBrand brand,
        int channel
    );
    
signals:
    /**
     * @brief Emitted when an async probe completes
     * @param url The probed URL
     * @param result The probe result
     */
    void probeCompleted(const QString& url, const RTSPProbeResult& result);
    
    /**
     * @brief Emitted during channel probing progress
     * @param current Current channel being probed
     * @param total Total channels to probe
     */
    void channelProgress(int current, int total);

private:
    /**
     * @brief Opens an RTSP stream with FFmpeg
     * @param url The RTSP URL
     * @param formatCtx Output format context
     * @return true if successful
     */
    bool openStream(const QString& url, AVFormatContext** formatCtx);
    
    /**
     * @brief Extracts stream information from format context
     * @param formatCtx The format context
     * @return StreamInfo structure
     */
    StreamInfo extractStreamInfo(AVFormatContext* formatCtx);
    
    /**
     * @brief Maps FFmpeg codec ID to VideoCodec enum
     * @param codecId FFmpeg codec ID
     * @param codecContext Codec context for additional detection
     * @return VideoCodec enum value
     */
    VideoCodec mapVideoCodec(AVCodecID codecId, const AVCodecParameters* codecPar);
    
    /**
     * @brief Maps FFmpeg codec ID to AudioCodec enum
     * @param codecId FFmpeg codec ID
     * @return AudioCodec enum value
     */
    AudioCodec mapAudioCodec(AVCodecID codecId);
    
    /**
     * @brief Detects H.265+ from stream data
     * @param formatCtx Format context
     * @param streamIndex Video stream index
     * @return true if H.265+ detected
     */
    bool detectH265Plus(AVFormatContext* formatCtx, int streamIndex);
    
    int m_timeoutMs = 5000;             ///< Connection timeout
    AVDictionary* m_options = nullptr;  ///< FFmpeg options
};

} // namespace telecloud

#endif // RTSPPROBE_H
