// ============================================================
// Hyperion CSS Parser — Main Runner (Testing Milestone)
// ============================================================

use hyperion_css::tokenizer::CssTokenizer;
use hyperion_css::parser::CssParser;
use hyperion_css::rules::Rule;

fn main() {
    let input = r#"
        body {
            color: red;
            margin: 10px;
        }
        
        div p:hover {
            color: blue;
        }

        @media screen {
            body { font-size: 16px; }
        }

        [type="text"] {
            border: 1px solid black;
        }
        
        /* Malformed CSS */
        broken {
            color ;;
            font-size: 14px;
        }
    "#;

    println!("Hyperion CSS Parser - Milestone 1 Runner\n");
    println!("Input:\n{}", input);

    // 1. Tokenize
    let tokenizer = CssTokenizer::new(input);
    let (tokens, mut diags) = tokenizer.tokenize();

    // 2. Parse AST
    let mut parser = CssParser::new(tokens, diags);
    let stylesheet = parser.parse_stylesheet();

    // 3. Output
    println!("Output:");
    println!("Stylesheet");
    
    for rule in stylesheet.rules {
        print_rule(&rule, 0);
    }

    println!("\nDiagnostics:");
    for diag in parser.diagnostics.get_all() {
        println!("  [{:?}] Line {}: {}", diag.level, diag.span.start.line, diag.message);
    }
}

fn print_rule(rule: &Rule, indent: usize) {
    let pad = " ".repeat(indent);
    match rule {
        Rule::StyleRule { selectors, declarations } => {
            let mut sel_strs = Vec::new();
            for complex in &selectors.selectors {
                let mut complex_str = String::new();
                for part in &complex.parts {
                    match part {
                        hyperion_css::selectors::SelectorPart::Compound(c) => {
                            for simple in c {
                                match simple {
                                    hyperion_css::selectors::SimpleSelector::TagName(t) => complex_str.push_str(t),
                                    hyperion_css::selectors::SimpleSelector::Id(id) => complex_str.push_str(&format!("#{}", id)),
                                    hyperion_css::selectors::SimpleSelector::Class(c) => complex_str.push_str(&format!(".{}", c)),
                                    hyperion_css::selectors::SimpleSelector::PseudoClass(p) => complex_str.push_str(&format!(":{}", p)),
                                    hyperion_css::selectors::SimpleSelector::Attribute { name, operator, value } => {
                                        complex_str.push('[');
                                        complex_str.push_str(name);
                                        if let Some(op) = operator {
                                            complex_str.push_str(op);
                                            if let Some(v) = value {
                                                complex_str.push_str(&format!("\"{}\"", v));
                                            }
                                        }
                                        complex_str.push(']');
                                    }
                                    _ => complex_str.push('?'),
                                }
                            }
                        }
                        hyperion_css::selectors::SelectorPart::Combinator(comb) => {
                            match comb {
                                hyperion_css::selectors::Combinator::Child => complex_str.push_str(" > "),
                                hyperion_css::selectors::Combinator::Descendant => complex_str.push(' '),
                                hyperion_css::selectors::Combinator::NextSibling => complex_str.push_str(" + "),
                                hyperion_css::selectors::Combinator::SubsequentSibling => complex_str.push_str(" ~ "),
                            }
                        }
                    }
                }
                sel_strs.push(complex_str);
            }
            
            println!("{} └── Rule", pad);
            println!("{}      ├── Selector({})", pad, sel_strs.join(", "));
            println!("{}      └── Declarations", pad);
            
            for (i, decl) in declarations.iter().enumerate() {
                let branch = if i == declarations.len() - 1 { "└──" } else { "├──" };
                let imp = if decl.important { " !important" } else { "" };
                println!("{}           {} {}: {}{}", pad, branch, decl.property, decl.value.to_css_string(), imp);
            }
        }
        Rule::AtRule { name, prelude, block } => {
            println!("{} └── AtRule(@{})", pad, name);
            if !prelude.is_empty() {
                println!("{}      ├── Prelude({})", pad, prelude);
            }
            if let Some(inner) = block {
                println!("{}      └── Block", pad);
                for r in inner {
                    print_rule(r, indent + 6);
                }
            }
        }
    }
}
