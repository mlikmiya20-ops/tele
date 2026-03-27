/**
 * @file networkscanner.h
 * @brief Asynchronous network scanner for DVR detection on local network
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 * 
 * This file implements an asynchronous network scanner that:
 * - Scans IP range 192.168.0.1 to 192.168.255.254 for DVR devices
 * - Detects DVR brands via RTSP probing on port 554
 * - Supports multi-threaded scanning for performance
 */

#ifndef NETWORKSCANNER_H
#define NETWORKSCANNER_H

#include <QObject>
#include <QThread>
#include <QThreadPool>
#include <QRunnable>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTimer>
#include <QMap>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <atomic>
#include <memory>

namespace telecloud {

/**
 * @enum DVRBrand
 * @brief Enumeration of supported DVR brands
 */
enum class DVRBrand {
    Unknown = 0,
    Hikvision,
    Dahua,
    Axis,
    Bosch,
    Reolink,
    TPLinkTapo,
    Uniview,
    Sony,
    Samsung,
    Panasonic,
    Vivotek,
    Honeywell,
    Hanwha,
    Generic
};

/**
 * @brief Converts DVRBrand enum to human-readable string
 * @param brand The brand enum value
 * @return QString representation of the brand
 */
inline QString brandToString(DVRBrand brand) {
    switch (brand) {
        case DVRBrand::Hikvision: return "Hikvision";
        case DVRBrand::Dahua: return "Dahua";
        case DVRBrand::Axis: return "Axis";
        case DVRBrand::Bosch: return "Bosch";
        case DVRBrand::Reolink: return "Reolink";
        case DVRBrand::TPLinkTapo: return "TP-Link/Tapo";
        case DVRBrand::Uniview: return "Uniview";
        case DVRBrand::Sony: return "Sony";
        case DVRBrand::Samsung: return "Samsung";
        case DVRBrand::Panasonic: return "Panasonic";
        case DVRBrand::Vivotek: return "Vivotek";
        case DVRBrand::Honeywell: return "Honeywell";
        case DVRBrand::Hanwha: return "Hanwha";
        case DVRBrand::Generic: return "Generic DVR";
        default: return "Unknown";
    }
}

/**
 * @struct DVRInfo
 * @brief Information about a detected DVR device
 */
struct DVRInfo {
    QString ipAddress;          ///< IP address of the DVR
    int port;                   ///< RTSP port (default 554)
    DVRBrand brand;             ///< Detected brand
    QString brandName;          ///< Human-readable brand name
    QString rtspPath;           ///< Detected RTSP path pattern
    int channelCount;           ///< Number of detected channels/cameras
    bool requiresAuth;          ///< Whether authentication is required
    QString firmware;           ///< Firmware version if detected
    QString model;              ///< Model name if detected
    
    /**
     * @brief Converts DVRInfo to JSON object
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["ipAddress"] = ipAddress;
        obj["port"] = port;
        obj["brand"] = static_cast<int>(brand);
        obj["brandName"] = brandName;
        obj["rtspPath"] = rtspPath;
        obj["channelCount"] = channelCount;
        obj["requiresAuth"] = requiresAuth;
        obj["firmware"] = firmware;
        obj["model"] = model;
        return obj;
    }
    
    /**
     * @brief Creates DVRInfo from JSON object
     */
    static DVRInfo fromJson(const QJsonObject& obj) {
        DVRInfo info;
        info.ipAddress = obj["ipAddress"].toString();
        info.port = obj["port"].toInt(554);
        info.brand = static_cast<DVRBrand>(obj["brand"].toInt());
        info.brandName = obj["brandName"].toString();
        info.rtspPath = obj["rtspPath"].toString();
        info.channelCount = obj["channelCount"].toInt();
        info.requiresAuth = obj["requiresAuth"].toBool();
        info.firmware = obj["firmware"].toString();
        info.model = obj["model"].toString();
        return info;
    }
};

/**
 * @class ScanTask
 * @brief QRunnable task for scanning a single IP address
 */
class ScanTask : public QObject, public QRunnable {
    Q_OBJECT

public:
    /**
     * @brief Constructor for scan task
     * @param ip The IP address to scan
     * @param port The port to check (default 554)
     * @param timeoutMs Connection timeout in milliseconds
     */
    ScanTask(const QString& ip, int port = 554, int timeoutMs = 2000);
    
    /**
     * @brief Main execution method called by thread pool
     */
    void run() override;
    
    /**
     * @brief Attempts brand detection via RTSP OPTIONS request
     * @param socket Connected TCP socket
     * @return Detected DVR brand
     */
    DVRBrand detectBrand(QTcpSocket& socket);
    
    /**
     * @brief Parses RTSP response headers for brand identification
     * @param response Raw RTSP response data
     * @return Detected DVR brand
     */
    DVRBrand parseRTSPResponse(const QByteArray& response);
    
    /**
     * @brief Attempts HTTP-based brand detection
     * @return Detected DVR brand
     */
    DVRBrand detectBrandViaHTTP();

signals:
    /**
     * @brief Emitted when a DVR is detected
     * @param info Information about the detected DVR
     */
    void dvrDetected(const DVRInfo& info);
    
    /**
     * @brief Emitted when progress is updated
     * @param current Current IP being scanned
     */
    void progressUpdate(const QString& current);

private:
    QString m_ip;
    int m_port;
    int m_timeout;
    std::atomic<bool> m_cancelled{false};
};

/**
 * @class NetworkScanner
 * @brief Asynchronous network scanner for DVR detection
 * 
 * NetworkScanner performs asynchronous scanning of the local network
 * to detect DVR devices. It uses a thread pool for concurrent scanning
 * and emits signals when DVRs are discovered.
 * 
 * Usage:
 * @code
 * NetworkScanner scanner;
 * connect(&scanner, &NetworkScanner::dvrFound, this, &MyClass::onDVRFound);
 * scanner.startScan();
 * @endcode
 */
class NetworkScanner : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int totalHosts READ totalHosts CONSTANT)

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit NetworkScanner(QObject* parent = nullptr);
    
    /**
     * @brief Destructor - stops any running scan
     */
    ~NetworkScanner();
    
    /**
     * @brief Checks if a scan is currently in progress
     * @return true if scanning, false otherwise
     */
    bool isScanning() const { return m_scanning.load(); }
    
    /**
     * @brief Gets the current scan progress percentage
     * @return Progress from 0 to 100
     */
    int progress() const { return m_progress.load(); }
    
    /**
     * @brief Gets the total number of hosts to scan
     * @return Total host count
     */
    int totalHosts() const { return m_totalHosts; }
    
    /**
     * @brief Gets all discovered DVRs
     * @return List of discovered DVR information
     */
    QList<DVRInfo> discoveredDVRs() const { return m_discoveredDVRs; }
    
    /**
     * @brief Gets the local network subnet for scanning
     * @return List of IP prefixes to scan (e.g., "192.168.0", "192.168.1")
     */
    QStringList getLocalSubnets();
    
    /**
     * @brief Generates list of IPs to scan based on detected subnets
     * @return List of IP addresses to scan
     */
    QStringList generateScanList();
    
public slots:
    /**
     * @brief Starts the network scan
     * @param customRange Optional custom IP range (e.g., "192.168.1.1-192.168.1.254")
     * 
     * If no custom range is provided, automatically detects local subnets
     * and scans all hosts from 192.168.0.1 to 192.168.255.254.
     */
    void startScan(const QString& customRange = QString());
    
    /**
     * @brief Stops the current scan operation
     */
    void stopScan();
    
    /**
     * @brief Clears the list of discovered DVRs
     */
    void clearDiscovered() {
        m_discoveredDVRs.clear();
        emit discoveredChanged();
    }

signals:
    /**
     * @brief Emitted when a DVR is found
     * @param info Information about the discovered DVR
     */
    void dvrFound(const DVRInfo& info);
    
    /**
     * @brief Emitted when scan progress updates
     * @param current Number of hosts scanned
     * @param total Total number of hosts to scan
     */
    void scanProgress(int current, int total);
    
    /**
     * @brief Emitted when scanning state changes
     */
    void scanningChanged();
    
    /**
     * @brief Emitted when progress percentage changes
     */
    void progressChanged();
    
    /**
     * @brief Emitted when scan completes
     * @param dvrCount Number of DVRs discovered
     */
    void scanCompleted(int dvrCount);
    
    /**
     * @brief Emitted when discovered DVR list changes
     */
    void discoveredChanged();

private slots:
    /**
     * @brief Handles DVR detection from scan task
     */
    void onDVRDetected(const DVRInfo& info);
    
    /**
     * @brief Updates progress counter
     */
    void onTaskCompleted();

private:
    /**
     * @brief Parses IP range string to generate IP list
     * @param range Range string (e.g., "192.168.1.1-192.168.1.254")
     * @return List of IP addresses
     */
    QStringList parseRange(const QString& range);
    
    QThreadPool* m_threadPool;                  ///< Thread pool for concurrent scanning
    std::atomic<bool> m_scanning{false};        ///< Scanning state flag
    std::atomic<int> m_progress{0};             ///< Current progress
    std::atomic<int> m_scannedCount{0};         ///< Number of hosts scanned
    int m_totalHosts{0};                        ///< Total hosts to scan
    QList<DVRInfo> m_discoveredDVRs;            ///< List of discovered DVRs
    QString m_currentIP;                        ///< Current IP being scanned
};

} // namespace telecloud

#endif // NETWORKSCANNER_H
