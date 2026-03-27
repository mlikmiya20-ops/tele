/**
 * @file dvrmanager.h
 * @brief DVR management and configuration handling
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 * 
 * This file implements DVR management functionality that:
 * - Loads and saves DVR configurations to JSON
 * - Manages camera settings per DVR
 * - Handles authentication credentials (with optional encryption)
 */

#ifndef DVRMANAGER_H
#define DVRMANAGER_H

#include <QObject>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QMutex>
#include <QMutexLocker>
#include <memory>
#include "networkscanner.h"
#include "rtspprobe.h"
#include "recorder.h"

namespace telecloud {

/**
 * @struct CameraConfig
 * @brief Configuration for a single camera
 */
struct CameraConfig {
    int cameraNumber;                   ///< Camera/channel number (1-based)
    QString rtspUrl;                    ///< RTSP URL for this camera
    bool enabled;                       ///< Whether this camera is enabled for recording
    VideoCodec detectedVideoCodec;      ///< Detected video codec
    AudioCodec detectedAudioCodec;      ///< Detected audio codec
    QString containerFormat;            ///< Output container format
    int width;                          ///< Video width
    int height;                         ///< Video height
    double frameRate;                   ///< Frame rate
    int bitRate;                        ///< Bit rate in bps
    
    /**
     * @brief Default constructor
     */
    CameraConfig()
        : cameraNumber(0)
        , enabled(true)
        , detectedVideoCodec(VideoCodec::Unknown)
        , detectedAudioCodec(AudioCodec::Unknown)
        , width(0)
        , height(0)
        , frameRate(0.0)
        , bitRate(0)
    {}
    
    /**
     * @brief Converts to JSON
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["cameraNumber"] = cameraNumber;
        obj["rtspUrl"] = rtspUrl;
        obj["enabled"] = enabled;
        obj["detectedVideoCodec"] = static_cast<int>(detectedVideoCodec);
        obj["detectedAudioCodec"] = static_cast<int>(detectedAudioCodec);
        obj["containerFormat"] = containerFormat;
        obj["width"] = width;
        obj["height"] = height;
        obj["frameRate"] = frameRate;
        obj["bitRate"] = bitRate;
        return obj;
    }
    
    /**
     * @brief Creates from JSON
     */
    static CameraConfig fromJson(const QJsonObject& obj) {
        CameraConfig config;
        config.cameraNumber = obj["cameraNumber"].toInt();
        config.rtspUrl = obj["rtspUrl"].toString();
        config.enabled = obj["enabled"].toBool(true);
        config.detectedVideoCodec = static_cast<VideoCodec>(obj["detectedVideoCodec"].toInt());
        config.detectedAudioCodec = static_cast<AudioCodec>(obj["detectedAudioCodec"].toInt());
        config.containerFormat = obj["containerFormat"].toString();
        config.width = obj["width"].toInt();
        config.height = obj["height"].toInt();
        config.frameRate = obj["frameRate"].toDouble();
        config.bitRate = obj["bitRate"].toInt();
        return config;
    }
};

/**
 * @struct DVRConfig
 * @brief Complete configuration for a DVR device
 */
struct DVRConfig {
    QString id;                         ///< Unique identifier
    QString name;                       ///< User-friendly name
    QString ipAddress;                  ///< IP address
    int port;                           ///< RTSP port
    DVRBrand brand;                     ///< DVR brand
    QString username;                   ///< Username
    QString password;                   ///< Password (encrypted in storage)
    bool saveCredentials;               ///< Whether to save credentials
    int channelCount;                   ///< Number of channels
    QMap<int, CameraConfig> cameras;    ///< Camera configurations
    
    /**
     * @brief Default constructor
     */
    DVRConfig()
        : port(554)
        , brand(DVRBrand::Unknown)
        , saveCredentials(false)
        , channelCount(0)
    {}
    
    /**
     * @brief Converts to JSON (password may be encrypted)
     */
    QJsonObject toJson(bool encryptPassword = false) const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["ipAddress"] = ipAddress;
        obj["port"] = port;
        obj["brand"] = static_cast<int>(brand);
        obj["username"] = username;
        
        if (encryptPassword && !password.isEmpty()) {
            obj["password"] = encryptString(password);
            obj["passwordEncrypted"] = true;
        } else {
            obj["password"] = password;
            obj["passwordEncrypted"] = false;
        }
        
        obj["saveCredentials"] = saveCredentials;
        obj["channelCount"] = channelCount;
        
        QJsonArray camerasArray;
        for (auto it = cameras.constBegin(); it != cameras.constEnd(); ++it) {
            camerasArray.append(it.value().toJson());
        }
        obj["cameras"] = camerasArray;
        
        return obj;
    }
    
    /**
     * @brief Creates from JSON
     */
    static DVRConfig fromJson(const QJsonObject& obj) {
        DVRConfig config;
        config.id = obj["id"].toString();
        config.name = obj["name"].toString();
        config.ipAddress = obj["ipAddress"].toString();
        config.port = obj["port"].toInt(554);
        config.brand = static_cast<DVRBrand>(obj["brand"].toInt());
        config.username = obj["username"].toString();
        
        QString pwd = obj["password"].toString();
        bool encrypted = obj["passwordEncrypted"].toBool(false);
        config.password = encrypted ? decryptString(pwd) : pwd;
        
        config.saveCredentials = obj["saveCredentials"].toBool();
        config.channelCount = obj["channelCount"].toInt();
        
        QJsonArray camerasArray = obj["cameras"].toArray();
        for (const auto& camVal : camerasArray) {
            CameraConfig cam = CameraConfig::fromJson(camVal.toObject());
            config.cameras[cam.cameraNumber] = cam;
        }
        
        return config;
    }
    
private:
    /**
     * @brief Simple encryption for password storage
     * @note This is basic obfuscation, not cryptographically secure
     */
    static QString encryptString(const QString& input) {
        QByteArray data = input.toUtf8();
        QByteArray key = QCryptographicHash::hash("TELECLOUD_KEY_2025", QCryptographicHash::Sha256);
        
        QByteArray result;
        for (int i = 0; i < data.size(); ++i) {
            result.append(data[i] ^ key[i % key.size()]);
        }
        
        return result.toBase64();
    }
    
    /**
     * @brief Decrypt string encrypted with encryptString
     */
    static QString decryptString(const QString& input) {
        QByteArray data = QByteArray::fromBase64(input.toUtf8());
        QByteArray key = QCryptographicHash::hash("TELECLOUD_KEY_2025", QCryptographicHash::Sha256);
        
        QByteArray result;
        for (int i = 0; i < data.size(); ++i) {
            result.append(data[i] ^ key[i % key.size()]);
        }
        
        return QString::fromUtf8(result);
    }
};

/**
 * @struct AppConfig
 * @brief Global application configuration
 */
struct AppConfig {
    int fps;                            ///< Target FPS for display
    int segmentDurationMinutes;         ///< Recording segment duration
    QString storageDirectory;           ///< Output directory
    bool autoStartRecording;            ///< Auto-start on launch
    bool minimizeToTray;                ///< Minimize to system tray (desktop)
    int maxConcurrentRecordings;        ///< Maximum concurrent recordings
    
    /**
     * @brief Default constructor with sensible defaults
     */
    AppConfig()
        : fps(12)
        , segmentDurationMinutes(60)
        , autoStartRecording(false)
        , minimizeToTray(true)
        , maxConcurrentRecordings(64)
    {
#ifdef Q_OS_ANDROID
        storageDirectory = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/TELECLOUD";
#else
        storageDirectory = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/TELECLOUD";
#endif
    }
    
    /**
     * @brief Converts to JSON
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["fps"] = fps;
        obj["segmentDurationMinutes"] = segmentDurationMinutes;
        obj["storageDirectory"] = storageDirectory;
        obj["autoStartRecording"] = autoStartRecording;
        obj["minimizeToTray"] = minimizeToTray;
        obj["maxConcurrentRecordings"] = maxConcurrentRecordings;
        return obj;
    }
    
    /**
     * @brief Creates from JSON
     */
    static AppConfig fromJson(const QJsonObject& obj) {
        AppConfig config;
        config.fps = obj["fps"].toInt(12);
        config.segmentDurationMinutes = obj["segmentDurationMinutes"].toInt(60);
        config.storageDirectory = obj["storageDirectory"].toString();
        config.autoStartRecording = obj["autoStartRecording"].toBool();
        config.minimizeToTray = obj["minimizeToTray"].toBool(true);
        config.maxConcurrentRecordings = obj["maxConcurrentRecordings"].toInt(64);
        return config;
    }
};

/**
 * @class DVRManager
 * @brief Manages DVR configurations and camera settings
 * 
 * DVRManager is the central configuration manager that handles:
 * - Loading and saving DVR configurations to JSON files
 * - Managing camera settings per DVR
 * - Credential storage with optional encryption
 * - Application-wide settings
 * 
 * The configuration is stored in dvr.json with the following structure:
 * - Global application settings
 * - List of configured DVRs
 * - Camera settings for each DVR
 * 
 * Usage:
 * @code
 * DVRManager manager;
 * manager.loadConfiguration();
 * 
 * // Access configuration
 * DVRConfig dvr = manager.getDVR("dvr-001");
 * 
 * // Modify and save
 * manager.updateDVR(dvr);
 * manager.saveConfiguration();
 * @endcode
 */
class DVRManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString configFilePath READ configFilePath WRITE setConfigFilePath NOTIFY configFilePathChanged)
    Q_PROPERTY(AppConfig appConfig READ appConfig WRITE setAppConfig NOTIFY appConfigChanged)
    Q_PROPERTY(int dvrCount READ dvrCount NOTIFY dvrCountChanged)

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit DVRManager(QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~DVRManager() override = default;
    
    /**
     * @brief Gets the configuration file path
     * @return Path to dvr.json
     */
    QString configFilePath() const { return m_configFilePath; }
    
    /**
     * @brief Sets the configuration file path
     * @param path New file path
     */
    void setConfigFilePath(const QString& path);
    
    /**
     * @brief Gets the application configuration
     * @return Application settings
     */
    AppConfig appConfig() const { return m_appConfig; }
    
    /**
     * @brief Sets the application configuration
     * @param config New application settings
     */
    void setAppConfig(const AppConfig& config);
    
    /**
     * @brief Gets the number of configured DVRs
     * @return DVR count
     */
    int dvrCount() const { return m_dvrs.size(); }
    
    /**
     * @brief Gets all configured DVRs
     * @return Map of DVR ID to configuration
     */
    QMap<QString, DVRConfig> getAllDVRs() const { return m_dvrs; }
    
    /**
     * @brief Gets a specific DVR configuration
     * @param id DVR identifier
     * @return DVR configuration, or empty if not found
     */
    DVRConfig getDVR(const QString& id) const { return m_dvrs.value(id); }
    
    /**
     * @brief Gets camera configuration
     * @param dvrId DVR identifier
     * @param cameraNumber Camera number
     * @return Camera configuration
     */
    CameraConfig getCamera(const QString& dvrId, int cameraNumber) const;
    
    /**
     * @brief Gets all cameras for a DVR
     * @param dvrId DVR identifier
     * @return Map of camera number to configuration
     */
    QMap<int, CameraConfig> getCameras(const QString& dvrId) const;
    
    /**
     * @brief Checks if configuration is loaded
     * @return true if loaded
     */
    bool isLoaded() const { return m_loaded; }
    
    /**
     * @brief Generates recording configurations for all enabled cameras
     * @return List of recording configurations
     */
    QList<RecordingConfig> generateRecordingConfigs() const;

public slots:
    /**
     * @brief Loads configuration from file
     * @return true if successful
     */
    bool loadConfiguration();
    
    /**
     * @brief Saves configuration to file
     * @return true if successful
     */
    bool saveConfiguration();
    
    /**
     * @brief Adds or updates a DVR configuration
     * @param dvr DVR configuration
     */
    void updateDVR(const DVRConfig& dvr);
    
    /**
     * @brief Removes a DVR configuration
     * @param id DVR identifier
     */
    void removeDVR(const QString& id);
    
    /**
     * @brief Updates camera configuration
     * @param dvrId DVR identifier
     * @param camera Camera configuration
     */
    void updateCamera(const QString& dvrId, const CameraConfig& camera);
    
    /**
     * @brief Enables or disables a camera
     * @param dvrId DVR identifier
     * @param cameraNumber Camera number
     * @param enabled Enable state
     */
    void setCameraEnabled(const QString& dvrId, int cameraNumber, bool enabled);
    
    /**
     * @brief Imports configuration from discovered DVR
     * @param info DVR information from network scan
     * @param username DVR username
     * @param password DVR password
     * @param probeChannels Whether to probe for channels
     * @return Created DVR configuration
     */
    DVRConfig importFromDiscovery(const DVRInfo& info, 
                                   const QString& username = QString(),
                                   const QString& password = QString(),
                                   bool probeChannels = true);
    
    /**
     * @brief Clears all configurations
     */
    void clearAll();
    
    /**
     * @brief Validates the current configuration
     * @return List of validation errors
     */
    QStringList validate() const;

signals:
    /**
     * @brief Emitted when configuration file path changes
     */
    void configFilePathChanged();
    
    /**
     * @brief Emitted when application configuration changes
     */
    void appConfigChanged();
    
    /**
     * @brief Emitted when DVR count changes
     */
    void dvrCountChanged();
    
    /**
     * @brief Emitted when a DVR is added
     * @param id DVR identifier
     */
    void dvrAdded(const QString& id);
    
    /**
     * @brief Emitted when a DVR is updated
     * @param id DVR identifier
     */
    void dvrUpdated(const QString& id);
    
    /**
     * @brief Emitted when a DVR is removed
     * @param id DVR identifier
     */
    void dvrRemoved(const QString& id);
    
    /**
     * @brief Emitted when configuration is loaded
     */
    void configurationLoaded();
    
    /**
     * @brief Emitted when configuration is saved
     */
    void configurationSaved();
    
    /**
     * @brief Emitted on validation error
     * @param errors List of error messages
     */
    void validationError(const QStringList& errors);

private:
    /**
     * @brief Generates a unique DVR ID
     * @return Unique identifier string
     */
    QString generateDVRId();
    
    /**
     * @brief Sets default configuration file path based on platform
     */
    void setDefaultConfigPath();
    
    QString m_configFilePath;           ///< Path to dvr.json
    AppConfig m_appConfig;              ///< Application configuration
    QMap<QString, DVRConfig> m_dvrs;    ///< DVR configurations by ID
    bool m_loaded;                      ///< Whether configuration is loaded
    mutable QMutex m_mutex;             ///< Thread safety mutex
};

} // namespace telecloud

#endif // DVRMANAGER_H
