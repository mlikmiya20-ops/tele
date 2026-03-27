/**
 * @file dvrmanager.cpp
 * @brief Implementation of DVRManager class
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 */

#include "dvrmanager.h"
#include "errorlogger.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>

namespace telecloud {

// ============================================================================
// DVRManager Implementation
// ============================================================================

DVRManager::DVRManager(QObject* parent)
    : QObject(parent)
    , m_loaded(false)
{
    setDefaultConfigPath();
}

void DVRManager::setConfigFilePath(const QString& path) {
    if (m_configFilePath != path) {
        m_configFilePath = path;
        emit configFilePathChanged();
    }
}

void DVRManager::setAppConfig(const AppConfig& config) {
    if (m_appConfig.fps != config.fps ||
        m_appConfig.segmentDurationMinutes != config.segmentDurationMinutes ||
        m_appConfig.storageDirectory != config.storageDirectory) {
        
        m_appConfig = config;
        emit appConfigChanged();
    }
}

CameraConfig DVRManager::getCamera(const QString& dvrId, int cameraNumber) const {
    if (m_dvrs.contains(dvrId)) {
        return m_dvrs[dvrId].cameras.value(cameraNumber);
    }
    return CameraConfig();
}

QMap<int, CameraConfig> DVRManager::getCameras(const QString& dvrId) const {
    if (m_dvrs.contains(dvrId)) {
        return m_dvrs[dvrId].cameras;
    }
    return QMap<int, CameraConfig>();
}

QList<RecordingConfig> DVRManager::generateRecordingConfigs() const {
    QList<RecordingConfig> configs;
    
    for (auto dvrIt = m_dvrs.constBegin(); dvrIt != m_dvrs.constEnd(); ++dvrIt) {
        const DVRConfig& dvr = dvrIt.value();
        
        for (auto camIt = dvr.cameras.constBegin(); camIt != dvr.cameras.constEnd(); ++camIt) {
            const CameraConfig& cam = camIt.value();
            
            if (cam.enabled && !cam.rtspUrl.isEmpty()) {
                RecordingConfig recConfig;
                recConfig.rtspUrl = cam.rtspUrl;
                recConfig.cameraNumber = cam.cameraNumber;
                recConfig.outputDirectory = m_appConfig.storageDirectory;
                recConfig.segmentDurationMinutes = m_appConfig.segmentDurationMinutes;
                recConfig.videoCodec = cam.detectedVideoCodec;
                recConfig.audioCodec = cam.detectedAudioCodec;
                recConfig.containerFormat = cam.containerFormat;
                
                configs.append(recConfig);
            }
        }
    }
    
    return configs;
}

bool DVRManager::loadConfiguration() {
    QMutexLocker locker(&m_mutex);
    
    QFile file(m_configFilePath);
    
    if (!file.exists()) {
        ErrorLogger::instance().logInfo("DVR001", 
            "Configuration file not found, creating default");
        m_loaded = true;
        return saveConfiguration();
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorLogger::instance().logError("DVR002", 
            QString("Cannot open configuration file: %1").arg(file.errorString()));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        ErrorLogger::instance().logError("DVR003", 
            QString("JSON parse error: %1").arg(parseError.errorString()));
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Load application configuration
    if (root.contains("appConfig")) {
        m_appConfig = AppConfig::fromJson(root["appConfig"].toObject());
    }
    
    // Load DVR configurations
    m_dvrs.clear();
    if (root.contains("dvrs")) {
        QJsonArray dvrsArray = root["dvrs"].toArray();
        for (const auto& dvrVal : dvrsArray) {
            DVRConfig dvr = DVRConfig::fromJson(dvrVal.toObject());
            m_dvrs[dvr.id] = dvr;
        }
    }
    
    m_loaded = true;
    emit configurationLoaded();
    emit dvrCountChanged();
    
    ErrorLogger::instance().logInfo("DVR004", 
        QString("Configuration loaded: %1 DVRs").arg(m_dvrs.size()));
    
    return true;
}

bool DVRManager::saveConfiguration() {
    QMutexLocker locker(&m_mutex);
    
    // Ensure directory exists
    QFileInfo fileInfo(m_configFilePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QJsonObject root;
    
    // Save application configuration
    root["appConfig"] = m_appConfig.toJson();
    
    // Save DVR configurations (with encrypted passwords)
    QJsonArray dvrsArray;
    for (auto it = m_dvrs.constBegin(); it != m_dvrs.constEnd(); ++it) {
        dvrsArray.append(it.value().toJson(true)); // Encrypt passwords
    }
    root["dvrs"] = dvrsArray;
    
    // Metadata
    root["version"] = "1.00";
    root["lastSaved"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    
    QJsonDocument doc(root);
    
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        ErrorLogger::instance().logError("DVR005", 
            QString("Cannot write configuration file: %1").arg(file.errorString()));
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    emit configurationSaved();
    
    ErrorLogger::instance().logInfo("DVR006", "Configuration saved");
    
    return true;
}

void DVRManager::updateDVR(const DVRConfig& dvr) {
    QMutexLocker locker(&m_mutex);
    
    bool isNew = !m_dvrs.contains(dvr.id);
    m_dvrs[dvr.id] = dvr;
    
    if (isNew) {
        emit dvrAdded(dvr.id);
    } else {
        emit dvrUpdated(dvr.id);
    }
    
    emit dvrCountChanged();
}

void DVRManager::removeDVR(const QString& id) {
    QMutexLocker locker(&m_mutex);
    
    if (m_dvrs.remove(id) > 0) {
        emit dvrRemoved(id);
        emit dvrCountChanged();
    }
}

void DVRManager::updateCamera(const QString& dvrId, const CameraConfig& camera) {
    QMutexLocker locker(&m_mutex);
    
    if (m_dvrs.contains(dvrId)) {
        m_dvrs[dvrId].cameras[camera.cameraNumber] = camera;
        m_dvrs[dvrId].channelCount = m_dvrs[dvrId].cameras.size();
        emit dvrUpdated(dvrId);
    }
}

void DVRManager::setCameraEnabled(const QString& dvrId, int cameraNumber, bool enabled) {
    QMutexLocker locker(&m_mutex);
    
    if (m_dvrs.contains(dvrId) && m_dvrs[dvrId].cameras.contains(cameraNumber)) {
        m_dvrs[dvrId].cameras[cameraNumber].enabled = enabled;
        emit dvrUpdated(dvrId);
    }
}

DVRConfig DVRManager::importFromDiscovery(const DVRInfo& info,
                                           const QString& username,
                                           const QString& password,
                                           bool probeChannels) {
    DVRConfig dvr;
    dvr.id = generateDVRId();
    dvr.name = QString("%1 (%2)").arg(brandToString(info.brand)).arg(info.ipAddress);
    dvr.ipAddress = info.ipAddress;
    dvr.port = info.port;
    dvr.brand = info.brand;
    dvr.username = username;
    dvr.password = password;
    dvr.saveCredentials = !password.isEmpty();
    
    if (probeChannels && !username.isEmpty()) {
        // Probe for channels
        RTSPProbe probe;
        probe.setTimeout(3000);
        
        QMap<int, RTSPProbeResult> channels = probe.probeChannels(
            info.ipAddress, info.port, username, password, info.brand, 16);
        
        for (auto it = channels.constBegin(); it != channels.constEnd(); ++it) {
            const RTSPProbeResult& result = it.value();
            
            CameraConfig cam;
            cam.cameraNumber = it.key();
            cam.rtspUrl = result.rtspUrl;
            cam.enabled = true;
            cam.detectedVideoCodec = result.streamInfo.videoCodec;
            cam.detectedAudioCodec = result.streamInfo.audioCodec;
            cam.containerFormat = RTSPProbe::getRecommendedContainer(
                result.streamInfo.videoCodec, result.streamInfo.audioCodec);
            cam.width = result.streamInfo.resolution.width();
            cam.height = result.streamInfo.resolution.height();
            cam.frameRate = result.streamInfo.getFrameRate();
            cam.bitRate = result.streamInfo.bitRate;
            
            dvr.cameras[cam.cameraNumber] = cam;
        }
        
        dvr.channelCount = dvr.cameras.size();
    }
    
    updateDVR(dvr);
    
    ErrorLogger::instance().logInfo("DVR007", 
        QString("Imported DVR: %1 with %2 channels").arg(dvr.name).arg(dvr.channelCount));
    
    return dvr;
}

void DVRManager::clearAll() {
    QMutexLocker locker(&m_mutex);
    
    m_dvrs.clear();
    m_appConfig = AppConfig();
    
    emit dvrCountChanged();
    
    ErrorLogger::instance().logInfo("DVR008", "All configurations cleared");
}

QStringList DVRManager::validate() const {
    QStringList errors;
    
    if (m_appConfig.segmentDurationMinutes < 1) {
        errors << "Segment duration must be at least 1 minute";
    }
    
    if (m_appConfig.fps < 1 || m_appConfig.fps > 60) {
        errors << "FPS must be between 1 and 60";
    }
    
    if (m_appConfig.storageDirectory.isEmpty()) {
        errors << "Storage directory is not set";
    }
    
    for (auto dvrIt = m_dvrs.constBegin(); dvrIt != m_dvrs.constEnd(); ++dvrIt) {
        const DVRConfig& dvr = dvrIt.value();
        
        if (dvr.ipAddress.isEmpty()) {
            errors << QString("DVR %1 has no IP address").arg(dvr.name);
        }
        
        if (dvr.cameras.isEmpty()) {
            errors << QString("DVR %1 has no configured cameras").arg(dvr.name);
        }
        
        for (auto camIt = dvr.cameras.constBegin(); camIt != dvr.cameras.constEnd(); ++camIt) {
            const CameraConfig& cam = camIt.value();
            
            if (cam.enabled && cam.rtspUrl.isEmpty()) {
                errors << QString("Camera %1 on DVR %2 is enabled but has no RTSP URL")
                          .arg(cam.cameraNumber).arg(dvr.name);
            }
        }
    }
    
    if (!errors.isEmpty()) {
        const_cast<DVRManager*>(this)->emit validationError(errors);
    }
    
    return errors;
}

QString DVRManager::generateDVRId() {
    return QString("dvr-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

void DVRManager::setDefaultConfigPath() {
#ifdef Q_OS_ANDROID
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
#endif
    
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    m_configFilePath = configDir + "/dvr.json";
}

} // namespace telecloud
