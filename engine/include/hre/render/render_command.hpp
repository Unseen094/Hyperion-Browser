#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

namespace hre::render {

enum class command_type : uint32_t {
    RECTANGLE = 1,
    ROUNDED_RECTANGLE = 2,
    TEXT = 3,
    IMAGE = 4,
    SHADOW = 5
};

struct render_command {
    command_type type;
    
    // Bounds/Dimensions
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    
    // Styling properties
    uint32_t bg_color = 0;          // AARRGGBB (or first gradient color)
    uint32_t bg_color2 = 0;         // second gradient color (0 = no gradient)
    uint32_t border_color = 0;      // AARRGGBB
    float border_width = 0.0f;
    float border_radius = 0.0f;
    
    // Box Shadow
    float shadow_offset_x = 0.0f;
    float shadow_offset_y = 0.0f;
    uint32_t shadow_color = 0;      // AARRGGBB
    float shadow_blur = 0.0f;
    
    // For text
    std::wstring text_content;
    std::wstring font_family;
    float font_size = 14.0f;
    uint32_t text_color = 0xFF000000; // Black
    
    // For images
    std::wstring image_src;
};

// Serialization helper
inline std::vector<uint8_t> serialize_frame(const std::vector<render_command>& commands) {
    std::vector<uint8_t> buffer;
    
    auto write_data = [&](const void* data, size_t size) {
        const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(data);
        buffer.insert(buffer.end(), byte_ptr, byte_ptr + size);
    };

    auto write_wstring = [&](const std::wstring& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        write_data(&len, sizeof(len));
        if (len > 0) {
            write_data(str.data(), len * sizeof(wchar_t));
        }
    };

    uint32_t num_commands = static_cast<uint32_t>(commands.size());
    write_data(&num_commands, sizeof(num_commands));

    for (const auto& cmd : commands) {
        uint32_t type_val = static_cast<uint32_t>(cmd.type);
        write_data(&type_val, sizeof(type_val));
        write_data(&cmd.x, sizeof(cmd.x));
        write_data(&cmd.y, sizeof(cmd.y));
        write_data(&cmd.width, sizeof(cmd.width));
        write_data(&cmd.height, sizeof(cmd.height));
        write_data(&cmd.bg_color, sizeof(cmd.bg_color));
        write_data(&cmd.bg_color2, sizeof(cmd.bg_color2));
        write_data(&cmd.border_color, sizeof(cmd.border_color));
        write_data(&cmd.border_width, sizeof(cmd.border_width));
        write_data(&cmd.border_radius, sizeof(cmd.border_radius));
        write_data(&cmd.shadow_offset_x, sizeof(cmd.shadow_offset_x));
        write_data(&cmd.shadow_offset_y, sizeof(cmd.shadow_offset_y));
        write_data(&cmd.shadow_color, sizeof(cmd.shadow_color));
        write_data(&cmd.shadow_blur, sizeof(cmd.shadow_blur));
        write_data(&cmd.font_size, sizeof(cmd.font_size));
        write_data(&cmd.text_color, sizeof(cmd.text_color));
        
        write_wstring(cmd.text_content);
        write_wstring(cmd.font_family);
        write_wstring(cmd.image_src);
    }
    
    return buffer;
}

// Deserialization helper
inline std::vector<render_command> deserialize_frame(const std::vector<uint8_t>& buffer) {
    std::vector<render_command> commands;
    if (buffer.size() < sizeof(uint32_t)) return commands;
    
    size_t offset = 0;
    auto read_data = [&](void* dest, size_t size) -> bool {
        if (offset + size > buffer.size()) return false;
        std::memcpy(dest, buffer.data() + offset, size);
        offset += size;
        return true;
    };

    auto read_wstring = [&](std::wstring& str) -> bool {
        uint32_t len = 0;
        if (!read_data(&len, sizeof(len))) return false;
        if (len > 0) {
            str.resize(len);
            if (!read_data(str.data(), len * sizeof(wchar_t))) return false;
        } else {
            str.clear();
        }
        return true;
    };

    uint32_t num_commands = 0;
    if (!read_data(&num_commands, sizeof(num_commands))) return commands;
    commands.reserve(num_commands);

    for (uint32_t i = 0; i < num_commands; ++i) {
        render_command cmd;
        uint32_t type_val = 0;
        if (!read_data(&type_val, sizeof(type_val))) break;
        cmd.type = static_cast<command_type>(type_val);
        
        if (!read_data(&cmd.x, sizeof(cmd.x))) break;
        if (!read_data(&cmd.y, sizeof(cmd.y))) break;
        if (!read_data(&cmd.width, sizeof(cmd.width))) break;
        if (!read_data(&cmd.height, sizeof(cmd.height))) break;
        if (!read_data(&cmd.bg_color, sizeof(cmd.bg_color))) break;
        if (!read_data(&cmd.bg_color2, sizeof(cmd.bg_color2))) break;
        if (!read_data(&cmd.border_color, sizeof(cmd.border_color))) break;
        if (!read_data(&cmd.border_width, sizeof(cmd.border_width))) break;
        if (!read_data(&cmd.border_radius, sizeof(cmd.border_radius))) break;
        if (!read_data(&cmd.shadow_offset_x, sizeof(cmd.shadow_offset_x))) break;
        if (!read_data(&cmd.shadow_offset_y, sizeof(cmd.shadow_offset_y))) break;
        if (!read_data(&cmd.shadow_color, sizeof(cmd.shadow_color))) break;
        if (!read_data(&cmd.shadow_blur, sizeof(cmd.shadow_blur))) break;
        if (!read_data(&cmd.font_size, sizeof(cmd.font_size))) break;
        if (!read_data(&cmd.text_color, sizeof(cmd.text_color))) break;
        
        if (!read_wstring(cmd.text_content)) break;
        if (!read_wstring(cmd.font_family)) break;
        if (!read_wstring(cmd.image_src)) break;
        
        commands.push_back(cmd);
    }
    
    return commands;
}

} // namespace hre::render
