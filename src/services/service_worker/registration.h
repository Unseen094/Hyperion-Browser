#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <memory>
#include <mutex>

namespace hyperion::services::service_worker {

// Service Worker registration states
enum class RegistrationState : uint8_t {
    NOT_STARTED,
    REGISTERING,
    INSTALLING,
    INSTALLED,
    ACTIVATING,
    ACTIVATED,
    REDUNDANT,
};

// Service Worker registration
class ServiceWorkerRegistration {
public:
    ServiceWorkerRegistration(const std::string& scope, const std::string& script_url)
        : scope_(scope), script_url_(script_url), state_(RegistrationState::NOT_STARTED),
          registration_time_(std::time(nullptr)) {}
    
    const std::string& scope() const { return scope_; }
    const std::string& script_url() const { return script_url_; }
    RegistrationState state() const { return state_; }
    
    void set_state(RegistrationState state) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = state;
    }
    
    time_t registration_time() const { return registration_time_; }
    time_t last_update_check_time() const { return last_update_check_time_; }
    
    void set_last_update_check_time(time_t time) {
        std::lock_guard<std::mutex> lock(mutex_);
        last_update_check_time_ = time;
    }
    
    // Navigation preload enabled
    bool navigation_preload() const { return navigation_preload_; }
    void enable_navigation_preload(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        navigation_preload_ = enabled;
    }
    
    // Sync event registration
    bool sync_enabled() const { return sync_registration_count_ > 0; }
    void add_sync_registration() {
        std::lock_guard<std::mutex> lock(mutex_);
        sync_registration_count_++;
    }
    void remove_sync_registration() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sync_registration_count_ > 0) {
            sync_registration_count_--;
        }
    }
    
    // Push subscription
    void set_push_subscription(const std::string& subscription) {
        std::lock_guard<std::mutex> lock(mutex_);
        push_subscription_ = subscription;
    }
    
    const std::string& push_subscription() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return push_subscription_;
    }
    
private:
    std::string scope_;
    std::string script_url_;
    RegistrationState state_;
    time_t registration_time_;
    time_t last_update_check_time_ = 0;
    bool navigation_preload_ = false;
    int sync_registration_count_ = 0;
    std::string push_subscription_;
    mutable std::mutex mutex_;
};

// Service Worker registration manager
class ServiceWorkerRegistrar {
public:
    // Register a service worker
    std::shared_ptr<ServiceWorkerRegistration> register_service_worker(
        const std::string& script_url,
        const std::string& scope,
        bool allow_update = true
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Remove existing registrations in this scope
        auto it = registrations_.find(scope);
        if (it != registrations_.end()) {
            if (allow_update) {
                cleanup_old_registrations(it->second, scope);
            } else {
                return nullptr; // Already exists and updates not allowed
            }
        }
        
        // Create new registration
        auto registration = std::make_shared<ServiceWorkerRegistration>(scope, script_url);
        registration->set_state(RegistrationState::REGISTERING);
        
        registrations_[scope] = registration;
        registration->set_last_update_check_time(std::time(nullptr));
        
        return registration;
    }
    
    // Find registration by scope
    std::shared_ptr<ServiceWorkerRegistration> get_registration(const std::string& scope) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registrations_.find(scope);
        if (it != registrations_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // Get all active registrations
    std::unordered_map<std::string, std::shared_ptr<ServiceWorkerRegistration>> 
    get_registrations() {
        std::lock_guard<std::mutex> lock(mutex_);
        return registrations_;
    }
    
    // Unregister
    bool unregister(const std::string& scope) {
        std::lock_guard<std::mutex> lock(mutex_);
        return registrations_.erase(scope) > 0;
    }
    
    // Check for updates
    void check_for_updates() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [scope, registration] : registrations_) {
            if (registration->state() == RegistrationState::ACTIVATED) {
                registration->set_last_update_check_time(std::time(nullptr));
                // In real implementation: fetch script and compare versions
            }
        }
    }
    
private:
    void cleanup_old_registrations(
        const std::shared_ptr<ServiceWorkerRegistration>& reg,
        const std::string& new_scope
    ) {
        // Implement cleanup logic - remove redundant registrations
        // in real implementation, this might remove old scripts
    }
    
    std::unordered_map<std::string, std::shared_ptr<ServiceWorkerRegistration>> registrations_;
    mutable std::mutex mutex_;
};

// Service Worker container
class ServiceWorkerContainer {
public:
    explicit ServiceWorkerContainer(ServiceWorkerRegistrar& registrar) : registrar_(registrar) {}
    
    // Register method accessible from JavaScript
    std::shared_ptr<ServiceWorkerRegistration> register_method(
        const std::string& script_url,
        const std::string& scope = "/"
    ) {
        return registrar_.register_service_worker(script_url, scope, true);
    }
    
    std::shared_ptr<ServiceWorkerRegistration> get_registration(const std::string& scope) {
        return registrar_.get_registration(scope);
    }
    
    void update() {
        registrar_.check_for_updates();
    }

private:
    ServiceWorkerRegistrar& registrar_;
};

} // namespace hyperion::services::service_worker