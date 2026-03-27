/**
 * @file main.qml
 * @brief Main application window for TELECLOUD-MULTI
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * 
 * This is the root QML file that defines the main application window
 * with navigation between Dashboard and Configuration pages.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "pages"
import "styles"

ApplicationWindow {
    id: mainWindow
    
    visible: true
    width: 1280
    height: 720
    minimumWidth: 800
    minimumHeight: 600
    
    title: qsTr("TELECLOUD-MULTI 1.00v")
    
    // Dark theme colors
    color: Theme.backgroundColor
    
    // System tray support for desktop
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | 
           Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
    
    // Global properties
    property bool isRecording: Recorder.recording
    property int activeCameras: Recorder.activeCameraCount
    
    // Handle close event
    onClosing: function(close) {
        if (isRecording) {
            close.accepted = false
            closeDialog.open()
        }
    }
    
    // Background
    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundColor
    }
    
    // Main layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Theme.headerBackgroundColor
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                
                // Logo and title
                Row {
                    spacing: 12
                    
                    Image {
                        source: "qrc:/Icon.png"
                        sourceSize.width: 40
                        sourceSize.height: 40
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    Label {
                        text: "TELECLOUD-MULTI"
                        font.pixelSize: 20
                        font.bold: true
                        color: Theme.accentColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    Label {
                        text: "1.00v"
                        font.pixelSize: 14
                        color: Theme.secondaryTextColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // Status indicators
                Row {
                    spacing: 20
                    
                    // Recording status
                    Rectangle {
                        width: recordingStatus.implicitWidth + 20
                        height: 32
                        radius: 16
                        color: isRecording ? Theme.recordingColor : Theme.inactiveColor
                        
                        Row {
                            anchors.centerIn: parent
                            spacing: 8
                            
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: isRecording ? "#FFFFFF" : Theme.secondaryTextColor
                                
                                SequentialAnimation on opacity {
                                    running: isRecording
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 500 }
                                    NumberAnimation { to: 1.0; duration: 500 }
                                }
                            }
                            
                            Label {
                                id: recordingStatus
                                text: isRecording ? qsTr("RECORDING") : qsTr("IDLE")
                                font.pixelSize: 12
                                font.bold: true
                                color: "#FFFFFF"
                            }
                        }
                    }
                    
                    // Camera count
                    Rectangle {
                        width: cameraCountLabel.implicitWidth + 20
                        height: 32
                        radius: 16
                        color: Theme.cardBackgroundColor
                        
                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            
                            Label {
                                text: "📹"
                                font.pixelSize: 14
                            }
                            
                            Label {
                                id: cameraCountLabel
                                text: activeCameras + " " + qsTr("cameras")
                                font.pixelSize: 12
                                color: Theme.primaryTextColor
                            }
                        }
                    }
                }
            }
        }
        
        // Navigation bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: Theme.navBackgroundColor
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                
                TabBar {
                    id: navBar
                    Layout.fillWidth: true
                    background: null
                    
                    currentIndex: swipeView.currentIndex
                    
                    TabButton {
                        text: qsTr("Dashboard")
                        width: implicitWidth + 40
                        
                        background: Rectangle {
                            color: parent.checked ? Theme.accentColor : "transparent"
                            radius: 8
                        }
                        
                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 14
                            font.bold: parent.checked
                            color: parent.checked ? "#FFFFFF" : Theme.secondaryTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    
                    TabButton {
                        text: qsTr("Configuration")
                        width: implicitWidth + 40
                        
                        background: Rectangle {
                            color: parent.checked ? Theme.accentColor : "transparent"
                            radius: 8
                        }
                        
                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 14
                            font.bold: parent.checked
                            color: parent.checked ? "#FFFFFF" : Theme.secondaryTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // Storage indicator
                Row {
                    spacing: 8
                    
                    Label {
                        text: "💾"
                        font.pixelSize: 16
                    }
                    
                    Label {
                        text: qsTr("Storage: ") + formatBytes(getAvailableStorage())
                        font.pixelSize: 12
                        color: Theme.secondaryTextColor
                    }
                }
            }
        }
        
        // Main content
        SwipeView {
            id: swipeView
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navBar.currentIndex
            interactive: false
            
            DashboardPage {
                id: dashboardPage
            }
            
            ConfigurationPage {
                id: configurationPage
            }
        }
    }
    
    // Close confirmation dialog
    Dialog {
        id: closeDialog
        title: qsTr("Recording in Progress")
        modal: true
        anchors.centerIn: parent
        
        Label {
            text: qsTr("A recording is in progress. Are you sure you want to exit?\nThis will stop all recordings.")
            color: Theme.primaryTextColor
            wrapMode: Text.WordWrap
        }
        
        standardButtons: Dialog.Ok | Dialog.Cancel
        
        background: Rectangle {
            color: Theme.cardBackgroundColor
            radius: 12
            border.color: Theme.borderColor
            border.width: 1
        }
        
        onAccepted: {
            Recorder.stopAll()
            Qt.quit()
        }
    }
    
    // Functions
    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
    }
    
    function getAvailableStorage() {
        // This would be implemented in C++ and exposed to QML
        return 0 // Placeholder
    }
}
