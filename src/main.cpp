/**
 * @file main.cpp
 * @brief Application entry point for TELECLOUD-MULTI
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * @date 2025
 * 
 * This file initializes the Qt application, sets up the QML engine,
 * registers C++ types for QML, and handles platform-specific setup.
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QtAndroid>
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#endif

#include "core/errorlogger.h"
#include "core/networkscanner.h"
#include "core/rtspprobe.h"
#include "core/recorder.h"
#include "core/dvrmanager.h"

using namespace telecloud;

/**
 * @brief Gets the application data directory
 * @return Path to application data directory
 */
QString getAppDataDir() {
#ifdef Q_OS_ANDROID
    // Use external storage on Android
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
#endif
    
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    return path;
}

/**
 * @brief Initializes Android-specific permissions
 * @return true if all permissions granted
 */
#ifdef Q_OS_ANDROID
bool initAndroidPermissions() {
    // Required permissions for Android 8.0 to 16.0
    QStringList permissions;
    
    // Network permissions
    permissions << "android.permission.INTERNET";
    permissions << "android.permission.ACCESS_NETWORK_STATE";
    permissions << "android.permission.ACCESS_WIFI_STATE";
    
    // Storage permissions
    // For Android < 10 (API 29)
    permissions << "android.permission.WRITE_EXTERNAL_STORAGE";
    permissions << "android.permission.READ_EXTERNAL_STORAGE";
    
    // Camera and audio (for future features)
    permissions << "android.permission.CAMERA";
    permissions << "android.permission.RECORD_AUDIO";
    
    // Request permissions
    for (const QString& permission : permissions) {
        auto result = QtAndroid::requestPermissionsSync(QStringList() << permission);
        if (result[permission] != QtAndroid::PermissionResult::Granted) {
            qWarning() << "Permission not granted:" << permission;
        }
    }
    
    // For Android 11+ (API 30+), request MANAGE_EXTERNAL_STORAGE for scoped storage
    if (QtAndroid::androidSdkVersion() >= 30) {
        QAndroidJniEnvironment env;
        QAndroidJniObject activity = QtAndroid::androidActivity();
        QAndroidJniObject uri = QAndroidJniObject::callStaticObjectMethod(
            "android/net/Uri", 
            "parse", 
            "(Ljava/lang/String;)Landroid/net/Uri;",
            QAndroidJniObject::fromString("package:com.telecloud.multi").object<jstring>()
        );
        
        QAndroidJniObject intent = QAndroidJniObject::callStaticObjectMethod(
            "android/content/Intent",
            "parseUri",
            "(Ljava/lang/String;I)Landroid/content/Intent;",
            QAndroidJniObject::fromString(
                "android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION"
            ).object<jstring>(),
            0
        );
        
        activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object<jobject>());
    }
    
    return true;
}
#endif

/**
 * @brief Initializes FFmpeg libraries
 */
void initFFmpeg() {
    // Initialize FFmpeg network
    avformat_network_init();
    
    // Set FFmpeg log level
#ifdef QT_DEBUG
    av_log_set_level(AV_LOG_DEBUG);
#else
    av_log_set_level(AV_LOG_WARNING);
#endif
    
    qDebug() << "FFmpeg initialized";
    qDebug() << "FFmpeg configuration:" << avcodec_configuration();
}

/**
 * @brief Main application entry point
 */
int main(int argc, char *argv[])
{
    // High DPI scaling
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    
    QGuiApplication app(argc, argv);
    app.setApplicationName("TELECLOUD-MULTI");
    app.setApplicationVersion("1.00");
    app.setOrganizationName("TeleCloud");
    
    // Set application icon
    app.setWindowIcon(QIcon(":/Icon.png"));
    
    // Initialize FFmpeg
    initFFmpeg();
    
#ifdef Q_OS_ANDROID
    // Initialize Android permissions
    initAndroidPermissions();
#endif
    
    // Initialize error logger
    QString dataDir = getAppDataDir();
    ErrorLogger::instance().setLogFilePath(dataDir + "/error.json");
    ErrorLogger::instance().logInfo("APP001", "Application starting");
    
    // Initialize managers
    DVRManager dvrManager;
    dvrManager.loadConfiguration();
    
    NetworkScanner networkScanner;
    Recorder recorder;
    recorder.setStorageDirectory(dvrManager.appConfig().storageDirectory);
    recorder.setSegmentDuration(dvrManager.appConfig().segmentDurationMinutes);
    
    // Register QML types
    qRegisterMetaType<DVRInfo>("DVRInfo");
    qRegisterMetaType<StreamInfo>("StreamInfo");
    qRegisterMetaType<RTSPProbeResult>("RTSPProbeResult");
    qRegisterMetaType<RecordingConfig>("RecordingConfig");
    qRegisterMetaType<RecordingStats>("RecordingStats");
    qRegisterMetaType<QMap<int, RecordingStats>>("QMap<int,RecordingStats>");
    
    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Expose C++ objects to QML
    QQmlContext* context = engine.rootContext();
    
    // Singletons/Managers
    context->setContextProperty("DVRManager", &dvrManager);
    context->setContextProperty("NetworkScanner", &networkScanner);
    context->setContextProperty("Recorder", &recorder);
    
    // Expose utility functions
    context->setContextProperty("AppVersion", "1.00");
    context->setContextProperty("AppDataDir", dataDir);
    
    // Load main QML file
    const QUrl url(u"qrc:/ui/main.qml"_qs);
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);
    
    engine.load(url);
    
    // Check if QML loaded successfully
    if (engine.rootObjects().isEmpty()) {
        ErrorLogger::instance().logCritical("APP002", "Failed to load QML interface");
        return -1;
    }
    
    ErrorLogger::instance().logInfo("APP003", "Application started successfully");
    
    // Connect configuration changes
    QObject::connect(&dvrManager, &DVRManager::appConfigChanged, [&]() {
        auto config = dvrManager.appConfig();
        recorder.setStorageDirectory(config.storageDirectory);
        recorder.setSegmentDuration(config.segmentDurationMinutes);
    });
    
    // Run application
    int result = app.exec();
    
    // Cleanup
    ErrorLogger::instance().logInfo("APP004", "Application shutting down");
    recorder.stopAll();
    
    return result;
}
