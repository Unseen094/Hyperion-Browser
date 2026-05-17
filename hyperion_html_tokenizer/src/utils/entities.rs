// ============================================================
// Hyperion HTML Tokenizer — Entity Decoder
// ============================================================

pub fn decode_html_entities(input: &str) -> String {
    if !input.contains('&') {
        return input.to_string();
    }

    let mut result = String::with_capacity(input.len());
    let mut chars = input.chars().peekable();

    while let Some(c) = chars.next() {
        if c == '&' {
            let mut entity = String::new();
            let mut is_valid = false;
            
            while let Some(&next) = chars.peek() {
                if next == ';' {
                    chars.next(); // consume ';'
                    is_valid = true;
                    break;
                } else if next.is_ascii_alphanumeric() || next == '#' {
                    entity.push(chars.next().unwrap());
                } else {
                    break;
                }
            }

            if is_valid {
                match entity.as_str() {
                    "amp" => result.push('&'),
                    "lt" => result.push('<'),
                    "gt" => result.push('>'),
                    "quot" => result.push('"'),
                    "apos" => result.push('\''),
                    "nbsp" => result.push('\u{00A0}'),
                    "copy" => result.push('©'),
                    "reg" => result.push('®'),
                    "trade" => result.push('™'),
                    _ if entity.starts_with("#x") => {
                        // Hex numeric entity
                        if let Ok(val) = u32::from_str_radix(&entity[2..], 16) {
                            if let Some(decoded) = std::char::from_u32(val) {
                                result.push(decoded);
                                continue;
                            }
                        }
                        result.push_str(&format!("&{};", entity));
                    }
                    _ if entity.starts_with("#") => {
                        // Decimal numeric entity
                        if let Ok(val) = entity[1..].parse::<u32>() {
                            if let Some(decoded) = std::char::from_u32(val) {
                                result.push(decoded);
                                continue;
                            }
                        }
                        result.push_str(&format!("&{};", entity));
                    }
                    _ => {
                        // Unknown entity
                        result.push_str(&format!("&{};", entity));
                    }
                }
            } else {
                result.push('&');
                result.push_str(&entity);
            }
        } else {
            result.push(c);
        }
    }

    result
}
