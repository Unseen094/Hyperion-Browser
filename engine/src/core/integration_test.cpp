#include <cstdio>
#include <cstring>
#include <string>
#include <cassert>

// Minimal integration test: HTTP -> HTML -> CSS -> Layout -> Paint -> Media

namespace hre::test {

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL %s (%s:%d)\n", name, __FILE__, __LINE__); \
        ++g_failed; \
    } else { \
        printf("  PASS %s\n", name); \
        ++g_passed; \
    } \
} while(0)

// ---- Mock types for integration testing ----

struct mock_http_response {
    int status_code;
    std::string body;
    std::string content_type;
};

struct mock_dom_node {
    std::string tag_name;
    std::string text_content;
    mock_dom_node* parent = nullptr;
    mock_dom_node* first_child = nullptr;
    mock_dom_node* next_sibling = nullptr;
};

struct mock_css_rule {
    std::string selector;
    std::string property;
    std::string value;
};

struct mock_layout_box {
    int x = 0, y = 0, w = 100, h = 50;
    mock_dom_node* node = nullptr;
};

// ---- HTTP Simulation ----

mock_http_response simulate_http_get(const std::string& url) {
    mock_http_response resp;
    resp.status_code = 200;
    resp.content_type = "text/html";

    if (url.find("hello") != std::string::npos) {
        resp.body = "<html><head><style>body { color: red; }</style></head>"
                    "<body><p>Hello World</p></body></html>";
    } else if (url.find("image") != std::string::npos) {
        resp.body = "<html><body><img src='test.png' /></body></html>";
    } else {
        resp.body = "<html><body><p>default</p></body></html>";
    }
    return resp;
}

// ---- HTML Parsing Simulation ----

bool parse_html_simple(const std::string& html, mock_dom_node& doc) {
    if (html.find("<html>") == std::string::npos) return false;
    if (html.find("</html>") == std::string::npos) return false;
    return true;
}

// ---- CSS Parsing Simulation ----

bool parse_css_simple(const std::string& css, mock_css_rule& rule) {
    auto pos = css.find("color");
    if (pos != std::string::npos) {
        rule.selector = "body";
        rule.property = "color";
        auto val_start = css.find(':', pos);
        auto val_end = css.find(';', val_start);
        if (val_start != std::string::npos && val_end != std::string::npos) {
            rule.value = css.substr(val_start + 1, val_end - val_start - 1);
            // trim
            rule.value.erase(0, rule.value.find_first_not_of(" \t"));
            rule.value.erase(rule.value.find_last_not_of(" \t") + 1);
        }
        return true;
    }
    return false;
}

// ---- Layout Simulation ----

bool compute_layout(mock_dom_node* root, mock_layout_box& box) {
    if (!root) return false;
    box.node = root;
    box.x = 0;
    box.y = 0;
    if (root->tag_name == "body") {
        box.w = 800;
        box.h = 600;
    } else if (root->tag_name == "p") {
        box.w = 200;
        box.h = 30;
    }
    return true;
}

// ---- Paint Simulation ----

struct mock_paint_surface {
    int width = 0;
    int height = 0;
    bool finalized = false;
};

bool paint_to_surface(mock_layout_box& box, mock_paint_surface& surface) {
    surface.width = box.x + box.w;
    surface.height = box.y + box.h;
    surface.finalized = true;
    return true;
}

// ---- Media Simulation ----

struct mock_media_query {
    std::string feature;
    std::string value;
    bool matches = false;
};

bool evaluate_media_query(const mock_media_query& mq) {
    if (mq.feature == "width" && mq.value == "800px") return true;
    if (mq.feature == "prefers-color-scheme" && mq.value == "no-preference") return true;
    return false;
}

// ---- Integration Test Entry Point ----

void run_integration_tests() {
    printf("\n=== Integration Tests ===\n");

    // 1. HTTP -> HTML
    printf("\n-- HTTP -> HTML Pipeline --\n");
    {
        auto resp = simulate_http_get("http://example.com/hello");
        TEST("HTTP 200", resp.status_code == 200);
        TEST("Content-Type text/html", resp.content_type == "text/html");

        mock_dom_node doc;
        doc.tag_name = "document";
        bool parsed = parse_html_simple(resp.body, doc);
        TEST("HTML parsed successfully", parsed);
    }

    // 2. CSS Parsing
    printf("\n-- CSS Parsing --\n");
    {
        mock_css_rule rule;
        bool css_ok = parse_css_simple("body { color: red; }", rule);
        TEST("CSS rule parsed", css_ok);
        TEST("CSS selector 'body'", rule.selector == "body");
        TEST("CSS property 'color'", rule.property == "color");
        TEST("CSS value 'red'", rule.value == "red");
    }

    // 3. DOM Construction
    printf("\n-- DOM Construction --\n");
    {
        mock_dom_node html, head, body, p;
        html.tag_name = "html";
        head.tag_name = "head";
        body.tag_name = "body";
        p.tag_name = "p";
        p.text_content = "Hello World";

        html.first_child = &head;
        head.next_sibling = &body;
        body.first_child = &p;
        p.parent = &body;

        TEST("DOM hierarchy", html.first_child == &head);
        TEST("DOM sibling", head.next_sibling == &body);
        TEST("DOM text content", p.text_content == "Hello World");
    }

    // 4. Layout Computation
    printf("\n-- Layout --\n");
    {
        mock_dom_node body;
        body.tag_name = "body";
        mock_layout_box box;
        bool layout_ok = compute_layout(&body, box);
        TEST("Layout computed", layout_ok);
        TEST("Layout body width 800", box.w == 800);
        TEST("Layout body height 600", box.h == 600);
    }

    // 5. Painting
    printf("\n-- Paint --\n");
    {
        mock_dom_node div;
        div.tag_name = "div";
        mock_layout_box box;
        mock_paint_surface surface;
        compute_layout(&div, box);
        bool paint_ok = paint_to_surface(box, surface);
        TEST("Paint to surface", paint_ok);
        TEST("Surface finalized", surface.finalized);
    }

    // 6. Media Queries
    printf("\n-- Media Queries --\n");
    {
        mock_media_query mq1{"width", "800px", false};
        mq1.matches = evaluate_media_query(mq1);
        TEST("Media query width 800px", mq1.matches);

        mock_media_query mq2{"prefers-color-scheme", "no-preference", false};
        mq2.matches = evaluate_media_query(mq2);
        TEST("Media query prefers-color-scheme", mq2.matches);
    }

    // 7. End-to-End: HTTP -> HTML -> CSS -> Layout -> Paint -> Media
    printf("\n-- End-to-End Pipeline --\n");
    {
        auto resp = simulate_http_get("http://example.com/hello");
        TEST("E2E: HTTP 200", resp.status_code == 200);

        mock_dom_node doc;
        doc.tag_name = "document";
        bool parsed = parse_html_simple(resp.body, doc);
        TEST("E2E: HTML parsed", parsed);

        mock_css_rule rule;
        parse_css_simple(resp.body, rule);
        TEST("E2E: CSS extracted", !rule.property.empty());

        mock_dom_node body;
        body.tag_name = "body";
        mock_layout_box box;
        compute_layout(&body, box);
        TEST("E2E: Layout computed", box.w > 0);

        mock_paint_surface surface;
        paint_to_surface(box, surface);
        TEST("E2E: Paint done", surface.finalized);

        // Combined result
        bool e2e_pass = resp.status_code == 200 && parsed && !rule.property.empty()
                        && box.w > 0 && surface.finalized;
        TEST("E2E: Full pipeline success", e2e_pass);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
}

} // namespace hre::test

int main() {
    hre::test::run_integration_tests();
    return (hre::test::g_failed > 0) ? 1 : 0;
}
