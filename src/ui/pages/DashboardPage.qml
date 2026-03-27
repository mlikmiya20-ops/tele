/**
 * @file DashboardPage.qml
 * @brief Dashboard page showing recording status and statistics
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * 
 * This page displays:
 * - Recording status and controls
 * - DVR connection information
 * - Camera statistics
 * - Disk space information
 * - Detected codec information
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuickCharts
import "../styles"
import "../components"

Page {
    id: dashboardPage
    
    background: Rectangle {
        color: Theme.backgroundColor
    }
    
    // Properties
    property var recordingStats: ({})
    property var dvrConfig: ({})
    property bool isRecording: Recorder.recording
    property int recordingDuration: 0
    property var startTime: null
    
    // Update timer for recording duration
    Timer {
        id: durationTimer
        interval: 1000
        running: isRecording
        repeat: true
        onTriggered: {
            if (startTime) {
                recordingDuration = Math.floor((Date.now() - startTime.getTime()) / 1000)
            }
        }
    }
    
    // Stats update timer
    Timer {
        id: statsTimer
        interval: 2000
        running: true
        repeat: true
        onTriggered: {
            updateStats()
        }
    }
    
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingMedium
            
            // Top row - Status and Controls
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium
                
                // DVR Status Card
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    
                    title: qsTr("DVR Status")
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingNormal
                        
                        // DVR Info
                        RowLayout {
                            Layout.fillWidth: true
                            
                            Label {
                                text: "📹"
                                font.pixelSize: Theme.iconSizeMedium
                            }
                            
                            Label {
                                text: dvrConfig.name || qsTr("No DVR Connected")
                                font.pixelSize: Theme.fontSizeMedium
                                font.bold: true
                                color: Theme.primaryTextColor
                            }
                            
                            Item { Layout.fillWidth: true }
                            
                            Rectangle {
                                width: 12
                                height: 12
                                radius: 6
                                color: dvrConfig.ipAddress ? Theme.successColor : Theme.errorColor
                            }
                        }
                        
                        // DVR Details
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: Theme.spacingSmall
                            columnSpacing: Theme.spacingMedium
                            
                            Label {
                                text: qsTr("IP Address:")
                                color: Theme.secondaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            Label {
                                text: dvrConfig.ipAddress || "-"
                                color: Theme.primaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            
                            Label {
                                text: qsTr("Brand:")
                                color: Theme.secondaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            Label {
                                text: dvrConfig.brandName || "-"
                                color: Theme.primaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            
                            Label {
                                text: qsTr("Cameras:")
                                color: Theme.secondaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            Label {
                                text: (dvrConfig.channelCount || 0).toString()
                                color: Theme.primaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                        }
                        
                        Item { Layout.fillHeight: true }
                    }
                }
                
                // Recording Status Card
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    
                    title: qsTr("Recording Status")
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingNormal
                        
                        // Status indicator
                        RowLayout {
                            Layout.fillWidth: true
                            
                            Rectangle {
                                width: 60
                                height: 60
                                radius: 30
                                color: isRecording ? Theme.recordingColor : Theme.inactiveColor
                                
                                SequentialAnimation on opacity {
                                    running: isRecording
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.5; duration: 500 }
                                    NumberAnimation { to: 1.0; duration: 500 }
                                }
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: isRecording ? "⏺" : "⏹"
                                    font.pixelSize: 24
                                    color: "#FFFFFF"
                                }
                            }
                            
                            Column {
                                spacing: 4
                                
                                Label {
                                    text: isRecording ? qsTr("Recording") : qsTr("Idle")
                                    font.pixelSize: Theme.fontSizeLarge
                                    font.bold: true
                                    color: isRecording ? Theme.recordingColor : Theme.secondaryTextColor
                                }
                                
                                Label {
                                    text: isRecording ? formatDuration(recordingDuration) : qsTr("Not recording")
                                    font.pixelSize: Theme.fontSizeNormal
                                    color: Theme.secondaryTextColor
                                }
                            }
                            
                            Item { Layout.fillWidth: true }
                        }
                        
                        Item { Layout.fillHeight: true }
                    }
                }
                
                // Storage Card
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    
                    title: qsTr("Storage")
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingNormal
                        
                        // Storage progress
                        ProgressBar {
                            id: storageBar
                            Layout.fillWidth: true
                            value: 0.35 // Placeholder - would be calculated from actual storage
                            
                            background: Rectangle {
                                implicitWidth: 200
                                implicitHeight: 8
                                radius: 4
                                color: Theme.secondaryBackgroundColor
                            }
                            
                            contentItem: Item {
                                implicitWidth: 200
                                implicitHeight: 8
                                
                                Rectangle {
                                    width: storageBar.visualPosition * parent.width
                                    height: parent.height
                                    radius: 4
                                    color: Theme.accentColor
                                }
                            }
                        }
                        
                        RowLayout {
                            Layout.fillWidth: true
                            
                            Label {
                                text: qsTr("Used:")
                                color: Theme.secondaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            Label {
                                text: "125 GB"
                                color: Theme.primaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            
                            Item { Layout.fillWidth: true }
                            
                            Label {
                                text: qsTr("Free:")
                                color: Theme.secondaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            Label {
                                text: "230 GB"
                                color: Theme.primaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                        }
                        
                        Item { Layout.fillHeight: true }
                    }
                }
            }
            
            // Middle row - Codec Info and Bitrate
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium
                
                // Codec Information Card
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    
                    title: qsTr("Detected Codecs")
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingLarge
                        
                        // Video Codec
                        Column {
                            spacing: Theme.spacingSmall
                            
                            Label {
                                text: qsTr("Video")
                                color: Theme.secondaryTextColor
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            
                            Rectangle {
                                width: 100
                                height: 50
                                radius: Theme.radiusSmall
                                color: Theme.accentColor
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: "H.265"
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.bold: true
                                    color: "#FFFFFF"
                                }
                            }
                        }
                        
                        // Audio Codec
                        Column {
                            spacing: Theme.spacingSmall
                            
                            Label {
                                text: qsTr("Audio")
                                color: Theme.secondaryTextColor
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            
                            Rectangle {
                                width: 100
                                height: 50
                                radius: Theme.radiusSmall
                                color: Theme.secondaryAccentColor
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: "AAC"
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.bold: true
                                    color: "#FFFFFF"
                                }
                            }
                        }
                        
                        // Container
                        Column {
                            spacing: Theme.spacingSmall
                            
                            Label {
                                text: qsTr("Container")
                                color: Theme.secondaryTextColor
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            
                            Rectangle {
                                width: 100
                                height: 50
                                radius: Theme.radiusSmall
                                color: Theme.successColor
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: "MP4"
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.bold: true
                                    color: "#FFFFFF"
                                }
                            }
                        }
                        
                        Item { Layout.fillWidth: true }
                    }
                }
                
                // Bitrate Graph Card
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    
                    title: qsTr("Bitrate (kbps)")
                    
                    Item {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        
                        // Placeholder for bitrate graph
                        Label {
                            anchors.centerIn: parent
                            text: isRecording ? "2456 kbps" : qsTr("No data")
                            font.pixelSize: Theme.fontSizeTitle
                            color: isRecording ? Theme.accentColor : Theme.secondaryTextColor
                        }
                    }
                }
            }
            
            // Camera List
            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                
                title: qsTr("Active Cameras")
                
                ListView {
                    id: cameraListView
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    clip: true
                    
                    model: cameraModel
                    spacing: Theme.spacingSmall
                    
                    delegate: Rectangle {
                        width: cameraListView.width
                        height: 50
                        radius: Theme.radiusSmall
                        color: Theme.secondaryBackgroundColor
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMedium
                            anchors.rightMargin: Theme.spacingMedium
                            
                            Label {
                                text: "📹"
                                font.pixelSize: Theme.iconSizeNormal
                            }
                            
                            Label {
                                text: qsTr("Camera") + " " + model.cameraNumber
                                font.pixelSize: Theme.fontSizeNormal
                                color: Theme.primaryTextColor
                            }
                            
                            Label {
                                text: model.resolution || "1920x1080"
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.secondaryTextColor
                            }
                            
                            Label {
                                text: model.codec || "H.265"
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.accentColor
                            }
                            
                            Item { Layout.fillWidth: true }
                            
                            Rectangle {
                                width: 80
                                height: 30
                                radius: Theme.radiusSmall
                                color: model.recording ? Theme.recordingColor : Theme.inactiveColor
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: model.recording ? qsTr("Recording") : qsTr("Idle")
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: "#FFFFFF"
                                }
                            }
                        }
                    }
                }
            }
            
            // Control Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium
                
                Item { Layout.fillWidth: true }
                
                // Start Recording Button
                Button {
                    id: startButton
                    text: qsTr("Start Recording")
                    enabled: !isRecording && DVRManager.dvrCount > 0
                    
                    contentItem: Label {
                        text: startButton.text
                        font.pixelSize: Theme.fontSizeMedium
                        font.bold: true
                        color: startButton.enabled ? "#FFFFFF" : Theme.disabledTextColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    background: Rectangle {
                        implicitWidth: 180
                        implicitHeight: Theme.buttonHeightLarge
                        radius: Theme.radiusNormal
                        color: startButton.enabled ? 
                               (startButton.pressed ? Theme.successColor : Theme.accentColor) : 
                               Theme.inactiveColor
                    }
                    
                    onClicked: {
                        startRecording()
                    }
                }
                
                // Stop Recording Button
                Button {
                    id: stopButton
                    text: qsTr("Stop Recording")
                    enabled: isRecording
                    
                    contentItem: Label {
                        text: stopButton.text
                        font.pixelSize: Theme.fontSizeMedium
                        font.bold: true
                        color: stopButton.enabled ? "#FFFFFF" : Theme.disabledTextColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    background: Rectangle {
                        implicitWidth: 180
                        implicitHeight: Theme.buttonHeightLarge
                        radius: Theme.radiusNormal
                        color: stopButton.enabled ? 
                               (stopButton.pressed ? Theme.errorColor : Theme.recordingColor) : 
                               Theme.inactiveColor
                    }
                    
                    onClicked: {
                        stopRecording()
                    }
                }
            }
        }
    }
    
    // Camera model for list
    ListModel {
        id: cameraModel
        
        ListElement {
            cameraNumber: 1
            resolution: "1920x1080"
            codec: "H.265"
            recording: false
        }
        ListElement {
            cameraNumber: 2
            resolution: "1920x1080"
            codec: "H.265"
            recording: false
        }
        ListElement {
            cameraNumber: 3
            resolution: "1280x720"
            codec: "H.264"
            recording: false
        }
    }
    
    // Functions
    function formatDuration(seconds) {
        var hours = Math.floor(seconds / 3600)
        var minutes = Math.floor((seconds % 3600) / 60)
        var secs = seconds % 60
        
        return pad(hours) + ":" + pad(minutes) + ":" + pad(secs)
    }
    
    function pad(num) {
        return num < 10 ? "0" + num : num.toString()
    }
    
    function updateStats() {
        recordingStats = Recorder.getAllStats()
        // Update camera model with actual stats
    }
    
    function startRecording() {
        var configs = DVRManager.generateRecordingConfigs()
        if (configs.length > 0) {
            Recorder.startAll(configs)
            startTime = new Date()
            recordingDuration = 0
            
            // Update camera model
            for (var i = 0; i < cameraModel.count; i++) {
                cameraModel.setProperty(i, "recording", true)
            }
        }
    }
    
    function stopRecording() {
        Recorder.stopAll()
        startTime = null
        recordingDuration = 0
        
        // Update camera model
        for (var i = 0; i < cameraModel.count; i++) {
            cameraModel.setProperty(i, "recording", false)
        }
    }
    
    // Connections
    Connections {
        target: Recorder
        
        function onRecordingChanged() {
            isRecording = Recorder.recording
        }
        
        function onStatsUpdated(statsMap) {
            recordingStats = statsMap
        }
    }
    
    // Initialize
    Component.onCompleted: {
        updateStats()
    }
}
