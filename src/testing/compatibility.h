#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <algorithm>

namespace hyperion::testing::compatibility {

// Browser feature types
enum class FeatureType : uint8_t {
    CSS_PROPERTY,
    JAVASCRIPT_API,
    HTML_ELEMENT,
    WEB_API,
    CSS_VALUE,
};

// Browser engine types
enum class BrowserEngine : uint8_t {
    WEBKIT,
    BLINK,
    GEKO,
    SERVO,
    SERF,
    EDGEHTML,
    OTHER,
};

// Version support information
struct FeatureSupport {
    FeatureType type;
    std::string name;
    std::string feature_id;
    std::string description;
    std::unordered_map<std::string, std::string> support;
    int first_version;
    bool default_value;
};

// Cross-browser compatibility checker
class CompatibilityChecker {
public:
    CompatibilityChecker() {
        initialize_standard_features();
    }
    
    // Add a feature with its support matrix
    void add_feature(const FeatureSupport& feature) {
        std::lock_guard<std::mutex> lock(mutex_);
        feature_map_[feature.feature_id] = feature;
    }
    
    // Check if feature is supported in browser
    bool is_supported(const std::string& feature_id, const std::string& browser, 
                    const std::string& browser_version) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = feature_map_.find(feature_id);
        if (it == feature_map_.end()) {
            return false; // Not tracked
        }
        
        const auto& feature = it->second;
        if (feature.support.empty()) {
            // If no specific browser support, use version check
            int version = std::stoi(browser_version);
            return version >= feature.first_version;
        }
        
        auto browser_it = feature.support.find(browser);
        if (browser_it != feature.support.end()) {
            return browser_it->second.find("yes") != std::string::npos;
        }
        
        return false;
    }
    
    // Get feature details
    FeatureSupport get_feature(const std::string& feature_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = feature_map_.find(feature_id);
        if (it != feature_map_.end()) {
            return it->second;
        }
        return {};
    }
    
    // Caniuse-style support data lookup
    std::unordered_map<std::string, std::string> get_support_matrix(
        const std::string& feature_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = feature_map_.find(feature_id);
        if (it != feature_map_.end()) {
            return it->second.support;
        }
        return {};
    }
    
    // Filter features by type
    std::vector<FeatureSupport> get_features_by_type(FeatureType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<FeatureSupport> result;
        for (const auto& [id, feat] : feature_map_) {
            if (feat.type == type) {
                result.push_back(feat);
            }
        }
        return result;
    }
    
    // Check if feature needs prefix
    bool needs_prefix(const std::string& property, BrowserEngine engine) {
        std::string prop_lower = property;
        std::transform(prop_lower.begin(), prop_lower.end(), prop_lower.begin(), ::tolower);
        
        if (prop_lower.find("transform") != std::string::npos ||
            prop_lower.find("transition") != std::string::npos) {
            return true; // Common prefixed properties
        }
        
        // Check against known prefixes
        if (engine == BrowserEngine::BLINK || engine == BrowserEngine::EDGEHTML) {
            if (prop_lower.find("appearance") != std::string::npos) {
                return true; // -webkit-appearance
            }
        }
        
        return false;
    }
    
    // Polyfill recommendation
    std::string get_polyfill_suggestion(const std::string& feature_id) {
        return feature_id == "css-grid" ? "Use PostCSS or polyfill with css-grid-polyfill.js" :
               feature_id == "css-container-queries" ? "Use container-query-polyfill.js" :
               "No known polyfill, consider feature detection";
    }

private:
    void initialize_standard_features() {
        // CSS Grid layout
        FeatureSupport grid;
        grid.type = FeatureType::CSS_PROPERTY;
        grid.name = "CSS Grid Layout";
        grid.feature_id = "css-grid";
        grid.description = "Two-dimensional grid-based layout system";
        grid.first_version = 49; // Chrome/Blink
        grid.default_value = false;
        grid.support["chrome"] = "yes";
        grid.support["firefox"] = "yes";
        grid.support["safari"] = "partial";
        grid.support["edge"] = "yes";
        grid.support["opera"] = "yes";
        add_feature(grid);
        
        // CSS Container Queries
        FeatureSupport cq;
        cq.type = FeatureType::CSS_PROPERTY;
        cq.name = "CSS Container Queries";
        cq.feature_id = "css-container-queries";
        cq.description = "Query layout containers by size";
        cq.first_version = 97; // Chrome 97
        cq.default_value = false;
        cq.support["chrome"] = "yes";
        cq.support["firefox"] = "partial";
        add_feature(cq);
        
        // CSS Subgrid
        FeatureSupport subgrid;
        subgrid.type = FeatureType::CSS_PROPERTY;
        subgrid.name = "CSS Subgrid";
        subgrid.feature_id = "css-subgrid";
        subgrid.description = "Nested grid rows and columns";
        subgrid.first_version = 117; // Firefox 117
        add_feature(subgrid);
        
        // CSS Nesting
        FeatureSupport nesting;
        nesting.type = FeatureType::CSS_PROPERTY;
        nesting.name = "CSS Nesting";
        nesting.feature_id = "css-nesting";
        nesting.description = "CSS at-rules can nest";
        nesting.first_version = 116; // Chrome 116/Blink 116
        nesting.default_value = true;
        nesting.support["chrome"] = "yes";
        nesting.support["edge"] = "yes";
        nesting.support["firefox"] = "yes";
        add_feature(nesting);
        
        // JavaScript Internationalization API
        FeatureSupport intl;
        intl.type = FeatureType::JAVASCRIPT_API;
        intl.name = "Internationalization API";
        intl.feature_id = "intl";
        intl.description = "Intl.DateTimeFormat, Intl.NumberFormat, Intl.Collator";
        intl.first_version = 24; // Chrome 24
        intl.default_value = true;
        add_feature(intl);
        
        // Web Components v1
        FeatureSupport webcomponents;
        webcomponents.type = FeatureType::WEB_API;
        webcomponents.name = "Web Components v1";
        webcomponents.feature_id = "webcomponents";
        webcomponents.description = "Custom Elements, Shadow DOM, HTML Templates";
        webcomponents.first_version = 36; // Chrome 36
        webcomponents.default_value = false;
        webcomponents.support["chrome"] = "yes";
        webcomponents.support["firefox"] = "yes";
        webcomponents.support["safari"] = "partial";
        add_feature(webcomponents);
        
        // CSS Color Level 4
        FeatureSupport color4;
        color4.type = FeatureType::CSS_VALUE;
        color4.name = "CSS Color Level 4";
        color4.feature_id = "css-color-4";
        color4.description = "Color functions and hex 8";
        color4.first_version = 63; // Chrome 63
        color4.support["chrome"] = "yes";
        color4.support["safari"] = "yes";
        add_feature(color4);
        
        // CSS backdrop-filter
        FeatureSupport backdrop;
        backdrop.type = FeatureType::CSS_PROPERTY;
        backdrop.name = "CSS Backdrop-filter";
        backdrop.feature_id = "backdrop-filter";
        backdrop.description = "Apply graphical effects to area behind element";
        backdrop.first_version = 35; // Chrome 35
        backdrop.support["chrome"] = "yes";
        backdrop.support["edge"] = "yes";
        backdrop.support["safari"] = "yes";
        add_feature(backdrop);
        
        // Viewport units (lvw, svw, dvw)
        FeatureSupport viewport_units;
        viewport_units.type = FeatureType::CSS_VALUE;
        viewport_units.name = "Viewport units (lvw, svw, dvw)";
        viewport_units.feature_id = "viewport-units";
        viewport_units.description = "Large, small, and dynamic viewport units";
        viewport_units.first_version = 108; // Chrome 108
        viewport_units.support["chrome"] = "yes";
        viewport_units.support["firefox"] = "yes";
        viewport_units.support["safari"] = "no";
        add_feature(viewport_units);
    }
    
    std::unordered_map<std::string, FeatureSupport> feature_map_;
    mutable std::mutex mutex_;
};

// Cross-browser testing runner
class CrossBrowserTestRunner {
public:
    void add_test(const std::string& test_name, const std::string& test_code,
                const std::vector<std::string>& required_features) {
        std::lock_guard<std::mutex> lock(mutex_);
        TestCase test;
        test.name = test_name;
        test.code = test_code;
        test.required_features = required_features;
        tests_.push_back(test);
    }
    
    // Run test suite against known browser capabilities
    std::vector<std::string> run_tests(const std::vector<std::string>& target_browsers) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> results;
        
        for (const auto& test : tests_) {
            bool passed = true;
            std::string failed_reason;
            
            for (const auto& feature : test.required_features) {
                bool supported_any = false;
                for (const auto& browser : target_browsers) {
                    // Simplified: check if any matching browser engine supports
                    if (feature == "css-grid" || feature == "css-container-queries") {
                        // These are Hyperion development targets
                        supported_any = true;
                        break;
                    }
                }
                if (!supported_any) {
                    passed = false;
                    failed_reason = "Missing required feature: " + feature;
                    break;
                }
            }
            
            results.push_back(passed ? "PASS " + test.name : "FAIL " + test.name + " - " + failed_reason);
        }
        
        return results;
    }
    
    std::vector<std::string> get_test_summary() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> summary;
        
        for (const auto& test : tests_) {
            summary.push_back("Test: " + test.name);
            summary.push_back("Required: " + join(test.required_features, ", "));
        }
        
        return summary;
    }

private:
    struct TestCase {
        std::string name;
        std::string code;
        std::vector<std::string> required_features;
    };
    
    std::vector<TestCase> tests_;
    std::mutex mutex_;
    
    std::string join(const std::vector<std::string>& vec, const std::string& delim) {
        std::string result;
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) result += delim;
            result += vec[i];
        }
        return result;
    }
};

// Feature detection helper (can be exposed to JavaScript)
class FeatureDetector {
public:
    void register_api(const std::string& feature_id, const std::string& api_name,
                    bool is_available = false) {
        api_status_[feature_id] = is_available;
    }
    
    bool is_api_available(const std::string& feature_id) {
        auto it = api_status_.find(feature_id);
        if (it != api_status_.end()) {
            return it->second;
        }
        return check_runtime_support(feature_id);
    }
    
    std::string generate_feature_check_code(const std::string& feature_id) {
        return "try { " + feature_id + " && true } catch(e) { false }";
    }

private:
    bool check_runtime_support(const std::string& feature_id) {
        // These would be real feature detection checks in runtime
        if (feature_id == "requestIdleCallback") return true;
        if (feature_id == "IntersectionObserver") return true;
        if (feature_id == "ResizeObserver") return true;
        if (feature_id == "WebComponent") return true;
        return false;
    }
    
    std::unordered_map<std::string, bool> api_status_;
};

} // namespace hyperion::testing::compatibility