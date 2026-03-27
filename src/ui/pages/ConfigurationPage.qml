/**
 * @file ConfigurationPage.qml
 * @brief Configuration page for DVR and camera settings
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * 
 * This page provides:
 * - DVR connection settings (IP, port, credentials)
 * - Recording parameters (FPS, segment duration)
 * - Network scanning functionality
 * - Camera detection and configuration
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../styles"
import "../components"

Page {
    id: configPage
    
    background: Rectangle {
        color: Theme.backgroundColor
    }
    
    // Properties
    property bool isScanning: NetworkScanner.scanning
    property int scanProgress: NetworkScanner.progress
    property var discoveredDVRs: []
    property var selectedDVR: null
    property bool isProbing: false
    
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingMedium
            
            // Recording Settings Card
            Card {
                Layout.fillWidth: true
                
                title: qsTr("Recording Settings")
                icon: "⚙️"
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    
                    // Settings grid
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: Theme.spacingMedium
                        columnSpacing: Theme.spacingLarge
                        
                        // FPS
                        Label {
                            text: qsTr("Target FPS:")
                            color: Theme.secondaryTextColor
                            font.pixelSize: Theme.fontSizeNormal
                        }
                        
                        SpinBox {
                            id: fpsSpinBox
                            from: 1
                            to: 60
                            value: 12
                            editable: true
                            
                            textFromValue: function(value) {
                                return value + " fps"
                            }
                            
                            valueFromText: function(text) {
                                return parseInt(text)
                            }
                            
                            background: Rectangle {
                                implicitWidth: 120
                                implicitHeight: Theme.inputHeight
                                radius: Theme.radiusSmall
                                color: Theme.inputBackgroundColor
                                border.color: fpsSpinBox.activeFocus ? Theme.accentColor : Theme.inputBorderColor
                            }
                            
                            contentItem: Label {
                                text: fpsSpinBox.textFromValue(fpsSpinBox.value, fpsSpinBox.locale)
                                font.pixelSize: Theme.fontSizeNormal
                                color: Theme.primaryTextColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        
                        // Segment Duration
                        Label {
                            text: qsTr("Segment Duration:")
                            color: Theme.secondaryTextColor
                            font.pixelSize: Theme.fontSizeNormal
                        }
                        
                        SpinBox {
                            id: durationSpinBox
                            from: 1
                            to: 240
                            value: 60
                            editable: true
                            
                            textFromValue: function(value) {
                                return value + " min"
                            }
                            
                            valueFromText: function(text) {
                                return parseInt(text)
                            }
                            
                            background: Rectangle {
                                implicitWidth: 120
                                implicitHeight: Theme.inputHeight
                                radius: Theme.radiusSmall
                                color: Theme.inputBackgroundColor
                                border.color: durationSpinBox.activeFocus ? Theme.accentColor : Theme.inputBorderColor
                            }
                            
                            contentItem: Label {
                                text: durationSpinBox.textFromValue(durationSpinBox.value, durationSpinBox.locale)
                                font.pixelSize: Theme.fontSizeNormal
                                color: Theme.primaryTextColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        
                        // Storage Directory
                        Label {
                            text: qsTr("Storage Directory:")
                            color: Theme.secondaryTextColor
                            font.pixelSize: Theme.fontSizeNormal
                        }
                        
                        RowLayout {
                            spacing: Theme.spacingSmall
                            
                            TextField {
                                id: storageField
                                Layout.fillWidth: true
                                text: Recorder.storageDirectory
                                placeholderText: qsTr("Select storage directory...")
                                
                                background: Rectangle {
                                    implicitHeight: Theme.inputHeight
                                    radius: Theme.radiusSmall
                                    color: Theme.inputBackgroundColor
                                    border.color: storageField.activeFocus ? Theme.accentColor : Theme.inputBorderColor
                                }
                                
                                color: Theme.primaryTextColor
                                font.pixelSize: Theme.fontSizeNormal
                            }
                            
                            Button {
                                text: qsTr("Browse")
                                
                                background: Rectangle {
                                    implicitWidth: 80
                                    implicitHeight: Theme.inputHeight
                                    radius: Theme.radiusSmall
                                    color: parent.pressed ? Theme.accentColorDark : Theme.accentColor
                                }
                                
                                contentItem: Label {
                                    text: parent.text
                                    font.pixelSize: Theme.fontSizeNormal
                                    color: "#FFFFFF"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                
                                onClicked: {
                                    folderDialog.open()
                                }
                            }
                        }
                    }
                }
            }
            
            // DVR Credentials Card
            Card {
                Layout.fillWidth: true
                
                title: qsTr("DVR Credentials")
                icon: "🔐"
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: Theme.spacingMedium
                        columnSpacing: Theme.spacingLarge
                        
                        // Username
                        Label {
                            text: qsTr("Username:")
                            color: Theme.secondaryTextColor
                            font.pixelSize: Theme.fontSizeNormal
                        }
                        
                        TextField {
                            id: usernameField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Enter DVR username")
                            text: "admin"
                            
                            background: Rectangle {
                                implicitHeight: Theme.inputHeight
                                radius: Theme.radiusSmall
                                color: Theme.inputBackgroundColor
                                border.color: usernameField.activeFocus ? Theme.accentColor : Theme.inputBorderColor
                            }
                            
                            color: Theme.primaryTextColor
                            font.pixelSize: Theme.fontSizeNormal
                        }
                        
                        // Password
                        Label {
                            text: qsTr("Password:")
                            color: Theme.secondaryTextColor
                            font.pixelSize: Theme.fontSizeNormal
                        }
                        
                        TextField {
                            id: passwordField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Enter DVR password")
                            echoMode: TextInput.Password
                            
                            background: Rectangle {
                                implicitHeight: Theme.inputHeight
                                radius: Theme.radiusSmall
                                color: Theme.inputBackgroundColor
                                border.color: passwordField.activeFocus ? Theme.accentColor : Theme.inputBorderColor
                            }
                            
                            color: Theme.primaryTextColor
                            font.pixelSize: Theme.fontSizeNormal
                            
                            rightPadding: showPasswordButton.width + Theme.spacingNormal
                            
                            Button {
                                id: showPasswordButton
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: 30
                                height: 30
                                flat: true
                                
                                text: passwordField.echoMode === TextInput.Password ? "👁" : "🔒"
                                
                                contentItem: Label {
                                    text: parent.text
                                    font.pixelSize: Theme.iconSizeSmall
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                
                                background: null
                                
                                onClicked: {
                                    passwordField.echoMode = passwordField.echoMode === TextInput.Password ? 
                                                              TextInput.Normal : TextInput.Password
                                }
                            }
                        }
                    }
                }
            }
            
            // Network Scan Card
            Card {
                Layout.fillWidth: true
                
                title: qsTr("Network Scanner")
                icon: "🔍"
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    
                    // Scan controls
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Label {
                            text: qsTr("Scan local network for DVR devices")
                            color: Theme.secondaryTextColor
                            font.pixelSize: Theme.fontSizeNormal
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        // Scan progress
                        Label {
                            visible: isScanning
                            text: qsTr("Scanning...") + " " + scanProgress + "%"
                            color: Theme.accentColor
                            font.pixelSize: Theme.fontSizeNormal
                        }
                        
                        // Scan button
                        Button {
                            id: scanButton
                            text: isScanning ? qsTr("Stop Scan") : qsTr("Scan and Configure")
                            
                            background: Rectangle {
                                implicitWidth: 180
                                implicitHeight: Theme.buttonHeight
                                radius: Theme.radiusNormal
                                color: scanButton.pressed ? Theme.accentColorDark : Theme.accentColor
                            }
                            
                            contentItem: Row {
                                anchors.centerIn: parent
                                spacing: Theme.spacingSmall
                                
                                Label {
                                    text: isScanning ? "⏹" : "🔍"
                                    font.pixelSize: Theme.iconSizeSmall
                                    color: "#FFFFFF"
                                }
                                
                                Label {
                                    text: scanButton.text
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.bold: true
                                    color: "#FFFFFF"
                                }
                            }
                            
                            onClicked: {
                                if (isScanning) {
                                    NetworkScanner.stopScan()
                                } else {
                                    startNetworkScan()
                                }
                            }
                        }
                    }
                    
                    // Progress bar
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: isScanning
                        value: scanProgress / 100
                        
                        background: Rectangle {
                            implicitWidth: 200
                            implicitHeight: 6
                            radius: 3
                            color: Theme.secondaryBackgroundColor
                        }
                        
                        contentItem: Item {
                            implicitWidth: 200
                            implicitHeight: 6
                            
                            Rectangle {
                                width: parent.parent.visualPosition * parent.width
                                height: parent.height
                                radius: 3
                                color: Theme.accentColor
                            }
                        }
                    }
                    
                    // Discovered DVRs list
                    Label {
                        visible: !isScanning && discoveredDVRs.length === 0
                        text: qsTr("No DVRs discovered. Click 'Scan and Configure' to search your network.")
                        color: Theme.secondaryTextColor
                        font.pixelSize: Theme.fontSizeNormal
                    }
                    
                    ListView {
                        id: dvrListView
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(200, contentHeight)
                        visible: discoveredDVRs.length > 0
                        clip: true
                        
                        model: discoveredDVRs
                        spacing: Theme.spacingSmall
                        
                        delegate: Rectangle {
                            width: dvrListView.width
                            height: 60
                            radius: Theme.radiusSmall
                            color: selectedDVR === modelData ? Theme.accentColorDark : Theme.secondaryBackgroundColor
                            border.color: selectedDVR === modelData ? Theme.accentColor : "transparent"
                            border.width: 2
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                spacing: Theme.spacingMedium
                                
                                Label {
                                    text: "📹"
                                    font.pixelSize: Theme.iconSizeMedium
                                }
                                
                                Column {
                                    spacing: 2
                                    
                                    Label {
                                        text: modelData.brandName
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.bold: true
                                        color: Theme.primaryTextColor
                                    }
                                    
                                    Label {
                                        text: modelData.ipAddress + ":" + modelData.port
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.secondaryTextColor
                                    }
                                }
                                
                                Item { Layout.fillWidth: true }
                                
                                Button {
                                    text: qsTr("Select")
                                    
                                    background: Rectangle {
                                        implicitWidth: 70
                                        implicitHeight: 32
                                        radius: Theme.radiusSmall
                                        color: parent.pressed ? Theme.accentColor : Theme.secondaryAccentColor
                                    }
                                    
                                    contentItem: Label {
                                        text: parent.text
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: "#FFFFFF"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    
                                    onClicked: {
                                        selectDVR(modelData)
                                    }
                                }
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    selectedDVR = modelData
                                }
                            }
                        }
                    }
                }
            }
            
            // Detected Cameras Card
            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 250
                
                title: qsTr("Detected Cameras")
                icon: "🎥"
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    
                    Label {
                        visible: cameraListModel.count === 0
                        text: qsTr("No cameras detected. Scan for DVRs to detect cameras.")
                        color: Theme.secondaryTextColor
                        font.pixelSize: Theme.fontSizeNormal
                    }
                    
                    ListView {
                        id: cameraListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: cameraListModel.count > 0
                        clip: true
                        
                        model: ListModel {
                            id: cameraListModel
                        }
                        
                        spacing: Theme.spacingSmall
                        
                        delegate: Rectangle {
                            width: cameraListView.width
                            height: 50
                            radius: Theme.radiusSmall
                            color: Theme.secondaryBackgroundColor
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                
                                CheckBox {
                                    id: cameraCheckBox
                                    checked: model.enabled
                                    
                                    indicator: Rectangle {
                                        implicitWidth: 22
                                        implicitHeight: 22
                                        radius: Theme.radiusSmall
                                        color: cameraCheckBox.checked ? Theme.accentColor : Theme.inputBackgroundColor
                                        border.color: cameraCheckBox.checked ? Theme.accentColor : Theme.inputBorderColor
                                        
                                        Label {
                                            anchors.centerIn: parent
                                            text: cameraCheckBox.checked ? "✓" : ""
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: "#FFFFFF"
                                        }
                                    }
                                    
                                    onCheckedChanged: {
                                        cameraListModel.setProperty(index, "enabled", checked)
                                    }
                                }
                                
                                Label {
                                    text: qsTr("Camera") + " " + model.cameraNumber
                                    font.pixelSize: Theme.fontSizeNormal
                                    color: Theme.primaryTextColor
                                }
                                
                                Label {
                                    text: model.resolution
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.secondaryTextColor
                                }
                                
                                Label {
                                    text: model.codec
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.accentColor
                                }
                                
                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }
            }
            
            // Action Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium
                
                Button {
                    text: qsTr("Save Configuration")
                    
                    background: Rectangle {
                        implicitWidth: 160
                        implicitHeight: Theme.buttonHeight
                        radius: Theme.radiusNormal
                        color: parent.pressed ? Theme.successColor : Theme.accentColor
                    }
                    
                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        saveConfiguration()
                    }
                }
                
                Button {
                    text: qsTr("Load Configuration")
                    
                    background: Rectangle {
                        implicitWidth: 160
                        implicitHeight: Theme.buttonHeight
                        radius: Theme.radiusNormal
                        color: parent.pressed ? Theme.secondaryBackgroundColor : Theme.cardBackgroundColor
                        border.color: Theme.borderColor
                        border.width: 1
                    }
                    
                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: Theme.fontSizeNormal
                        color: Theme.primaryTextColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        loadConfiguration()
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                Label {
                    visible: statusText.text !== ""
                    text: statusText.text
                    color: Theme.successColor
                    font.pixelSize: Theme.fontSizeNormal
                }
            }
        }
    }
    
    // Folder Dialog for storage directory
    FolderDialog {
        id: folderDialog
        title: qsTr("Select Storage Directory")
        onAccepted: {
            storageField.text = selectedFolder
        }
    }
    
    // Status text
    QtObject {
        id: statusText
        property string text: ""
    }
    
    // Status timer
    Timer {
        id: statusTimer
        interval: 3000
        onTriggered: {
            statusText.text = ""
        }
    }
    
    // Functions
    function startNetworkScan() {
        discoveredDVRs = []
        selectedDVR = null
        cameraListModel.clear()
        NetworkScanner.startScan()
    }
    
    function selectDVR(dvrInfo) {
        selectedDVR = dvrInfo
        
        // Probe channels
        isProbing = true
        
        // This would be done via signal/slot with C++ backend
        // For now, simulate with a timer
        probingTimer.start()
    }
    
    Timer {
        id: probingTimer
        interval: 100
        onTriggered: {
            // Simulate channel detection results
            cameraListModel.clear()
            
            var channelCount = Math.floor(Math.random() * 8) + 1
            var codecs = ["H.264", "H.265", "H.265+"]
            var resolutions = ["1920x1080", "2560x1440", "3840x2160", "1280x720"]
            
            for (var i = 1; i <= channelCount; i++) {
                cameraListModel.append({
                    cameraNumber: i,
                    resolution: resolutions[Math.floor(Math.random() * resolutions.length)],
                    codec: codecs[Math.floor(Math.random() * codecs.length)],
                    enabled: true
                })
            }
            
            isProbing = false
            statusText.text = qsTr("Detected %1 cameras").arg(channelCount)
            statusTimer.start()
        }
    }
    
    function saveConfiguration() {
        // Update app config
        Recorder.setSegmentDuration(durationSpinBox.value)
        Recorder.setStorageDirectory(storageField.text)
        
        // Save DVR configuration
        if (selectedDVR) {
            DVRManager.importFromDiscovery(selectedDVR, usernameField.text, passwordField.text, false)
        }
        
        DVRManager.saveConfiguration()
        
        statusText.text = qsTr("Configuration saved")
        statusTimer.start()
    }
    
    function loadConfiguration() {
        DVRManager.loadConfiguration()
        
        // Update UI from loaded config
        var appConfig = DVRManager.appConfig
        fpsSpinBox.value = appConfig.fps || 12
        durationSpinBox.value = appConfig.segmentDurationMinutes || 60
        storageField.text = appConfig.storageDirectory || ""
        
        statusText.text = qsTr("Configuration loaded")
        statusTimer.start()
    }
    
    // Connections
    Connections {
        target: NetworkScanner
        
        function onScanningChanged() {
            isScanning = NetworkScanner.scanning
        }
        
        function onProgressChanged() {
            scanProgress = NetworkScanner.progress
        }
        
        function onDvrFound(dvrInfo) {
            discoveredDVRs.push(dvrInfo)
            discoveredDVRsChanged()
        }
        
        function onScanCompleted(dvrCount) {
            statusText.text = qsTr("Scan complete: %1 DVRs found").arg(dvrCount)
            statusTimer.start()
        }
    }
    
    // Initialize
    Component.onCompleted: {
        loadConfiguration()
    }
}
