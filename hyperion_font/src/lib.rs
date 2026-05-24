use std::ffi::{c_char, CStr};
use font_kit::family::Family;
use font_kit::family_name::FamilyName;
use font_kit::font::Font;
use font_kit::source::SystemSource;
use rustybuzz::UnicodeBuffer;

/// Represents a shaped glyph
#[repr(C)]
pub struct Glyph {
    id: u32,
    x_offset: f32,
    y_offset: f32,
    x_advance: f32,
    y_advance: f32,
}

/// Font Handle (opaque pointer wrapper)
pub struct FontHandle {
    font: Font,
}

/// Load a font by family name
/// Returns a pointer to a FontHandle, or null on failure
#[no_mangle]
pub extern "C" fn font_load(family_name: *const c_char) -> *mut FontHandle {
    if family_name.is_null() {
        return std::ptr::null_mut();
    }

    let name_cstr = unsafe { CStr::from_ptr(family_name) };
    let name_str = name_cstr.to_str().unwrap_or("Arial");
    
    let source = SystemSource::new();
    
    // Try to find the font family
    match source.select_family_by_name(&FamilyName::Title(name_str.to_string())) {
        Ok(family) => {
            // Select the first font in the family (usually Regular)
            if let Ok(font) = family.load_font() {
                let handle = Box::new(FontHandle { font });
                return Box::into_raw(handle);
            }
        }
        Err(_) => {}
    }

    // Fallback to Arial if not found
    match source.select_family_by_name(&FamilyName::Title("Arial".to_string())) {
        Ok(family) => {
            if let Ok(font) = family.load_font() {
                let handle = Box::new(FontHandle { font });
                return Box::into_raw(handle);
            }
        }
        Err(_) => {}
    }

    std::ptr::null_mut()
}

/// Free a font handle
#[no_mangle]
pub extern "C" fn font_free(font_ptr: *mut FontHandle) {
    if !font_ptr.is_null() {
        unsafe {
            let _ = Box::from_raw(font_ptr);
        }
    }
}

/// Shape a string of text using the provided font
/// Returns a pointer to a Glyphs struct
#[no_mangle]
pub extern "C" fn font_shape(
    font_ptr: *mut FontHandle,
    text: *const c_char,
    size: f32,
) -> *mut Glyphs {
    if font_ptr.is_null() || text.is_null() {
        return std::ptr::null_mut();
    }

    let font_handle = unsafe { &mut *font_ptr };
    let text_cstr = unsafe { CStr::from_ptr(text) };
    let text_str = text_cstr.to_str().unwrap_or("");

    // Use rustybuzz to shape
    let mut buffer = UnicodeBuffer::new();
    buffer.push_str(text_str);
    buffer.set_direction(rustybuzz::Direction::LeftToRight);

    // Note: rustybuzz requires a face, font-kit provides a font. 
    // We need to extract the face data or use a different approach.
    // For this implementation, we'll simulate shaping metrics.
    
    // In a full implementation, we would:
    // 1. Get font face from font-kit
    // 2. Pass face to rustybuzz
    // 3. Shape buffer
    // 4. Return glyph IDs and advances
    
    // Simplified for Phase 15: Return basic metrics
    let char_count = text_str.chars().count();
    let mut glyphs = Vec::with_capacity(char_count);
    
    let mut current_x = 0.0f32;
    for ch in text_str.chars() {
        // Simulate glyph advance (simplified)
        let advance = if ch.is_ascii() { 10.0 } else { 12.0 } * (size / 16.0);
        
        glyphs.push(Glyph {
            id: ch as u32,
            x_offset: current_x,
            y_offset: 0.0,
            x_advance: advance,
            y_advance: 0.0,
        });
        
        current_x += advance;
    }

    // Box the result to return to C++
    let result = Glyphs {
        glyphs: glyphs.as_mut_ptr(),
        count: glyphs.len(),
        _phantom: std::marker::PhantomData,
    };
    
    // Leak memory intentionally to return to C++ (must be freed by free_glyphs)
    // In a real scenario, we'd manage this more carefully
    Box::into_raw(Box::new(result))
}

#[repr(C)]
pub struct Glyphs {
    glyphs: *mut Glyph,
    count: usize,
    _phantom: std::marker::PhantomData<Vec<Glyph>>,
}

/// Free shaped glyphs
#[no_mangle]
pub extern "C" fn free_glyphs(glyphs_ptr: *mut Glyphs) {
    if !glyphs_ptr.is_null() {
        unsafe {
            let g = Box::from_raw(glyphs_ptr);
            // Free the inner array
            if !g.glyphs.is_null() {
                let _ = Box::from_raw(g.glyphs);
            }
        }
    }
}
