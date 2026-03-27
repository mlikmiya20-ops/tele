/**
 * @file recorder.h
 * @brief Multi-channel RTSP stream recorder with stream copy (remuxing)
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 * 
 * This file implements the recording functionality that:
 * - Performs stream copy without transcoding (no CPU-intensive decoding/encoding)
 * - Supports H.264, H.265, H.265+, H.266 codecs
 * - Rotates files based on configurable duration
 * - Handles multiple concurrent recordings
 * - Provides automatic reconnection on network issues
 */

#ifndef RECORDER_H
#define RECORDER_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QMap>
#include <QJsonObject>
#include <QJsonDocument>
#include <atomic>
#include <memory>

// FFmpeg headers
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
}

#include "rtspprobe.h"
#include "networkscanner.h"

namespace telecloud {

/**
 * @struct RecordingConfig
 * @brief Configuration for a recording session
 */
struct RecordingConfig {
    QString rtspUrl;                    ///< RTSP stream URL
    int cameraNumber;                   ///< Camera/channel number
    QString outputDirectory;            ///< Directory for output files
    int segmentDurationMinutes;         ///< File rotation duration in minutes
    VideoCodec videoCodec;              ///< Detected video codec
    AudioCodec audioCodec;              ///< Detected audio codec
    QString containerFormat;            ///< Output container format
    int maxRetries;                     ///< Maximum reconnection attempts
    int retryDelayMs;                   ///< Delay between retries in ms
    
    /**
     * @brief Default constructor with sensible defaults
     */
    RecordingConfig()
        : cameraNumber(1)
        , segmentDurationMinutes(60)
        , videoCodec(VideoCodec::Unknown)
        , audioCodec(AudioCodec::Unknown)
        , containerFormat("mp4")
        , maxRetries(10)
        , retryDelayMs(5000)
    {}
    
    /**
     * @brief Converts config to JSON
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["rtspUrl"] = rtspUrl;
        obj["cameraNumber"] = cameraNumber;
        obj["outputDirectory"] = outputDirectory;
        obj["segmentDurationMinutes"] = segmentDurationMinutes;
        obj["videoCodec"] = static_cast<int>(videoCodec);
        obj["audioCodec"] = static_cast<int>(audioCodec);
        obj["containerFormat"] = containerFormat;
        obj["maxRetries"] = maxRetries;
        obj["retryDelayMs"] = retryDelayMs;
        return obj;
    }
    
    /**
     * @brief Creates config from JSON
     */
    static RecordingConfig fromJson(const QJsonObject& obj) {
        RecordingConfig config;
        config.rtspUrl = obj["rtspUrl"].toString();
        config.cameraNumber = obj["cameraNumber"].toInt(1);
        config.outputDirectory = obj["outputDirectory"].toString();
        config.segmentDurationMinutes = obj["segmentDurationMinutes"].toInt(60);
        config.videoCodec = static_cast<VideoCodec>(obj["videoCodec"].toInt());
        config.audioCodec = static_cast<AudioCodec>(obj["audioCodec"].toInt());
        config.containerFormat = obj["containerFormat"].toString("mp4");
        config.maxRetries = obj["maxRetries"].toInt(10);
        config.retryDelayMs = obj["retryDelayMs"].toInt(5000);
        return config;
    }
};

/**
 * @struct RecordingStats
 * @brief Statistics for an active recording
 */
struct RecordingStats {
    int cameraNumber;                   ///< Camera number
    qint64 bytesWritten;                ///< Total bytes written
    qint64 currentFileSize;             ///< Current file size
    int segmentsCompleted;              ///< Number of completed segments
    qint64 recordingDurationMs;         ///< Total recording duration
    qint64 segmentStartTime;            ///< Current segment start time (ms since epoch)
    int retryCount;                     ///< Number of reconnection attempts
    bool isActive;                      ///< Whether recording is active
    QString currentFile;                ///< Current output file path
    QString lastError;                  ///< Last error message
    float bitrate;                      ///< Current bitrate in kbps
    
    /**
     * @brief Default constructor
     */
    RecordingStats()
        : cameraNumber(0)
        , bytesWritten(0)
        , currentFileSize(0)
        , segmentsCompleted(0)
        , recordingDurationMs(0)
        , segmentStartTime(0)
        , retryCount(0)
        , isActive(false)
        , bitrate(0.0f)
    {}
    
    /**
     * @brief Converts stats to JSON
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["cameraNumber"] = cameraNumber;
        obj["bytesWritten"] = bytesWritten;
        obj["currentFileSize"] = currentFileSize;
        obj["segmentsCompleted"] = segmentsCompleted;
        obj["recordingDurationMs"] = recordingDurationMs;
        obj["segmentStartTime"] = segmentStartTime;
        obj["retryCount"] = retryCount;
        obj["isActive"] = isActive;
        obj["currentFile"] = currentFile;
        obj["lastError"] = lastError;
        obj["bitrate"] = bitrate;
        return obj;
    }
};

/**
 * @class SingleRecorder
 * @brief Records a single RTSP stream with stream copy
 * 
 * SingleRecorder handles the recording of one RTSP stream, performing
 * stream copy (remuxing) without any transcoding. It manages file rotation,
 * reconnection, and error handling for a single camera.
 */
class SingleRecorder : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param config Recording configuration
     * @param parent Parent QObject
     */
    explicit SingleRecorder(const RecordingConfig& config, QObject* parent = nullptr);
    
    /**
     * @brief Destructor - stops recording
     */
    ~SingleRecorder();
    
    /**
     * @brief Gets the camera number
     * @return Camera number
     */
    int cameraNumber() const { return m_config.cameraNumber; }
    
    /**
     * @brief Gets current recording statistics
     * @return RecordingStats structure
     */
    RecordingStats getStats() const;
    
    /**
     * @brief Checks if recording is active
     * @return true if recording
     */
    bool isActive() const { return m_active.load(); }

public slots:
    /**
     * @brief Starts the recording
     */
    void start();
    
    /**
     * @brief Stops the recording
     */
    void stop();
    
    /**
     * @brief Pauses the recording
     */
    void pause();
    
    /**
     * @brief Resumes the recording
     */
    void resume();

signals:
    /**
     * @brief Emitted when recording starts
     * @param cameraNumber The camera number
     */
    void started(int cameraNumber);
    
    /**
     * @brief Emitted when recording stops
     * @param cameraNumber The camera number
     */
    void stopped(int cameraNumber);
    
    /**
     * @brief Emitted when a new segment starts
     * @param cameraNumber The camera number
     * @param filePath The new file path
     */
    void segmentStarted(int cameraNumber, const QString& filePath);
    
    /**
     * @brief Emitted when a segment completes
     * @param cameraNumber The camera number
     * @param filePath The completed file path
     * @param durationMs Segment duration in milliseconds
     */
    void segmentCompleted(int cameraNumber, const QString& filePath, qint64 durationMs);
    
    /**
     * @brief Emitted on error
     * @param cameraNumber The camera number
     * @param error Error message
     */
    void error(int cameraNumber, const QString& error);
    
    /**
     * @brief Emitted when reconnection is attempted
     * @param cameraNumber The camera number
     * @param attempt Attempt number
     * @param maxAttempts Maximum attempts
     */
    void reconnecting(int cameraNumber, int attempt, int maxAttempts);
    
    /**
     * @brief Emitted periodically with stats update
     * @param stats Current recording statistics
     */
    void statsUpdate(const RecordingStats& stats);

private:
    /**
     * @brief Main recording loop
     */
    void recordingLoop();
    
    /**
     * @brief Opens input RTSP stream
     * @return true if successful
     */
    bool openInput();
    
    /**
     * @brief Closes input stream
     */
    void closeInput();
    
    /**
     * @brief Opens output file
     * @return true if successful
     */
    bool openOutput();
    
    /**
     * @brief Closes output file
     */
    void closeOutput();
    
    /**
     * @brief Generates output filename based on configuration
     * @return Full file path
     */
    QString generateOutputFilename();
    
    /**
     * @brief Checks if segment rotation is needed
     * @return true if rotation needed
     */
    bool needsRotation() const;
    
    /**
     * @brief Performs segment rotation
     */
    void rotateSegment();
    
    /**
     * @brief Processes a single packet
     * @param packet The AVPacket to process
     * @return true if successful
     */
    bool processPacket(AVPacket* packet);
    
    /**
     * @brief Attempts to reconnect after connection loss
     * @return true if reconnected
     */
    bool attemptReconnect();
    
    /**
     * @brief Updates recording statistics
     */
    void updateStats();
    
    RecordingConfig m_config;               ///< Recording configuration
    AVFormatContext* m_inputCtx;            ///< Input format context
    AVFormatContext* m_outputCtx;           ///< Output format context
    int m_videoStreamIndex;                 ///< Input video stream index
    int m_audioStreamIndex;                 ///< Input audio stream index
    int m_outputVideoIndex;                 ///< Output video stream index
    int m_outputAudioIndex;                 ///< Output audio stream index
    qint64 m_startTime;                     ///< Recording start time
    qint64 m_segmentStart;                  ///< Current segment start time
    qint64 m_lastPacketTime;                ///< Last packet timestamp
    std::atomic<bool> m_active{false};              ///< Active flag
    std::atomic<bool> m_paused{false};              ///< Paused flag
    std::atomic<bool> m_stopRequested{false};       ///< Stop request flag
    QString m_currentOutputFile;            ///< Current output file path
    RecordingStats m_stats;                 ///< Recording statistics
    QThread* m_workerThread;                ///< Worker thread
    QTimer* m_statsTimer;                   ///< Stats update timer
    mutable QMutex m_mutex;                 ///< Mutex for thread safety
    int64_t m_firstDts;                     ///< First DTS for timestamp calculation
    int64_t m_prevDts;                      ///< Previous DTS for discontinuity detection
};

/**
 * @class Recorder
 * @brief Multi-channel recorder manager
 * 
 * Recorder manages multiple SingleRecorder instances for concurrent
 * recording of multiple camera streams. It provides a unified interface
 * for starting, stopping, and monitoring all recordings.
 * 
 * Usage:
 * @code
 * Recorder recorder;
 * recorder.setStorageDirectory("/path/to/recordings");
 * recorder.setSegmentDuration(60); // 60 minutes
 * 
 * QList<RecordingConfig> configs;
 * // ... populate configs
 * 
 * recorder.startAll(configs);
 * 
 * // Later...
 * recorder.stopAll();
 * @endcode
 */
class Recorder : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(int activeCameraCount READ activeCameraCount NOTIFY activeCameraCountChanged)
    Q_PROPERTY(QString storageDirectory READ storageDirectory WRITE setStorageDirectory NOTIFY storageDirectoryChanged)
    Q_PROPERTY(int segmentDuration READ segmentDuration WRITE setSegmentDuration NOTIFY segmentDurationChanged)

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit Recorder(QObject* parent = nullptr);
    
    /**
     * @brief Destructor - stops all recordings
     */
    ~Recorder();
    
    /**
     * @brief Checks if any recording is active
     * @return true if recording
     */
    bool isRecording() const { return m_recording.load(); }
    
    /**
     * @brief Gets the number of active recordings
     * @return Active camera count
     */
    int activeCameraCount() const { return m_recorders.size(); }
    
    /**
     * @brief Gets the storage directory
     * @return Storage directory path
     */
    QString storageDirectory() const { return m_storageDirectory; }
    
    /**
     * @brief Sets the storage directory
     * @param dir Directory path
     */
    void setStorageDirectory(const QString& dir);
    
    /**
     * @brief Gets the segment duration
     * @return Segment duration in minutes
     */
    int segmentDuration() const { return m_segmentDurationMinutes; }
    
    /**
     * @brief Sets the segment duration
     * @param minutes Segment duration in minutes
     */
    void setSegmentDuration(int minutes);
    
    /**
     * @brief Gets statistics for all recordings
     * @return Map of camera number to stats
     */
    QMap<int, RecordingStats> getAllStats() const;
    
    /**
     * @brief Gets statistics for a specific camera
     * @param cameraNumber Camera number
     * @return Recording stats
     */
    RecordingStats getStats(int cameraNumber) const;

public slots:
    /**
     * @brief Starts recording all configured cameras
     * @param configs List of recording configurations
     */
    void startAll(const QList<RecordingConfig>& configs);
    
    /**
     * @brief Starts recording a single camera
     * @param config Recording configuration
     */
    void startOne(const RecordingConfig& config);
    
    /**
     * @brief Stops all recordings
     */
    void stopAll();
    
    /**
     * @brief Stops a specific camera recording
     * @param cameraNumber Camera number to stop
     */
    void stopOne(int cameraNumber);
    
    /**
     * @brief Pauses all recordings
     */
    void pauseAll();
    
    /**
     * @brief Resumes all recordings
     */
    void resumeAll();

signals:
    /**
     * @brief Emitted when recording state changes
     */
    void recordingChanged();
    
    /**
     * @brief Emitted when active camera count changes
     */
    void activeCameraCountChanged();
    
    /**
     * @brief Emitted when storage directory changes
     */
    void storageDirectoryChanged();
    
    /**
     * @brief Emitted when segment duration changes
     */
    void segmentDurationChanged();
    
    /**
     * @brief Emitted when a camera starts recording
     * @param cameraNumber The camera number
     */
    void cameraStarted(int cameraNumber);
    
    /**
     * @brief Emitted when a camera stops recording
     * @param cameraNumber The camera number
     */
    void cameraStopped(int cameraNumber);
    
    /**
     * @brief Emitted when a segment starts
     * @param cameraNumber The camera number
     * @param filePath The file path
     */
    void segmentStarted(int cameraNumber, const QString& filePath);
    
    /**
     * @brief Emitted when a segment completes
     * @param cameraNumber The camera number
     * @param filePath The file path
     * @param durationMs Duration in ms
     */
    void segmentCompleted(int cameraNumber, const QString& filePath, qint64 durationMs);
    
    /**
     * @brief Emitted on error
     * @param cameraNumber The camera number
     * @param error Error message
     */
    void error(int cameraNumber, const QString& error);
    
    /**
     * @brief Emitted periodically with total stats
     * @param statsMap Map of camera to stats
     */
    void statsUpdated(const QMap<int, RecordingStats>& statsMap);

private slots:
    /**
     * @brief Handles SingleRecorder started signal
     */
    void onRecorderStarted(int cameraNumber);
    
    /**
     * @brief Handles SingleRecorder stopped signal
     */
    void onRecorderStopped(int cameraNumber);
    
    /**
     * @brief Handles SingleRecorder error signal
     */
    void onRecorderError(int cameraNumber, const QString& error);
    
    /**
     * @brief Handles SingleRecorder segment started signal
     */
    void onSegmentStarted(int cameraNumber, const QString& filePath);
    
    /**
     * @brief Handles SingleRecorder segment completed signal
     */
    void onSegmentCompleted(int cameraNumber, const QString& filePath, qint64 durationMs);
    
    /**
     * @brief Handles stats update from SingleRecorder
     */
    void onStatsUpdate(const RecordingStats& stats);
    
    /**
     * @brief Emits periodic stats update
     */
    void onStatsTimer();

private:
    QString m_storageDirectory;             ///< Storage directory for recordings
    int m_segmentDurationMinutes;           ///< Segment duration in minutes
    QMap<int, SingleRecorder*> m_recorders; ///< Map of camera number to recorder
    std::atomic<bool> m_recording{false};           ///< Recording state flag
    QTimer* m_statsTimer;                   ///< Periodic stats timer
    mutable QMutex m_mutex;                 ///< Mutex for thread safety
};

} // namespace telecloud

#endif // RECORDER_H
