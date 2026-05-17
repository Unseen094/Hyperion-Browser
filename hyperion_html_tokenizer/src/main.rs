use hyperion_html_tokenizer::tokenize;

fn main() {
    let args: Vec<String> = std::env::args().collect();

    let source = if args.len() > 1 {
        std::fs::read_to_string(&args[1]).expect("Failed to read file")
    } else {
        // Built-in demo
        r#"<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8"/>
    <title>Hyperion</title>
  </head>
  <body>
    <!-- Main content -->
    <div id="root" class="container">
      <h1>Hello, Hyperion!</h1>
      <p>A <strong>native</strong> browser engine.</p>
    </div>
  </body>
</html>"#.to_string()
    };

    let (tokens, diags) = tokenize(&source);

    println!("=== Token Stream ===");
    for tok in &tokens {
        println!("  [{:<8}] {} @ {}", tok.kind_name(), tok, tok.span().start);
    }

    println!("\n=== Summary ===");
    println!("  Tokens    : {}", tokens.len());
    println!("  Diagnostics: {}", diags.len());

    if !diags.is_empty() {
        println!("\n=== Diagnostics ===");
        for d in diags.iter() {
            println!("  {}", d);
        }
    }
}
