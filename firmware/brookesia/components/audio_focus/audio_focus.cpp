#include "audio_focus.hpp"

namespace audio_focus {

manager::manager()
{
    mutex_ = xSemaphoreCreateMutex();
}

manager &manager::instance()
{
    static manager focus_manager;
    return focus_manager;
}

client manager::select_next() const
{
    for (int index = static_cast<int>(client::max) - 1; index >= 0; --index) {
        if (entries_[static_cast<std::size_t>(index)].requested) {
            return static_cast<client>(index);
        }
    }
    return client::max;
}

bool manager::activate_next()
{
    while (true) {
        client next_client = select_next();
        if (next_client == client::max) {
            active_client_ = client::max;
            return true;
        }
        entry &next_entry = entries_[static_cast<std::size_t>(next_client)];
        if (next_entry.client_callbacks.activate == nullptr ||
                next_entry.client_callbacks.activate(next_entry.client_callbacks.user_data)) {
            active_client_ = next_client;
            return true;
        }
        next_entry.requested = false;
    }
}

bool manager::request(client requested_client, const callbacks &client_callbacks)
{
    if (mutex_ == nullptr || requested_client == client::max ||
            xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    entry &requested_entry = entries_[static_cast<std::size_t>(requested_client)];
    requested_entry.requested = true;
    requested_entry.client_callbacks = client_callbacks;
    if (active_client_ != client::max && requested_client <= active_client_) {
        xSemaphoreGive(mutex_);
        return true;
    }
    if (active_client_ != client::max) {
        entry &active_entry = entries_[static_cast<std::size_t>(active_client_)];
        if (active_entry.client_callbacks.suspend != nullptr &&
                !active_entry.client_callbacks.suspend(active_entry.client_callbacks.user_data)) {
            xSemaphoreGive(mutex_);
            return false;
        }
    }
    active_client_ = client::max;
    bool result = activate_next();
    xSemaphoreGive(mutex_);
    return result;
}

bool manager::abandon(client abandoned_client)
{
    if (mutex_ == nullptr || abandoned_client == client::max ||
            xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    entry &abandoned_entry = entries_[static_cast<std::size_t>(abandoned_client)];
    abandoned_entry.requested = false;
    if (active_client_ != abandoned_client) {
        xSemaphoreGive(mutex_);
        return true;
    }
    if (abandoned_entry.client_callbacks.suspend != nullptr &&
            !abandoned_entry.client_callbacks.suspend(abandoned_entry.client_callbacks.user_data)) {
        abandoned_entry.requested = true;
        xSemaphoreGive(mutex_);
        return false;
    }
    active_client_ = client::max;
    bool result = activate_next();
    xSemaphoreGive(mutex_);
    return result;
}

bool manager::is_active(client queried_client)
{
    if (mutex_ == nullptr || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    bool active = active_client_ == queried_client;
    xSemaphoreGive(mutex_);
    return active;
}

} // namespace audio_focus
