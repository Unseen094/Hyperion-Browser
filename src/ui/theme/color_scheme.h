#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace hyperion::ui::theme {

// System color scheme detection
enum class ColorScheme : uint8_t {
    SYSTEM,  // Follow system setting
    LIGHT,
    DARK,
    HIGH_CONTRAST,
};

class ColorSchemeManager {
public:
    ColorSchemeManager() {
        detect_system_scheme();
    }
    
    // Set current color scheme
    void set_scheme(ColorScheme scheme) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_scheme_ = scheme;
    }
    
    // Get current color scheme
    ColorScheme current_scheme() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (force_scheme_ != SYSTEM) {
            return force_scheme_;
        }
        return current_scheme_;
    }
    
    // Force scheme for testing
    void force_scheme(ColorScheme scheme) {
        std::lock_guard<std::mutex> lock(mutex_);
        force_scheme_ = scheme;
    }
    
    // Update on system change
    void system_scheme_changed() {
        std::lock_guard<std::mutex> lock(mutex_);
        detect_system_scheme();
    }
    
    // Get CSS variables for current scheme
    std::unordered_map<std::string, std::string> generate_css_variables() const {
        std::lock_guard<std::mutex> lock(mutex_);
        ColorScheme target = (force_scheme_ != SYSTEM) ? force_scheme_ : current_scheme_;
        
        switch (target) {
            case DARK:
                return dark_css_variables();
            case LIGHT:
                return light_css_variables();
            case HIGH_CONTRAST:
                return high_contrast_variables();
            default:
                return system_scheme_variables();
        }
    }
    
    // Generate media query detection
    std::string generate_media_query() {
        switch (current_scheme_) {
            case DARK:
                return "(prefers-color-scheme: dark)";
            case LIGHT:
                return "(prefers-color-scheme: light)";
            case HIGH_CONTRAST:
                return "(prefers-contrast: more)";
            default:
                return "(prefers-color-scheme: no-preference)";
        }
    }
    
    // Status string for debugging
    std::string scheme_status() const {
        switch (current_scheme()) {
            case SYSTEM: return "System";
            case LIGHT: return "Light";
            case DARK: return "Dark";
            case HIGH_CONTRAST: return "High Contrast";
            default: return "Unknown";
        }
    }

private:
    void detect_system_scheme() {
        #ifdef _WIN32
        // Query Windows registry for system theme
        // Implementation would use Windows API to check registry key:
        // HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize
        // AppsUseLightTheme = 0 for dark
        current_scheme_ = LIGHT; // Default
        #elif __linux__
        // Query GNOME/KDE GTK theme detection
        // Would check org.gnome.desktop.interface.color-scheme
        current_scheme_ = DARK;
        #else
        current_scheme_ = SYSTEM;
        #endif
    }
    
    std::unordered_map<std::string, std::string> dark_css_variables() {
        return {
            {"--background", "#121212"},
            {"--text", "#e0e0e0"},
            {"--primary", "#bb86fc"},
            {"--primary-variant", "#3700b3"},
            {"--secondary", "#03dac6"},
            {"--surface", "#1e1e1e"},
            {"--error", "#cf6679"}
        };
    }
    
    std::unordered_map<std::string, std::string> light_css_variables() {
        return {
            {"--background", "#ffffff"},
            {"--text", "#000000"},
            {"--primary", "#6200ee"},
            {"--primary-variant", "#3700b3"},
            {"--secondary", "#03dac6"},
            {"--surface", "#f5f5f5"},
            {"--error", "#b00020"}
        };
    }
    
    std::unordered_map<std::string, std::string> high_contrast_variables() {
        return {
            {"--background", "#000000"},
            {"--text", "#ffffff"},
            {"--primary", "#ffff00"},
            {"--primary-variant", "#ffffff"},
            {"--secondary", "#00ff00"},
            {"--surface", "#000000"},
            {"--error", "#ff0000"}
        };
    }
    
    std::unordered_map<std::string, std::string> system_scheme_variables() {
        // Use system scheme from environment variables
        #ifdef _WIN32
        return dark_css_variables(); // Default fallback
        #elif __linux__
        return dark_css_variables();
        #else
        return light_css_variables();
        #endif
    }
    
    ColorScheme current_scheme_;
    ColorScheme force_scheme_ = SYSTEM;
    mutable std::mutex mutex_;
};

} // namespace hyperion::ui::theme