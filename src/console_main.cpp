/**
 * @file console_main.cpp
 * @brief Console-only entry point for TELECLOUD-MULTI (headless builds)
 * @version 1.00
 */

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

#include "core/errorlogger.h"
#include "core/networkscanner.h"
#include "core/rtspprobe.h"
#include "core/recorder.h"
#include "core/dvrmanager.h"

using namespace telecloud;

/**
 * @brief Console application entry point
 */
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("TELECLOUD-MULTI");
    app.setApplicationVersion("1.00");
    
    qDebug() << "TELECLOUD-MULTI Console v1.00";
    qDebug() << "=============================";
    
    // Initialize FFmpeg
    avformat_network_init();
    av_log_set_level(AV_LOG_WARNING);
    
    // Initialize error logger
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(dataDir);
    if (!dir.exists()) dir.mkpath(".");
    ErrorLogger::instance().setLogFilePath(dataDir + "/error.json");
    
    qDebug() << "Data directory:" << dataDir;
    
    // Initialize managers
    DVRManager dvrManager;
    dvrManager.loadConfiguration();
    
    NetworkScanner scanner;
    Recorder recorder;
    
    // Print configuration
    auto appConfig = dvrManager.appConfig();
    qDebug() << "\nConfiguration:";
    qDebug() << "  Storage:" << appConfig.storageDirectory;
    qDebug() << "  Segment Duration:" << appConfig.segmentDurationMinutes << "minutes";
    qDebug() << "  DVRs configured:" << dvrManager.dvrCount();
    
    // Exit after 5 seconds for testing
    QTimer::singleShot(5000, [&]() {
        qDebug() << "\nConsole test completed.";
        QCoreApplication::quit();
    });
    
    return app.exec();
}
