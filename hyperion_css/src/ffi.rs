// ============================================================
// Hyperion CSS Parser — C FFI Bridge
// ============================================================

use crate::tokenizer::CssTokenizer;
use crate::parser::CssParser;
use crate::stylesheet::Stylesheet;
use std::ffi::CStr;
use std::os::raw::c_char;

#[repr(C)]
pub struct HStylesheet {
    pub inner: *mut Stylesheet,
}

#[unsafe(no_mangle)]
pub extern "C" fn hcss_parse_stylesheet(css_utf8: *const c_char) -> *mut HStylesheet {
    if css_utf8.is_null() { return std::ptr::null_mut(); }
    
    let c_str = unsafe { CStr::from_ptr(css_utf8) };
    let r_str = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let tokenizer = CssTokenizer::new(r_str);
    let (tokens, diags) = tokenizer.tokenize();
    let mut parser = CssParser::new(tokens, diags);
    let stylesheet = parser.parse_stylesheet();

    let boxed = Box::new(HStylesheet {
        inner: Box::into_raw(Box::new(stylesheet)),
    });

    Box::into_raw(boxed)
}

#[unsafe(no_mangle)]
pub extern "C" fn hcss_free_stylesheet(handle: *mut HStylesheet) {
    if handle.is_null() { return; }
    unsafe {
        let h = Box::from_raw(handle);
        let _ = Box::from_raw(h.inner);
    }
}
