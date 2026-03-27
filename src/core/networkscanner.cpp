/**
 * @file networkscanner.cpp
 * @brief Implementation of NetworkScanner and ScanTask classes
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 */

#include "networkscanner.h"
#include "errorlogger.h"
#include <QDebug>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

namespace telecloud {

// ============================================================================
// ScanTask Implementation
// ============================================================================

ScanTask::ScanTask(const QString& ip, int port, int timeoutMs)
    : QObject(nullptr)
    , m_ip(ip)
    , m_port(port)
    , m_timeout(timeoutMs)
{
    setAutoDelete(true);
}

void ScanTask::run() {
    if (m_cancelled.load()) return;
    
    emit progressUpdate(m_ip);
    
    QTcpSocket socket;
    socket.connectToHost(m_ip, m_port);
    
    if (!socket.waitForConnected(m_timeout)) {
        return; // Host not reachable on this port
    }
    
    // Connected - attempt brand detection
    DVRBrand brand = detectBrand(socket);
    
    if (brand != DVRBrand::Unknown) {
        DVRInfo info;
        info.ipAddress = m_ip;
        info.port = m_port;
        info.brand = brand;
        info.brandName = brandToString(brand);
        info.channelCount = 0; // Will be detected separately
        info.requiresAuth = true;
        
        emit dvrDetected(info);
    } else if (socket.state() == QAbstractSocket::ConnectedState) {
        // Port 554 is open but brand unknown - still report as generic DVR
        DVRInfo info;
        info.ipAddress = m_ip;
        info.port = m_port;
        info.brand = DVRBrand::Generic;
        info.brandName = brandToString(DVRBrand::Generic);
        info.channelCount = 0;
        info.requiresAuth = true;
        
        emit dvrDetected(info);
    }
    
    socket.disconnectFromHost();
}

DVRBrand ScanTask::detectBrand(QTcpSocket& socket) {
    // Send RTSP OPTIONS request
    QString optionsRequest = QString(
        "OPTIONS rtsp://%1:%2 RTSP/1.0\r\n"
        "CSeq: 1\r\n"
        "User-Agent: TELECLOUD-MULTI/1.00\r\n"
        "\r\n"
    ).arg(m_ip).arg(m_port);
    
    socket.write(optionsRequest.toUtf8());
    if (!socket.waitForBytesWritten(m_timeout)) {
        return DVRBrand::Unknown;
    }
    
    if (!socket.waitForReadyRead(m_timeout)) {
        return DVRBrand::Unknown;
    }
    
    QByteArray response = socket.readAll();
    return parseRTSPResponse(response);
}

DVRBrand ScanTask::parseRTSPResponse(const QByteArray& response) {
    QString resp = QString::fromUtf8(response).toLower();
    
    // Brand detection patterns based on Server header and RTSP path patterns
    
    // Hikvision detection
    if (resp.contains("hikvision") || 
        resp.contains("hipserver") ||
        resp.contains("dvr") && resp.contains("hik")) {
        return DVRBrand::Hikvision;
    }
    
    // Dahua detection
    if (resp.contains("dahua") || 
        resp.contains("lprd") ||
        resp.contains("dvr") && resp.contains("dah")) {
        return DVRBrand::Dahua;
    }
    
    // Axis detection
    if (resp.contains("axis") || 
        resp.contains("axiscam")) {
        return DVRBrand::Axis;
    }
    
    // Bosch detection
    if (resp.contains("bosch") || 
        resp.contains("gevision")) {
        return DVRBrand::Bosch;
    }
    
    // Reolink detection
    if (resp.contains("reolink") || 
        resp.contains("rlink")) {
        return DVRBrand::Reolink;
    }
    
    // TP-Link/Tapo detection
    if (resp.contains("tp-link") || 
        resp.contains("tapo") ||
        resp.contains("tplink")) {
        return DVRBrand::TPLinkTapo;
    }
    
    // Uniview detection
    if (resp.contains("uniview") || 
        resp.contains("unv")) {
        return DVRBrand::Uniview;
    }
    
    // Sony detection
    if (resp.contains("sony") || 
        resp.contains("snc-")) {
        return DVRBrand::Sony;
    }
    
    // Samsung detection
    if (resp.contains("samsung") || 
        resp.contains("samsungtechwin") ||
        resp.contains("hik") == false && resp.contains("samsung")) {
        return DVRBrand::Samsung;
    }
    
    // Panasonic detection
    if (resp.contains("panasonic") || 
        resp.contains("wv-")) {
        return DVRBrand::Panasonic;
    }
    
    // Vivotek detection
    if (resp.contains("vivotek") || 
        resp.contains("network camera")) {
        return DVRBrand::Vivotek;
    }
    
    // Honeywell detection
    if (resp.contains("honeywell") || 
        resp.contains("performance")) {
        return DVRBrand::Honeywell;
    }
    
    // Hanwha (formerly Samsung Techwin) detection
    if (resp.contains("hanwha") || 
        resp.contains("wisenet")) {
        return DVRBrand::Hanwha;
    }
    
    // Check for generic RTSP server
    if (resp.contains("rtsp/1.0") && 
        (resp.contains("200 ok") || resp.contains("200"))) {
        // RTSP server found but brand unknown
        return DVRBrand::Generic;
    }
    
    return DVRBrand::Unknown;
}

DVRBrand ScanTask::detectBrandViaHTTP() {
    // Try HTTP detection on port 80
    QTcpSocket httpSocket;
    httpSocket.connectToHost(m_ip, 80);
    
    if (!httpSocket.waitForConnected(m_timeout)) {
        return DVRBrand::Unknown;
    }
    
    QString httpRequest = QString(
        "GET / HTTP/1.0\r\n"
        "Host: %1\r\n"
        "User-Agent: TELECLOUD-MULTI/1.00\r\n"
        "\r\n"
    ).arg(m_ip);
    
    httpSocket.write(httpRequest.toUtf8());
    if (!httpSocket.waitForBytesWritten(m_timeout)) {
        return DVRBrand::Unknown;
    }
    
    if (!httpSocket.waitForReadyRead(m_timeout)) {
        return DVRBrand::Unknown;
    }
    
    QByteArray response = httpSocket.readAll();
    httpSocket.disconnectFromHost();
    
    QString resp = QString::fromUtf8(response).toLower();
    
    // Check for brand-specific HTTP patterns
    if (resp.contains("hikvision")) return DVRBrand::Hikvision;
    if (resp.contains("dahua")) return DVRBrand::Dahua;
    if (resp.contains("axis")) return DVRBrand::Axis;
    if (resp.contains("bosch")) return DVRBrand::Bosch;
    if (resp.contains("reolink")) return DVRBrand::Reolink;
    if (resp.contains("tapo") || resp.contains("tp-link")) return DVRBrand::TPLinkTapo;
    if (resp.contains("uniview") || resp.contains("unv")) return DVRBrand::Uniview;
    if (resp.contains("sony")) return DVRBrand::Sony;
    
    return DVRBrand::Unknown;
}

// ============================================================================
// NetworkScanner Implementation
// ============================================================================

NetworkScanner::NetworkScanner(QObject* parent)
    : QObject(parent)
    , m_threadPool(new QThreadPool(this))
{
    m_threadPool->setMaxThreadCount(50); // High concurrency for fast scanning
}

NetworkScanner::~NetworkScanner() {
    stopScan();
    delete m_threadPool;
}

QStringList NetworkScanner::getLocalSubnets() {
    QStringList subnets;
    
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    
    for (const QHostAddress& addr : addresses) {
        if (addr.isLoopback() || addr.protocol() != QAbstractSocket::IPv4Protocol) {
            continue;
        }
        
        QString ip = addr.toString();
        QStringList parts = ip.split('.');
        
        if (parts.size() == 4) {
            // Check for private network ranges
            QString subnet = parts[0] + "." + parts[1];
            
            // 192.168.x.x range
            if (parts[0] == "192" && parts[1] == "168") {
                if (!subnets.contains(subnet)) {
                    subnets.append(subnet);
                }
            }
            // 10.x.x.x range (just add the 10.x)
            else if (parts[0] == "10") {
                QString subnet10 = parts[0] + "." + parts[1];
                if (!subnets.contains(subnet10)) {
                    subnets.append(subnet10);
                }
            }
            // 172.16.x.x - 172.31.x.x range
            else if (parts[0] == "172") {
                int second = parts[1].toInt();
                if (second >= 16 && second <= 31) {
                    if (!subnets.contains(subnet)) {
                        subnets.append(subnet);
                    }
                }
            }
        }
    }
    
    // If no local subnets found, default to common ranges
    if (subnets.isEmpty()) {
        subnets << "192.168.0" << "192.168.1";
    }
    
    return subnets;
}

QStringList NetworkScanner::generateScanList() {
    QStringList ipList;
    QStringList subnets = getLocalSubnets();
    
    // For the spec requirement: scan 192.168.0.1 to 192.168.255.254
    // We'll scan the entire 192.168.x.x range plus detected subnets
    
    // Scan entire 192.168.x.x range
    for (int third = 0; third <= 255; ++third) {
        for (int fourth = 1; fourth <= 254; ++fourth) {
            QString ip = QString("192.168.%1.%2").arg(third).arg(fourth);
            ipList.append(ip);
        }
    }
    
    // Add detected non-192.168 subnets
    for (const QString& subnet : subnets) {
        if (!subnet.startsWith("192.168")) {
            for (int fourth = 1; fourth <= 254; ++fourth) {
                QString ip = QString("%1.%2").arg(subnet).arg(fourth);
                if (!ipList.contains(ip)) {
                    ipList.append(ip);
                }
            }
        }
    }
    
    return ipList;
}

QStringList NetworkScanner::parseRange(const QString& range) {
    QStringList ipList;
    
    // Format: "192.168.1.1-192.168.1.254" or "192.168.1.0/24"
    
    if (range.contains("/")) {
        // CIDR notation
        QStringList parts = range.split("/");
        if (parts.size() == 2) {
            QString baseIP = parts[0];
            int prefix = parts[1].toInt();
            
            QStringList baseParts = baseIP.split(".");
            if (baseParts.size() == 4) {
                int base3 = baseParts[2].toInt();
                int base4 = baseParts[3].toInt();
                
                if (prefix == 24) {
                    for (int i = 1; i <= 254; ++i) {
                        ipList.append(QString("%1.%2.%3.%4")
                            .arg(baseParts[0])
                            .arg(baseParts[1])
                            .arg(base3)
                            .arg(i));
                    }
                } else if (prefix == 16) {
                    for (int third = 0; third <= 255; ++third) {
                        for (int fourth = 1; fourth <= 254; ++fourth) {
                            ipList.append(QString("%1.%2.%3.%4")
                                .arg(baseParts[0])
                                .arg(baseParts[1])
                                .arg(third)
                                .arg(fourth));
                        }
                    }
                }
            }
        }
    } else if (range.contains("-")) {
        // Range notation
        QStringList parts = range.split("-");
        if (parts.size() == 2) {
            QString startIP = parts[0].trimmed();
            QString endIP = parts[1].trimmed();
            
            QStringList startParts = startIP.split(".");
            QStringList endParts = endIP.split(".");
            
            if (startParts.size() == 4 && endParts.size() == 4) {
                int startThird = startParts[2].toInt();
                int startFourth = startParts[3].toInt();
                int endThird = endParts[2].toInt();
                int endFourth = endParts[3].toInt();
                
                QString base = QString("%1.%2").arg(startParts[0]).arg(startParts[1]);
                
                for (int third = startThird; third <= endThird; ++third) {
                    int start = (third == startThird) ? startFourth : 1;
                    int end = (third == endThird) ? endFourth : 254;
                    
                    for (int fourth = start; fourth <= end; ++fourth) {
                        ipList.append(QString("%1.%2.%3").arg(base).arg(third).arg(fourth));
                    }
                }
            }
        }
    }
    
    return ipList;
}

void NetworkScanner::startScan(const QString& customRange) {
    if (m_scanning.load()) {
        ErrorLogger::instance().logWarning("SCAN001", "Scan already in progress");
        return;
    }
    
    m_scanning.store(true);
    m_scannedCount.store(0);
    m_progress.store(0);
    m_discoveredDVRs.clear();
    emit scanningChanged();
    emit discoveredChanged();
    
    QStringList ipList;
    
    if (!customRange.isEmpty()) {
        ipList = parseRange(customRange);
    } else {
        ipList = generateScanList();
    }
    
    m_totalHosts = ipList.size();
    
    ErrorLogger::instance().logInfo("SCAN002", 
        QString("Starting scan of %1 hosts").arg(m_totalHosts));
    
    for (const QString& ip : ipList) {
        if (!m_scanning.load()) break;
        
        ScanTask* task = new ScanTask(ip, 554, 1500);
        connect(task, &ScanTask::dvrDetected, this, &NetworkScanner::onDVRDetected);
        
        // Note: We can't connect progressUpdate directly because task is deleted after run
        m_threadPool->start(task);
    }
    
    // Monitor thread pool for completion
    QTimer* completionTimer = new QTimer(this);
    connect(completionTimer, &QTimer::timeout, this, [this, completionTimer]() {
        int active = m_threadPool->activeThreadCount();
        
        if (m_scanning.load() && active == 0) {
            m_scanning.store(false);
            emit scanningChanged();
            emit scanCompleted(m_discoveredDVRs.size());
            completionTimer->stop();
            completionTimer->deleteLater();
        }
        
        // Update progress
        int completed = m_scannedCount.load();
        int newProgress = (completed * 100) / qMax(1, m_totalHosts);
        if (newProgress != m_progress.load()) {
            m_progress.store(newProgress);
            emit progressChanged();
            emit scanProgress(completed, m_totalHosts);
        }
    });
    completionTimer->start(500);
}

void NetworkScanner::stopScan() {
    m_scanning.store(false);
    m_threadPool->clear();
    m_threadPool->waitForDone(3000);
    emit scanningChanged();
    ErrorLogger::instance().logInfo("SCAN003", "Scan stopped by user");
}

void NetworkScanner::onDVRDetected(const DVRInfo& info) {
    m_discoveredDVRs.append(info);
    emit dvrFound(info);
    emit discoveredChanged();
    
    ErrorLogger::instance().logInfo("SCAN004", 
        QString("DVR detected: %1 at %2").arg(info.brandName).arg(info.ipAddress));
}

void NetworkScanner::onTaskCompleted() {
    m_scannedCount++;
    int newProgress = (m_scannedCount.load() * 100) / qMax(1, m_totalHosts);
    
    if (newProgress != m_progress.load()) {
        m_progress.store(newProgress);
        emit progressChanged();
        emit scanProgress(m_scannedCount.load(), m_totalHosts);
    }
}

} // namespace telecloud
