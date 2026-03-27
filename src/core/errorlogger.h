/**
 * @file errorlogger.h
 * @brief Singleton class for structured error logging with deduplication
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 * 
 * This file implements a thread-safe error logging system that:
 * - Writes errors to error.json in structured format
 * - Prevents consecutive duplicate error entries
 * - Includes timestamps, error codes, and detailed context
 */

#ifndef ERRORLOGGER_H
#define ERRORLOGGER_H

#include <QObject>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QString>
#include <memory>
#include <deque>

namespace telecloud {

/**
 * @struct ErrorEntry
 * @brief Represents a single error log entry with all relevant information
 */
struct ErrorEntry {
    QString timestamp;      ///< ISO 8601 formatted timestamp
    QString errorCode;      ///< Application-specific error code (e.g., "NET001", "RTSP002")
    QString message;        ///< Human-readable error message
    QString context;        ///< Additional context information (IP, camera ID, etc.)
    QString severity;       ///< Error severity: "info", "warning", "error", "critical"
    
    /**
     * @brief Converts the error entry to a JSON object
     * @return QJsonObject representation of the error entry
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["timestamp"] = timestamp;
        obj["errorCode"] = errorCode;
        obj["message"] = message;
        obj["context"] = context;
        obj["severity"] = severity;
        return obj;
    }
    
    /**
     * @brief Creates a unique key for deduplication comparison
     * @return QString combining error code and context for deduplication
     */
    QString deduplicationKey() const {
        return errorCode + "::" + context;
    }
};

/**
 * @class ErrorLogger
 * @brief Thread-safe singleton for structured error logging with deduplication
 * 
 * ErrorLogger provides a centralized logging mechanism that writes errors
 * to a JSON file with automatic deduplication of consecutive identical errors.
 * The singleton pattern ensures a single point of logging throughout the application.
 * 
 * @note Thread-safe: All public methods can be called from any thread
 * @note Deduplication: Consecutive errors with same code and context are not duplicated
 * 
 * Example usage:
 * @code
 * ErrorLogger::instance().logError("NET001", "Connection failed", "192.168.1.100:554", "error");
 * @endcode
 */
class ErrorLogger : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Gets the singleton instance of ErrorLogger
     * @return Reference to the singleton ErrorLogger instance
     */
    static ErrorLogger& instance() {
        static ErrorLogger instance;
        return instance;
    }
    
    /**
     * @brief Deleted copy constructor to enforce singleton pattern
     */
    ErrorLogger(const ErrorLogger&) = delete;
    
    /**
     * @brief Deleted assignment operator to enforce singleton pattern
     */
    ErrorLogger& operator=(const ErrorLogger&) = delete;
    
    /**
     * @brief Sets the path for the error log file
     * @param path Absolute path to the error.json file
     * 
     * This method should be called once at application startup to set
     * the appropriate platform-specific log file location.
     */
    void setLogFilePath(const QString& path) {
        QMutexLocker locker(&m_mutex);
        m_logFilePath = path;
        loadExistingLogs();
    }
    
    /**
     * @brief Logs an error entry with deduplication
     * @param errorCode Application-specific error code (e.g., "NET001")
     * @param message Human-readable error message
     * @param context Additional context (IP address, camera ID, operation name)
     * @param severity Error severity level
     * 
     * The error will only be written if it differs from the last error
     * (comparing errorCode and context). Timestamp is always updated.
     * 
     * Severity levels:
     * - "info": Informational messages, not errors
     * - "warning": Recoverable issues that don't stop operation
     * - "error": Errors that affect functionality but don't crash the app
     * - "critical": Severe errors that may cause application instability
     */
    void logError(const QString& errorCode, 
                  const QString& message, 
                  const QString& context = QString(),
                  const QString& severity = "error") {
        
        ErrorEntry entry;
        entry.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        entry.errorCode = errorCode;
        entry.message = message;
        entry.context = context;
        entry.severity = severity;
        
        QMutexLocker locker(&m_mutex);
        
        // Check for duplicate consecutive error
        if (!m_lastErrors.empty()) {
            if (m_lastErrors.back().deduplicationKey() == entry.deduplicationKey()) {
                // Update timestamp of last entry but don't add duplicate
                m_lastErrors.back().timestamp = entry.timestamp;
                return;
            }
        }
        
        m_lastErrors.push_back(entry);
        
        // Keep only last 1000 errors in memory
        if (m_lastErrors.size() > 1000) {
            m_lastErrors.pop_front();
        }
        
        writeToFile(entry);
        emit errorLogged(entry.errorCode, entry.message, entry.context, entry.severity);
    }
    
    /**
     * @brief Convenience method for logging info-level messages
     */
    void logInfo(const QString& errorCode, const QString& message, const QString& context = QString()) {
        logError(errorCode, message, context, "info");
    }
    
    /**
     * @brief Convenience method for logging warning-level messages
     */
    void logWarning(const QString& errorCode, const QString& message, const QString& context = QString()) {
        logError(errorCode, message, context, "warning");
    }
    
    /**
     * @brief Convenience method for logging critical-level messages
     */
    void logCritical(const QString& errorCode, const QString& message, const QString& context = QString()) {
        logError(errorCode, message, context, "critical");
    }
    
    /**
     * @brief Gets all logged errors
     * @return QJsonArray containing all error entries
     */
    QJsonArray getAllErrors() const {
        QMutexLocker locker(&m_mutex);
        QJsonArray array;
        for (const auto& entry : m_lastErrors) {
            array.append(entry.toJson());
        }
        return array;
    }
    
    /**
     * @brief Clears all logged errors from memory and file
     */
    void clearLogs() {
        QMutexLocker locker(&m_mutex);
        m_lastErrors.clear();
        
        if (!m_logFilePath.isEmpty()) {
            QFile file(m_logFilePath);
            if (file.open(QIODevice::WriteOnly)) {
                QJsonDocument doc{QJsonArray{}};
                file.write(doc.toJson());
                file.close();
            }
        }
        emit logsCleared();
    }
    
    /**
     * @brief Gets the count of errors by severity
     * @param severity The severity level to count
     * @return Number of errors with the specified severity
     */
    int getErrorCount(const QString& severity) const {
        QMutexLocker locker(&m_mutex);
        int count = 0;
        for (const auto& entry : m_lastErrors) {
            if (entry.severity == severity) {
                ++count;
            }
        }
        return count;
    }

signals:
    /**
     * @brief Emitted when a new error is logged
     * @param errorCode The error code
     * @param message The error message
     * @param context The error context
     * @param severity The error severity
     */
    void errorLogged(const QString& errorCode, const QString& message, 
                     const QString& context, const QString& severity);
    
    /**
     * @brief Emitted when logs are cleared
     */
    void logsCleared();

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    ErrorLogger() : QObject(nullptr) {}
    
    /**
     * @brief Loads existing logs from file at startup
     */
    void loadExistingLogs() {
        if (m_logFilePath.isEmpty()) return;
        
        QFile file(m_logFilePath);
        if (!file.exists()) return;
        
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            
            if (doc.isArray()) {
                QJsonArray array = doc.array();
                for (const auto& value : array) {
                    if (value.isObject()) {
                        QJsonObject obj = value.toObject();
                        ErrorEntry entry;
                        entry.timestamp = obj["timestamp"].toString();
                        entry.errorCode = obj["errorCode"].toString();
                        entry.message = obj["message"].toString();
                        entry.context = obj["context"].toString();
                        entry.severity = obj["severity"].toString();
                        m_lastErrors.push_back(entry);
                    }
                }
            }
        }
    }
    
    /**
     * @brief Writes a single error entry to the log file
     * @param entry The error entry to write
     */
    void writeToFile(const ErrorEntry& entry) {
        if (m_logFilePath.isEmpty()) return;
        
        QFile file(m_logFilePath);
        QJsonArray array;
        
        // Read existing entries
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            if (doc.isArray()) {
                array = doc.array();
            }
        }
        
        // Append new entry
        array.append(entry.toJson());
        
        // Write back to file
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonDocument doc(array);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
        }
    }
    
    mutable QMutex m_mutex;                     ///< Mutex for thread safety
    QString m_logFilePath;                      ///< Path to error.json
    std::deque<ErrorEntry> m_lastErrors;        ///< In-memory error buffer with deduplication
};

} // namespace telecloud

#endif // ERRORLOGGER_H
