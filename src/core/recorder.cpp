/**
 * @file recorder.cpp
 * @brief Implementation of Recorder and SingleRecorder classes
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 */

#include "recorder.h"
#include "errorlogger.h"
#include <QDebug>
#include <QStandardPaths>
#include <QCoreApplication>

namespace telecloud {

// ============================================================================
// SingleRecorder Implementation
// ============================================================================

SingleRecorder::SingleRecorder(const RecordingConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_inputCtx(nullptr)
    , m_outputCtx(nullptr)
    , m_videoStreamIndex(-1)
    , m_audioStreamIndex(-1)
    , m_outputVideoIndex(-1)
    , m_outputAudioIndex(-1)
    , m_startTime(0)
    , m_segmentStart(0)
    , m_lastPacketTime(0)
    , m_workerThread(nullptr)
    , m_statsTimer(nullptr)
    , m_firstDts(AV_NOPTS_VALUE)
    , m_prevDts(AV_NOPTS_VALUE)
{
    m_active.store(false);
    m_paused.store(false);
    m_stopRequested.store(false);
    
    m_stats.cameraNumber = m_config.cameraNumber;
}

SingleRecorder::~SingleRecorder() {
    stop();
}

void SingleRecorder::start() {
    if (m_active.load()) {
        return;
    }
    
    m_stopRequested.store(false);
    m_paused.store(false);
    
    // Create worker thread
    m_workerThread = QThread::create([this]() {
        recordingLoop();
    });
    
    m_workerThread->start();
    
    // Start stats timer
    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &SingleRecorder::updateStats);
    m_statsTimer->start(1000); // Update stats every second
    
    ErrorLogger::instance().logInfo("REC001", 
        QString("Starting recording for camera %1").arg(m_config.cameraNumber),
        m_config.rtspUrl);
}

void SingleRecorder::stop() {
    if (!m_active.load()) {
        return;
    }
    
    m_stopRequested.store(true);
    
    if (m_workerThread) {
        m_workerThread->wait(5000);
        if (m_workerThread->isRunning()) {
            m_workerThread->terminate();
            m_workerThread->wait();
        }
        delete m_workerThread;
        m_workerThread = nullptr;
    }
    
    if (m_statsTimer) {
        m_statsTimer->stop();
        delete m_statsTimer;
        m_statsTimer = nullptr;
    }
    
    closeOutput();
    closeInput();
    
    m_active.store(false);
    emit stopped(m_config.cameraNumber);
    
    ErrorLogger::instance().logInfo("REC002", 
        QString("Recording stopped for camera %1").arg(m_config.cameraNumber));
}

void SingleRecorder::pause() {
    m_paused.store(true);
    ErrorLogger::instance().logInfo("REC003", 
        QString("Recording paused for camera %1").arg(m_config.cameraNumber));
}

void SingleRecorder::resume() {
    m_paused.store(false);
    ErrorLogger::instance().logInfo("REC004", 
        QString("Recording resumed for camera %1").arg(m_config.cameraNumber));
}

RecordingStats SingleRecorder::getStats() const {
    QMutexLocker locker(&m_mutex);
    return m_stats;
}

void SingleRecorder::recordingLoop() {
    m_active.store(true);
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_segmentStart = m_startTime;
    m_firstDts = AV_NOPTS_VALUE;
    m_prevDts = AV_NOPTS_VALUE;
    
    emit started(m_config.cameraNumber);
    
    while (!m_stopRequested.load()) {
        // Open input stream
        if (!openInput()) {
            QString err = "Failed to open input stream";
            m_stats.lastError = err;
            emit error(m_config.cameraNumber, err);
            
            if (!attemptReconnect()) {
                break;
            }
            continue;
        }
        
        // Open output file
        if (!openOutput()) {
            QString err = "Failed to open output file";
            m_stats.lastError = err;
            emit error(m_config.cameraNumber, err);
            closeInput();
            
            if (!attemptReconnect()) {
                break;
            }
            continue;
        }
        
        // Reset segment timer
        m_segmentStart = QDateTime::currentMSecsSinceEpoch();
        m_firstDts = AV_NOPTS_VALUE;
        
        // Read and write packets
        AVPacket* packet = av_packet_alloc();
        int ret;
        
        while (!m_stopRequested.load() && !m_paused.load()) {
            ret = av_read_frame(m_inputCtx, packet);
            
            if (ret < 0) {
                if (ret == AVERROR_EOF) {
                    ErrorLogger::instance().logWarning("REC005", "Stream ended unexpectedly",
                        QString("Camera %1").arg(m_config.cameraNumber));
                } else {
                    char errBuf[256];
                    av_strerror(ret, errBuf, sizeof(errBuf));
                    ErrorLogger::instance().logWarning("REC006", 
                        QString("Read error: %1").arg(errBuf),
                        QString("Camera %1").arg(m_config.cameraNumber));
                }
                
                av_packet_unref(packet);
                break;
            }
            
            // Process the packet
            if (!processPacket(packet)) {
                av_packet_unref(packet);
                break;
            }
            
            // Check for segment rotation
            if (needsRotation()) {
                rotateSegment();
            }
            
            av_packet_unref(packet);
        }
        
        av_packet_free(&packet);
        
        closeOutput();
        closeInput();
        
        // Increment segment count on normal completion
        if (m_stopRequested.load()) {
            m_stats.segmentsCompleted++;
        }
    }
    
    m_active.store(false);
}

bool SingleRecorder::openInput() {
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0); // 5 second timeout
    av_dict_set(&options, "max_delay", "500000", 0);
    av_dict_set(&options, "reorder_queue_size", "0", 0);
    
    int ret = avformat_open_input(&m_inputCtx, 
                                   m_config.rtspUrl.toUtf8().constData(),
                                   nullptr, &options);
    av_dict_free(&options);
    
    if (ret != 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        ErrorLogger::instance().logError("REC007", 
            QString("Failed to open input: %1").arg(errBuf),
            m_config.rtspUrl);
        return false;
    }
    
    ret = avformat_find_stream_info(m_inputCtx, nullptr);
    if (ret < 0) {
        ErrorLogger::instance().logError("REC008", "Failed to find stream info",
            m_config.rtspUrl);
        avformat_close_input(&m_inputCtx);
        return false;
    }
    
    // Find video and audio streams
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    
    for (unsigned int i = 0; i < m_inputCtx->nb_streams; ++i) {
        if (m_inputCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            m_videoStreamIndex == -1) {
            m_videoStreamIndex = i;
        } else if (m_inputCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                   m_audioStreamIndex == -1) {
            m_audioStreamIndex = i;
        }
    }
    
    if (m_videoStreamIndex == -1) {
        ErrorLogger::instance().logError("REC009", "No video stream found",
            m_config.rtspUrl);
        avformat_close_input(&m_inputCtx);
        return false;
    }
    
    return true;
}

void SingleRecorder::closeInput() {
    if (m_inputCtx) {
        avformat_close_input(&m_inputCtx);
        m_inputCtx = nullptr;
    }
}

bool SingleRecorder::openOutput() {
    QString outputPath = generateOutputFilename();
    m_currentOutputFile = outputPath;
    
    // Determine output format
    QString formatName = m_config.containerFormat;
    if (formatName.isEmpty()) {
        formatName = RTSPProbe::getRecommendedContainer(m_config.videoCodec, 
                                                         m_config.audioCodec);
    }
    
    const AVOutputFormat* outputFormat = av_guess_format(
        formatName.toUtf8().constData(),
        outputPath.toUtf8().constData(),
        nullptr);
    
    if (!outputFormat) {
        ErrorLogger::instance().logError("REC010", 
            QString("Could not find output format: %1").arg(formatName));
        return false;
    }
    
    int ret = avformat_alloc_output_context2(&m_outputCtx, outputFormat,
                                              nullptr, outputPath.toUtf8().constData());
    if (ret < 0 || !m_outputCtx) {
        ErrorLogger::instance().logError("REC011", "Failed to allocate output context");
        return false;
    }
    
    // Add video stream
    if (m_videoStreamIndex >= 0) {
        AVStream* inStream = m_inputCtx->streams[m_videoStreamIndex];
        AVStream* outStream = avformat_new_stream(m_outputCtx, nullptr);
        if (!outStream) {
            ErrorLogger::instance().logError("REC012", "Failed to create video stream");
            avformat_free_context(m_outputCtx);
            m_outputCtx = nullptr;
            return false;
        }
        
        ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
        if (ret < 0) {
            ErrorLogger::instance().logError("REC013", "Failed to copy video parameters");
            avformat_free_context(m_outputCtx);
            m_outputCtx = nullptr;
            return false;
        }
        
        outStream->time_base = inStream->time_base;
        m_outputVideoIndex = outStream->index;
    }
    
    // Add audio stream if present
    if (m_audioStreamIndex >= 0) {
        AVStream* inStream = m_inputCtx->streams[m_audioStreamIndex];
        AVStream* outStream = avformat_new_stream(m_outputCtx, nullptr);
        if (!outStream) {
            ErrorLogger::instance().logWarning("REC014", "Failed to create audio stream, continuing without audio");
            m_outputAudioIndex = -1;
        } else {
            ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
            if (ret < 0) {
                ErrorLogger::instance().logWarning("REC015", "Failed to copy audio parameters, continuing without audio");
                m_outputAudioIndex = -1;
            } else {
                outStream->time_base = inStream->time_base;
                m_outputAudioIndex = outStream->index;
            }
        }
    }
    
    // Open output file
    if (!(m_outputCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_outputCtx->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errBuf[256];
            av_strerror(ret, errBuf, sizeof(errBuf));
            ErrorLogger::instance().logError("REC016", 
                QString("Failed to open output file: %1").arg(errBuf));
            avformat_free_context(m_outputCtx);
            m_outputCtx = nullptr;
            return false;
        }
    }
    
    // Write file header
    AVDictionary* muxerOpts = nullptr;
    av_dict_set(&muxerOpts, "movflags", "faststart", 0); // For MP4 fast start
    
    ret = avformat_write_header(m_outputCtx, &muxerOpts);
    av_dict_free(&muxerOpts);
    
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        ErrorLogger::instance().logError("REC017", 
            QString("Failed to write header: %1").arg(errBuf));
        if (!(m_outputCtx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_outputCtx->pb);
        }
        avformat_free_context(m_outputCtx);
        m_outputCtx = nullptr;
        return false;
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_stats.currentFile = outputPath;
        m_stats.isActive = true;
    }
    
    emit segmentStarted(m_config.cameraNumber, outputPath);
    
    return true;
}

void SingleRecorder::closeOutput() {
    if (m_outputCtx) {
        // Write trailer
        av_write_trailer(m_outputCtx);
        
        // Close file
        if (!(m_outputCtx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_outputCtx->pb);
        }
        
        avformat_free_context(m_outputCtx);
        m_outputCtx = nullptr;
    }
}

QString SingleRecorder::generateOutputFilename() {
    QDateTime now = QDateTime::currentDateTime();
    
    // Format: cam{numéro}_{JJ_MM_AA}h{HH}h{MM}m{SS}s
    QString fileName = QString("cam%1_%2_%3_%4_%5h%6m%7s.%8")
        .arg(m_config.cameraNumber, 2, 10, QChar('0'))
        .arg(now.date().day(), 2, 10, QChar('0'))
        .arg(now.date().month(), 2, 10, QChar('0'))
        .arg(now.date().year() % 100, 2, 10, QChar('0')) // Last 2 digits of year
        .arg(now.time().hour(), 2, 10, QChar('0'))
        .arg(now.time().minute(), 2, 10, QChar('0'))
        .arg(now.time().second(), 2, 10, QChar('0'))
        .arg(RTSPProbe::getFileExtension(m_config.containerFormat));
    
    QDir dir(m_config.outputDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    return dir.filePath(fileName);
}

bool SingleRecorder::needsRotation() const {
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_segmentStart;
    qint64 segmentDurationMs = m_config.segmentDurationMinutes * 60 * 1000;
    return elapsed >= segmentDurationMs;
}

void SingleRecorder::rotateSegment() {
    qint64 segmentDurationMs = QDateTime::currentMSecsSinceEpoch() - m_segmentStart;
    QString completedFile = m_currentOutputFile;
    
    // Close current output
    closeOutput();
    
    emit segmentCompleted(m_config.cameraNumber, completedFile, segmentDurationMs);
    
    {
        QMutexLocker locker(&m_mutex);
        m_stats.segmentsCompleted++;
    }
    
    // Open new output
    if (!openOutput()) {
        ErrorLogger::instance().logError("REC018", "Failed to open new segment after rotation");
    }
    
    m_segmentStart = QDateTime::currentMSecsSinceEpoch();
    m_firstDts = AV_NOPTS_VALUE;
    
    ErrorLogger::instance().logInfo("REC019", 
        QString("Rotated to new segment for camera %1").arg(m_config.cameraNumber));
}

bool SingleRecorder::processPacket(AVPacket* packet) {
    if (!m_outputCtx || !packet) {
        return false;
    }
    
    int streamIndex = packet->stream_index;
    AVStream* inStream = m_inputCtx->streams[streamIndex];
    AVStream* outStream = nullptr;
    
    // Map stream indices
    if (streamIndex == m_videoStreamIndex) {
        outStream = m_outputCtx->streams[m_outputVideoIndex];
    } else if (streamIndex == m_audioStreamIndex && m_outputAudioIndex >= 0) {
        outStream = m_outputCtx->streams[m_outputAudioIndex];
    } else {
        return true; // Skip unknown streams
    }
    
    // Rescale timestamps
    av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);
    packet->stream_index = outStream->index;
    
    // Handle timestamp discontinuities
    if (m_firstDts == AV_NOPTS_VALUE) {
        m_firstDts = packet->dts;
    }
    
    // Write the packet
    int ret = av_interleaved_write_frame(m_outputCtx, packet);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        ErrorLogger::instance().logWarning("REC020", 
            QString("Write error: %1").arg(errBuf));
        return false;
    }
    
    // Update statistics
    {
        QMutexLocker locker(&m_mutex);
        m_stats.bytesWritten += packet->size;
        m_stats.currentFileSize += packet->size;
    }
    
    return true;
}

bool SingleRecorder::attemptReconnect() {
    m_stats.retryCount++;
    
    if (m_stats.retryCount > m_config.maxRetries) {
        ErrorLogger::instance().logCritical("REC021", 
            QString("Max retries exceeded for camera %1").arg(m_config.cameraNumber));
        return false;
    }
    
    emit reconnecting(m_config.cameraNumber, m_stats.retryCount, m_config.maxRetries);
    
    ErrorLogger::instance().logWarning("REC022", 
        QString("Reconnection attempt %1/%2 for camera %3")
            .arg(m_stats.retryCount)
            .arg(m_config.maxRetries)
            .arg(m_config.cameraNumber));
    
    QThread::msleep(m_config.retryDelayMs);
    
    return true;
}

void SingleRecorder::updateStats() {
    QMutexLocker locker(&m_mutex);
    
    m_stats.recordingDurationMs = QDateTime::currentMSecsSinceEpoch() - m_startTime;
    
    // Calculate bitrate
    if (m_stats.recordingDurationMs > 0) {
        float durationSec = m_stats.recordingDurationMs / 1000.0f;
        m_stats.bitrate = (m_stats.bytesWritten * 8.0f / 1024.0f) / durationSec;
    }
    
    emit statsUpdate(m_stats);
}

// ============================================================================
// Recorder Implementation
// ============================================================================

Recorder::Recorder(QObject* parent)
    : QObject(parent)
    , m_segmentDurationMinutes(60)
    , m_statsTimer(new QTimer(this))
{
    m_recording.store(false);
    
    // Set default storage directory
#ifdef Q_OS_ANDROID
    m_storageDirectory = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/TELECLOUD";
#else
    m_storageDirectory = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/TELECLOUD";
#endif
    
    connect(m_statsTimer, &QTimer::timeout, this, &Recorder::onStatsTimer);
}

Recorder::~Recorder() {
    stopAll();
}

void Recorder::setStorageDirectory(const QString& dir) {
    if (m_storageDirectory != dir) {
        m_storageDirectory = dir;
        emit storageDirectoryChanged();
    }
}

void Recorder::setSegmentDuration(int minutes) {
    if (m_segmentDurationMinutes != minutes && minutes > 0) {
        m_segmentDurationMinutes = minutes;
        emit segmentDurationChanged();
    }
}

QMap<int, RecordingStats> Recorder::getAllStats() const {
    QMutexLocker locker(&m_mutex);
    QMap<int, RecordingStats> stats;
    for (auto it = m_recorders.constBegin(); it != m_recorders.constEnd(); ++it) {
        stats[it.key()] = it.value()->getStats();
    }
    return stats;
}

RecordingStats Recorder::getStats(int cameraNumber) const {
    QMutexLocker locker(&m_mutex);
    if (m_recorders.contains(cameraNumber)) {
        return m_recorders[cameraNumber]->getStats();
    }
    return RecordingStats();
}

void Recorder::startAll(const QList<RecordingConfig>& configs) {
    if (m_recording.load()) {
        ErrorLogger::instance().logWarning("REC023", "Recording already in progress");
        return;
    }
    
    // Ensure output directory exists
    QDir dir(m_storageDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    m_recording.store(true);
    emit recordingChanged();
    
    for (const RecordingConfig& config : configs) {
        startOne(config);
    }
    
    m_statsTimer->start(2000);
    
    ErrorLogger::instance().logInfo("REC024", 
        QString("Started recording %1 cameras").arg(configs.size()));
}

void Recorder::startOne(const RecordingConfig& config) {
    QMutexLocker locker(&m_mutex);
    
    if (m_recorders.contains(config.cameraNumber)) {
        ErrorLogger::instance().logWarning("REC025", 
            QString("Camera %1 already recording").arg(config.cameraNumber));
        return;
    }
    
    RecordingConfig modifiedConfig = config;
    modifiedConfig.outputDirectory = m_storageDirectory;
    modifiedConfig.segmentDurationMinutes = m_segmentDurationMinutes;
    
    SingleRecorder* recorder = new SingleRecorder(modifiedConfig, this);
    
    connect(recorder, &SingleRecorder::started, this, &Recorder::onRecorderStarted);
    connect(recorder, &SingleRecorder::stopped, this, &Recorder::onRecorderStopped);
    connect(recorder, &SingleRecorder::error, this, &Recorder::onRecorderError);
    connect(recorder, &SingleRecorder::segmentStarted, this, &Recorder::onSegmentStarted);
    connect(recorder, &SingleRecorder::segmentCompleted, this, &Recorder::onSegmentCompleted);
    connect(recorder, &SingleRecorder::statsUpdate, this, &Recorder::onStatsUpdate);
    
    m_recorders[config.cameraNumber] = recorder;
    recorder->start();
    
    emit activeCameraCountChanged();
}

void Recorder::stopAll() {
    QMutexLocker locker(&m_mutex);
    
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        it.value()->stop();
        it.value()->deleteLater();
    }
    
    m_recorders.clear();
    m_recording.store(false);
    m_statsTimer->stop();
    
    emit recordingChanged();
    emit activeCameraCountChanged();
    
    ErrorLogger::instance().logInfo("REC026", "All recordings stopped");
}

void Recorder::stopOne(int cameraNumber) {
    QMutexLocker locker(&m_mutex);
    
    if (m_recorders.contains(cameraNumber)) {
        m_recorders[cameraNumber]->stop();
        m_recorders[cameraNumber]->deleteLater();
        m_recorders.remove(cameraNumber);
        
        emit activeCameraCountChanged();
        
        if (m_recorders.isEmpty()) {
            m_recording.store(false);
            emit recordingChanged();
        }
    }
}

void Recorder::pauseAll() {
    QMutexLocker locker(&m_mutex);
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        it.value()->pause();
    }
}

void Recorder::resumeAll() {
    QMutexLocker locker(&m_mutex);
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        it.value()->resume();
    }
}

void Recorder::onRecorderStarted(int cameraNumber) {
    emit cameraStarted(cameraNumber);
    ErrorLogger::instance().logInfo("REC027", 
        QString("Camera %1 started recording").arg(cameraNumber));
}

void Recorder::onRecorderStopped(int cameraNumber) {
    emit cameraStopped(cameraNumber);
    ErrorLogger::instance().logInfo("REC028", 
        QString("Camera %1 stopped recording").arg(cameraNumber));
}

void Recorder::onRecorderError(int cameraNumber, const QString& errorMsg) {
    emit this->error(cameraNumber, errorMsg);
    ErrorLogger::instance().logError("REC029", errorMsg, 
        QString("Camera %1").arg(cameraNumber));
}

void Recorder::onSegmentStarted(int cameraNumber, const QString& filePath) {
    emit segmentStarted(cameraNumber, filePath);
}

void Recorder::onSegmentCompleted(int cameraNumber, const QString& filePath, qint64 durationMs) {
    emit segmentCompleted(cameraNumber, filePath, durationMs);
    ErrorLogger::instance().logInfo("REC030", 
        QString("Segment completed: %1, duration: %2s")
            .arg(filePath)
            .arg(durationMs / 1000));
}

void Recorder::onStatsUpdate(const RecordingStats& stats) {
    Q_UNUSED(stats);
    // Individual stats are collected in onStatsTimer
}

void Recorder::onStatsTimer() {
    emit statsUpdated(getAllStats());
}

} // namespace telecloud
