#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace hre::mathml {

enum class math_element_type {
    UNKNOWN,
    MATH,
    MI,
    MN,
    MO,
    MROW,
    MRow,
    MSUPER,
    MSUB,
    MSDOWN,
    MSTACK,
    MLONGDIV,
    MTABLE,
    MTABLECELL,
    MTABLE_ROW,
    MLTR,
    MFRAC,
    MSQRT,
    MENCLOSED,
    MSTYLE,
    MERROR,
    MPPHANTOM,
    MGAP,
    MSLINE,
    MSPACE,
    MACTION
};

struct math_layout_box {
    float ascent = 0;
    float descent = 0;
    float width = 0;
    float italic_correction = 0;
};

struct math_element {
    math_element_type type = math_element_type::UNKNOWN;
    std::wstring text_content;
    std::vector<std::unique_ptr<math_element>> children;
    std::map<std::wstring, std::wstring> attributes;
    math_layout_box metrics;

    std::wstring get_attribute(const std::wstring& name) const {
        auto it = attributes.find(name);
        return (it != attributes.end()) ? it->second : L"";
    }
};

class mathml_renderer {
public:
    mathml_renderer();
    ~mathml_renderer();

    bool parse_from_string(const std::wstring& mathml_content);
    void render(void* context);

    math_layout_box get_metrics() const { return m_root_metrics; }
    const math_element* root() const { return m_root.get(); }

private:
    std::unique_ptr<math_element> m_root;
    math_layout_box m_root_metrics;
    void* m_render_context = nullptr;

    math_layout_box layout_element(const math_element& elem);
    void render_element(const math_element& elem, void* context);
    void apply_italic_correction(math_element& elem);
};

} // namespace hre::mathml