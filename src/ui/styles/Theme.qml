/**
 * @file Theme.qml
 * @brief Dark theme definition for TELECLOUD-MULTI
 * @author TELECLOUD-MULTI Team
 * @version 1.00
 * 
 * This file defines the color palette and styling constants
 * for the modern dark theme used throughout the application.
 */

pragma Singleton
import QtQuick

QtObject {
    // ========================================
    // Primary Colors
    // ========================================
    
    /// Main background color - Very dark gray
    readonly property color backgroundColor: "#121212"
    
    /// Secondary background - Slightly lighter
    readonly property color secondaryBackgroundColor: "#1E1E1E"
    
    /// Card/panel background
    readonly property color cardBackgroundColor: "#252525"
    
    /// Header background
    readonly property color headerBackgroundColor: "#1A1A1A"
    
    /// Navigation background
    readonly property color navBackgroundColor: "#1E1E1E"
    
    // ========================================
    // Accent Colors
    // ========================================
    
    /// Primary accent - Blue
    readonly property color accentColor: "#2196F3"
    
    /// Primary accent - Hover/Light variant
    readonly property color accentColorLight: "#42A5F5"
    
    /// Primary accent - Dark variant
    readonly property color accentColorDark: "#1976D2"
    
    /// Secondary accent - Teal/Green
    readonly property color secondaryAccentColor: "#00BCD4"
    
    /// Success color - Green
    readonly property color successColor: "#4CAF50"
    
    /// Warning color - Orange
    readonly property color warningColor: "#FF9800"
    
    /// Error color - Red
    readonly property color errorColor: "#F44336"
    
    /// Recording indicator - Bright red
    readonly property color recordingColor: "#E53935"
    
    /// Inactive/Idle color
    readonly property color inactiveColor: "#424242"
    
    // ========================================
    // Text Colors
    // ========================================
    
    /// Primary text - White/Light
    readonly property color primaryTextColor: "#FFFFFF"
    
    /// Secondary text - Gray
    readonly property color secondaryTextColor: "#B0B0B0"
    
    /// Disabled text
    readonly property color disabledTextColor: "#666666"
    
    /// Link text
    readonly property color linkColor: "#64B5F6"
    
    // ========================================
    // Border & Separator Colors
    // ========================================
    
    /// Standard border
    readonly property color borderColor: "#333333"
    
    /// Light border
    readonly property color borderLightColor: "#404040"
    
    /// Separator line
    readonly property color separatorColor: "#2A2A2A"
    
    // ========================================
    // Input Field Colors
    // ========================================
    
    /// Input background
    readonly property color inputBackgroundColor: "#1E1E1E"
    
    /// Input border
    readonly property color inputBorderColor: "#404040"
    
    /// Input border - Focused
    readonly property color inputBorderFocusColor: "#2196F3"
    
    /// Selection highlight
    readonly property color selectionColor: "#2196F3"
    
    // ========================================
    // Chart & Graph Colors
    // ========================================
    
    /// Chart color palette
    readonly property var chartColors: [
        "#2196F3", // Blue
        "#00BCD4", // Cyan
        "#4CAF50", // Green
        "#FF9800", // Orange
        "#9C27B0", // Purple
        "#E91E63", // Pink
        "#009688", // Teal
        "#FFC107"  // Amber
    ]
    
    // ========================================
    // Shadows & Elevation
    // ========================================
    
    /// Elevation 1 shadow
    readonly property int elevation1: 2
    
    /// Elevation 2 shadow
    readonly property int elevation2: 4
    
    /// Elevation 3 shadow
    readonly property int elevation3: 8
    
    // ========================================
    // Typography
    // ========================================
    
    /// Font family
    readonly property string fontFamily: "Segoe UI, Roboto, -apple-system, BlinkMacSystemFont, sans-serif"
    
    /// Font sizes
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeMedium: 15
    readonly property int fontSizeLarge: 18
    readonly property int fontSizeTitle: 24
    readonly property int fontSizeHeading: 32
    
    /// Font weights
    readonly property int fontWeightNormal: 400
    readonly property int fontWeightMedium: 500
    readonly property int fontWeightBold: 700
    
    // ========================================
    // Spacing & Sizing
    // ========================================
    
    /// Standard spacing
    readonly property int spacingSmall: 4
    readonly property int spacingNormal: 8
    readonly property int spacingMedium: 16
    readonly property int spacingLarge: 24
    readonly property int spacingXLarge: 32
    
    /// Border radius
    readonly property int radiusSmall: 4
    readonly property int radiusNormal: 8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge: 16
    readonly property int radiusRound: 999
    
    /// Button sizes
    readonly property int buttonHeight: 40
    readonly property int buttonHeightSmall: 32
    readonly property int buttonHeightLarge: 48
    
    /// Input field height
    readonly property int inputHeight: 40
    
    /// Icon sizes
    readonly property int iconSizeSmall: 16
    readonly property int iconSizeNormal: 24
    readonly property int iconSizeMedium: 32
    readonly property int iconSizeLarge: 48
    
    // ========================================
    // Animation Durations
    // ========================================
    
    readonly property int animationDurationFast: 100
    readonly property int animationDurationNormal: 200
    readonly property int animationDurationSlow: 300
    
    // ========================================
    // Helper Functions
    // ========================================
    
    /**
     * Returns a semi-transparent version of a color
     * @param color The base color
     * @param opacity Opacity value (0.0 - 1.0)
     * @return Color with adjusted opacity
     */
    function withOpacity(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }
    
    /**
     * Returns a lighter version of a color
     * @param color The base color
     * @param factor Lightening factor (0.0 - 1.0)
     * @return Lightened color
     */
    function lighter(color, factor) {
        return Qt.lighter(color, 1 + factor)
    }
    
    /**
     * Returns a darker version of a color
     * @param color The base color
     * @param factor Darkening factor (0.0 - 1.0)
     * @return Darkened color
     */
    function darker(color, factor) {
        return Qt.darker(color, 1 + factor)
    }
}
