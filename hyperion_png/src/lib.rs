use std::io::Cursor;
use std::ffi::c_void;

#[repr(C)]
pub struct PngImage {
    pub width: u32,
    pub height: u32,
    pub pixels: *mut u8,
    pub len: usize,
}

#[no_mangle]
pub extern "C" fn decode_png(data: *const u8, len: usize) -> Option<PngImage> {
    let buf = unsafe { std::slice::from_raw_parts(data, len) };
    let cursor = Cursor::new(buf);
    let decoder = png::Decoder::new(cursor);
    let mut reader = decoder.read_info().ok()?;
    
    let mut buf = vec![0; reader.output_buffer_size()];
    reader.next_frame(&mut buf).ok()?;
    
    let pixels = unsafe {
        libc::malloc(buf.len()) as *mut u8
    };
    if pixels.is_null() {
        return None;
    }
    unsafe {
        std::ptr::copy_nonoverlapping(buf.as_ptr(), pixels, buf.len());
    }
    
    Some(PngImage {
        width: reader.info().width,
        height: reader.info().height,
        pixels,
        len: buf.len(),
    })
}

#[no_mangle]
pub extern "C" fn free_png_data(data: PngImage) {
    if !data.pixels.is_null() {
        unsafe {
            libc::free(data.pixels as *mut c_void);
        }
    }
}
