/**
 * @file Card.qml
 * @brief Reusable card component for dashboard and configuration pages
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * 
 * A styled card container with title, icon, and content area.
 * Provides consistent styling across the application.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

Rectangle {
    id: root
    
    // Public properties
    property string title: ""
    property string icon: ""
    property alias content: contentArea.children
    
    // Layout properties
    implicitHeight: contentColumn.height + Theme.spacingMedium * 2
    
    // Styling
    color: Theme.cardBackgroundColor
    radius: Theme.radiusMedium
    border.color: Theme.borderColor
    border.width: 1
    
    // Shadow effect (simplified)
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 2
        anchors.leftMargin: 2
        radius: parent.radius
        color: "#000000"
        opacity: 0.2
        z: -1
    }
    
    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 0
        
        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: title !== "" ? 40 : 0
            visible: title !== ""
            color: "transparent"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMedium
                anchors.rightMargin: Theme.spacingMedium
                spacing: Theme.spacingSmall
                
                // Icon
                Label {
                    visible: root.icon !== ""
                    text: root.icon
                    font.pixelSize: Theme.iconSizeNormal
                }
                
                // Title
                Label {
                    text: root.title
                    font.pixelSize: Theme.fontSizeMedium
                    font.bold: true
                    color: Theme.primaryTextColor
                }
                
                Item { Layout.fillWidth: true }
            }
            
            // Bottom border
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.separatorColor
            }
        }
        
        // Content area
        Item {
            id: contentArea
            Layout.fillWidth: true
            Layout.preferredHeight: childrenRect.height
            visible: children.length > 0
        }
    }
    
    // Hover effect
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "#FFFFFF"
        opacity: mouseArea.containsMouse ? 0.03 : 0
        
        Behavior on opacity {
            NumberAnimation { duration: Theme.animationDurationFast }
        }
    }
    
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }
}
