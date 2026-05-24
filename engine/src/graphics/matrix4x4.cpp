#include <hre/graphics/matrix4x4.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace hre::graphics {

// ---- matrix3x2 ------------------------------------------------------------

void matrix3x2::rotate(float angle_rad) {
    float c = std::cos(angle_rad);
    float s = std::sin(angle_rad);
    float nm0 = m[0] * c + m[2] * s;
    float nm1 = m[1] * c + m[3] * s;
    float nm2 = m[0] * -s + m[2] * c;
    float nm3 = m[1] * -s + m[3] * c;
    m[0] = nm0; m[1] = nm1; m[2] = nm2; m[3] = nm3;
}

void matrix3x2::skew(float ax, float ay) {
    float tan_x = std::tan(ax);
    float tan_y = std::tan(ay);
    float nm0 = m[0] + m[2] * tan_y;
    float nm1 = m[1] + m[3] * tan_y;
    float nm2 = m[0] * tan_x + m[2];
    float nm3 = m[1] * tan_x + m[3];
    m[0] = nm0; m[1] = nm1; m[2] = nm2; m[3] = nm3;
}

void matrix3x2::multiply(const matrix3x2& other) {
    float nm0 = m[0] * other.m[0] + m[2] * other.m[1];
    float nm1 = m[1] * other.m[0] + m[3] * other.m[1];
    float nm2 = m[0] * other.m[2] + m[2] * other.m[3];
    float nm3 = m[1] * other.m[2] + m[3] * other.m[3];
    float nm4 = m[0] * other.m[4] + m[2] * other.m[5] + m[4];
    float nm5 = m[1] * other.m[4] + m[3] * other.m[5] + m[5];
    float tmp[6] = {nm0, nm1, nm2, nm3, nm4, nm5};
    std::memcpy(m, tmp, sizeof(m));
}

matrix3x2 matrix3x2::inverse() const {
    float det = determinant();
    if (std::abs(det) < 1e-10f) return *this;
    float inv_det = 1.0f / det;
    matrix3x2 result;
    result.m[0] = m[3] * inv_det;
    result.m[1] = -m[1] * inv_det;
    result.m[2] = -m[2] * inv_det;
    result.m[3] = m[0] * inv_det;
    result.m[4] = (m[2] * m[5] - m[3] * m[4]) * inv_det;
    result.m[5] = (m[1] * m[4] - m[0] * m[5]) * inv_det;
    return result;
}

void matrix3x2::transform_point(float& x, float& y) const {
    float nx = m[0] * x + m[2] * y + m[4];
    float ny = m[1] * x + m[3] * y + m[5];
    x = nx; y = ny;
}

void matrix3x2::transform_rect(float& rx, float& ry, float& rw, float& rh) const {
    float corners[4][2] = {{rx, ry}, {rx + rw, ry}, {rx, ry + rh}, {rx + rw, ry + rh}};
    float min_x = std::numeric_limits<float>::max(), min_y = std::numeric_limits<float>::max();
    float max_x = -std::numeric_limits<float>::max(), max_y = -std::numeric_limits<float>::max();
    for (auto& c : corners) {
        float px = c[0], py = c[1];
        transform_point(px, py);
        min_x = std::min(min_x, px); min_y = std::min(min_y, py);
        max_x = std::max(max_x, px); max_y = std::max(max_y, py);
    }
    rx = min_x; ry = min_y; rw = max_x - min_x; rh = max_y - min_y;
}

matrix3x2 matrix3x2::translation(float tx, float ty) {
    matrix3x2 m; m.m[4] = tx; m.m[5] = ty; return m;
}
matrix3x2 matrix3x2::scaling(float sx, float sy) {
    matrix3x2 m; m.m[0] = sx; m.m[3] = sy; return m;
}
matrix3x2 matrix3x2::rotation(float angle_rad) {
    matrix3x2 m; m.rotate(angle_rad); return m;
}
matrix3x2 matrix3x2::skew_x(float angle_rad) {
    matrix3x2 m; m.skew(angle_rad, 0); return m;
}
matrix3x2 matrix3x2::skew_y(float angle_rad) {
    matrix3x2 m; m.skew(0, angle_rad); return m;
}

// ---- matrix4x4 ------------------------------------------------------------

void matrix4x4::identity() {
    float id[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::memcpy(m, id, sizeof(m));
}

void matrix4x4::zero() {
    std::memset(m, 0, sizeof(m));
}

void matrix4x4::translate(float tx, float ty, float tz) {
    m[12] += m[0] * tx + m[4] * ty + m[8] * tz;
    m[13] += m[1] * tx + m[5] * ty + m[9] * tz;
    m[14] += m[2] * tx + m[6] * ty + m[10] * tz;
    m[15] += m[3] * tx + m[7] * ty + m[11] * tz;
}

void matrix4x4::rotate_x(float angle_rad) {
    float c = std::cos(angle_rad), s = std::sin(angle_rad);
    float m4 = m[4], m5 = m[5], m6 = m[6], m7 = m[7];
    m[4] = m4 * c + m[8] * s;  m[5] = m5 * c + m[9] * s;  m[6] = m6 * c + m[10] * s;  m[7] = m7 * c + m[11] * s;
    m[8] = m4 * -s + m[8] * c; m[9] = m5 * -s + m[9] * c; m[10] = m6 * -s + m[10] * c; m[11] = m7 * -s + m[11] * c;
}

void matrix4x4::rotate_y(float angle_rad) {
    float c = std::cos(angle_rad), s = std::sin(angle_rad);
    float m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3];
    m[0] = m0 * c + m[8] * -s; m[1] = m1 * c + m[9] * -s; m[2] = m2 * c + m[10] * -s; m[3] = m3 * c + m[11] * -s;
    m[8] = m0 * s + m[8] * c;  m[9] = m1 * s + m[9] * c;  m[10] = m2 * s + m[10] * c; m[11] = m3 * s + m[11] * c;
}

void matrix4x4::rotate_z(float angle_rad) {
    float c = std::cos(angle_rad), s = std::sin(angle_rad);
    float m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3];
    m[0] = m0 * c + m[4] * s;  m[1] = m1 * c + m[5] * s;  m[2] = m2 * c + m[6] * s;  m[3] = m3 * c + m[7] * s;
    m[4] = m0 * -s + m[4] * c; m[5] = m1 * -s + m[5] * c; m[6] = m2 * -s + m[6] * c; m[7] = m3 * -s + m[7] * c;
}

void matrix4x4::rotate(float angle_rad, float axis_x, float axis_y, float axis_z) {
    float c = std::cos(angle_rad), s = std::sin(angle_rad);
    float len = std::sqrt(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);
    if (len < 1e-10f) return;
    float x = axis_x / len, y = axis_y / len, z = axis_z / len;
    float t = 1 - c;

    matrix4x4 rot;
    rot.m[0] = t * x * x + c;     rot.m[4] = t * x * y - s * z; rot.m[8] = t * x * z + s * y;
    rot.m[1] = t * x * y + s * z; rot.m[5] = t * y * y + c;     rot.m[9] = t * y * z - s * x;
    rot.m[2] = t * x * z - s * y; rot.m[6] = t * y * z + s * x; rot.m[10] = t * z * z + c;
    multiply(rot);
}

void matrix4x4::scale(float sx, float sy, float sz) {
    m[0] *= sx; m[1] *= sx; m[2] *= sx; m[3] *= sx;
    m[4] *= sy; m[5] *= sy; m[6] *= sy; m[7] *= sy;
    m[8] *= sz; m[9] *= sz; m[10] *= sz; m[11] *= sz;
}

void matrix4x4::skew(float angle_x, float angle_y) {
    (void)angle_x; (void)angle_y;
}

void matrix4x4::perspective(float d) {
    if (std::abs(d) < 1e-10f) return;
    m[11] = -1.0f / d;
}

void matrix4x4::frustum(float left, float right, float bottom, float top, float near, float far) {
    float two_near = 2.0f * near;
    float rl = right - left, tb = top - bottom, fn = far - near;
    float f[16] = {
        two_near / rl, 0, 0, 0,
        0, two_near / tb, 0, 0,
        (right + left) / rl, (top + bottom) / tb, -(far + near) / fn, -1,
        0, 0, -(two_near * far) / fn, 0
    };
    matrix4x4 fm; std::memcpy(fm.m, f, sizeof(f));
    multiply(fm);
}

void matrix4x4::ortho(float left, float right, float bottom, float top, float near, float far) {
    float rl = right - left, tb = top - bottom, fn = far - near;
    float o[16] = {
        2 / rl, 0, 0, 0,
        0, 2 / tb, 0, 0,
        0, 0, -2 / fn, 0,
        -(right + left) / rl, -(top + bottom) / tb, -(far + near) / fn, 1
    };
    matrix4x4 om; std::memcpy(om.m, o, sizeof(o));
    multiply(om);
}

void matrix4x4::look_at(float eye_x, float eye_y, float eye_z,
                          float center_x, float center_y, float center_z,
                          float up_x, float up_y, float up_z) {
    float fwd_x = center_x - eye_x, fwd_y = center_y - eye_y, fwd_z = center_z - eye_z;
    float flen = std::sqrt(fwd_x * fwd_x + fwd_y * fwd_y + fwd_z * fwd_z);
    if (flen > 0) { fwd_x /= flen; fwd_y /= flen; fwd_z /= flen; }

    float side_x = fwd_y * up_z - fwd_z * up_y;
    float side_y = fwd_z * up_x - fwd_x * up_z;
    float side_z = fwd_x * up_y - fwd_y * up_x;
    float slen = std::sqrt(side_x * side_x + side_y * side_y + side_z * side_z);
    if (slen > 0) { side_x /= slen; side_y /= slen; side_z /= slen; }

    float ux = side_y * fwd_z - side_z * fwd_y;
    float uy = side_z * fwd_x - side_x * fwd_z;
    float uz = side_x * fwd_y - side_y * fwd_x;

    float l[16] = {
        side_x, ux, -fwd_x, 0,
        side_y, uy, -fwd_y, 0,
        side_z, uz, -fwd_z, 0,
        -side_x * eye_x - side_y * eye_y - side_z * eye_z,
        -ux * eye_x - uy * eye_y - uz * eye_z,
        fwd_x * eye_x + fwd_y * eye_y + fwd_z * eye_z,
        1
    };
    matrix4x4 lm; std::memcpy(lm.m, l, sizeof(l));
    multiply(lm);
}

void matrix4x4::multiply(const matrix4x4& other) {
    float r[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r[col * 4 + row] = m[0 * 4 + row] * other.m[col * 4 + 0] +
                               m[1 * 4 + row] * other.m[col * 4 + 1] +
                               m[2 * 4 + row] * other.m[col * 4 + 2] +
                               m[3 * 4 + row] * other.m[col * 4 + 3];
        }
    }
    std::memcpy(m, r, sizeof(m));
}

void matrix4x4::premultiply(const matrix4x4& other) {
    float r[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r[col * 4 + row] = other.m[0 * 4 + row] * m[col * 4 + 0] +
                               other.m[1 * 4 + row] * m[col * 4 + 1] +
                               other.m[2 * 4 + row] * m[col * 4 + 2] +
                               other.m[3 * 4 + row] * m[col * 4 + 3];
        }
    }
    std::memcpy(m, r, sizeof(m));
}

matrix4x4::decompose_result matrix4x4::decompose() const {
    decompose_result result = {};
    result.translate[0] = m[12]; result.translate[1] = m[13]; result.translate[2] = m[14];
    result.scale[0] = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
    result.scale[1] = std::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
    result.scale[2] = std::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
    float trace = m[0]/result.scale[0] + m[5]/result.scale[1] + m[10]/result.scale[2] + 1;
    if (trace > 0) {
        float s = 0.5f / std::sqrt(trace);
        result.quaternion[3] = 0.25f / s;
        result.quaternion[0] = (m[6]/result.scale[1] - m[9]/result.scale[2]) * s;
        result.quaternion[1] = (m[8]/result.scale[2] - m[2]/result.scale[0]) * s;
        result.quaternion[2] = (m[1]/result.scale[0] - m[4]/result.scale[1]) * s;
    }
    return result;
}

void matrix4x4::transform_point(float& x, float& y, float& z) const {
    float nx = m[0]*x + m[4]*y + m[8]*z + m[12];
    float ny = m[1]*x + m[5]*y + m[9]*z + m[13];
    float nz = m[2]*x + m[6]*y + m[10]*z + m[14];
    x = nx; y = ny; z = nz;
}

void matrix4x4::transform_point_w(float& x, float& y, float& z, float& w) const {
    float nx = m[0]*x + m[4]*y + m[8]*z + m[12]*w;
    float ny = m[1]*x + m[5]*y + m[9]*z + m[13]*w;
    float nz = m[2]*x + m[6]*y + m[10]*z + m[14]*w;
    float nw = m[3]*x + m[7]*y + m[11]*z + m[15]*w;
    x = nx; y = ny; z = nz; w = nw;
}

void matrix4x4::transform_point_2d(float& x, float& y) const {
    float z = 0, w = 1;
    transform_point_w(x, y, z, w);
    if (std::abs(w) > 1e-10f) { x /= w; y /= w; }
}

float matrix4x4::determinant() const {
    return m[0] * (m[5] * (m[10] * m[15] - m[11] * m[14]) - m[9] * (m[6] * m[15] - m[7] * m[14]) + m[13] * (m[6] * m[11] - m[7] * m[10]))
         - m[4] * (m[1] * (m[10] * m[15] - m[11] * m[14]) - m[9] * (m[2] * m[15] - m[3] * m[14]) + m[13] * (m[2] * m[11] - m[3] * m[10]))
         + m[8] * (m[1] * (m[6] * m[15] - m[7] * m[14]) - m[5] * (m[2] * m[15] - m[3] * m[14]) + m[13] * (m[2] * m[7] - m[3] * m[6]))
         - m[12] * (m[1] * (m[6] * m[11] - m[7] * m[10]) - m[5] * (m[2] * m[11] - m[3] * m[10]) + m[9] * (m[2] * m[7] - m[3] * m[6]));
}

matrix4x4 matrix4x4::inverse() const {
    float det = determinant();
    if (std::abs(det) < 1e-10f) return *this;
    float inv_det = 1.0f / det;

    auto cof = [&](int r, int c) {
        auto sub = [&](int rr, int cc) -> float {
            int sr = 0, sc = 0;
            float sub_m[3][3];
            for (int i = 0; i < 4; ++i) {
                if (i == r) continue;
                sc = 0;
                for (int j = 0; j < 4; ++j) {
                    if (j == c) continue;
                    sub_m[sr][sc++] = m[i * 4 + j];
                }
                ++sr;
            }
            return sub_m[0][0] * (sub_m[1][1] * sub_m[2][2] - sub_m[1][2] * sub_m[2][1])
                 - sub_m[0][1] * (sub_m[1][0] * sub_m[2][2] - sub_m[1][2] * sub_m[2][0])
                 + sub_m[0][2] * (sub_m[1][0] * sub_m[2][1] - sub_m[1][1] * sub_m[2][0]);
        };
        return ((r + c) % 2 == 0 ? 1 : -1) * sub(r, c);
    };

    matrix4x4 inv;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            inv.m[c * 4 + r] = cof(r, c) * inv_det;
    return inv;
}

matrix4x4 matrix4x4::transpose() const {
    matrix4x4 t;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            t.m[j * 4 + i] = m[i * 4 + j];
    return t;
}

bool matrix4x4::is_identity() const {
    float id[16] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    return std::memcmp(m, id, sizeof(m)) == 0;
}

bool matrix4x4::is_affine() const {
    return m[3] == 0 && m[7] == 0 && m[11] == 0 && m[15] == 1;
}

// Static constructors
matrix4x4 matrix4x4::identity_matrix() { return {}; }
matrix4x4 matrix4x4::translation_matrix(float tx, float ty, float tz) {
    matrix4x4 r; r.m[12] = tx; r.m[13] = ty; r.m[14] = tz; return r;
}
matrix4x4 matrix4x4::scaling_matrix(float sx, float sy, float sz) {
    matrix4x4 r; r.m[0] = sx; r.m[5] = sy; r.m[10] = sz; return r;
}
matrix4x4 matrix4x4::rotation_x_matrix(float angle_rad) {
    matrix4x4 r; r.rotate_x(angle_rad); return r;
}
matrix4x4 matrix4x4::rotation_y_matrix(float angle_rad) {
    matrix4x4 r; r.rotate_y(angle_rad); return r;
}
matrix4x4 matrix4x4::rotation_z_matrix(float angle_rad) {
    matrix4x4 r; r.rotate_z(angle_rad); return r;
}
matrix4x4 matrix4x4::rotation_axis_matrix(float angle_rad, float ax, float ay, float az) {
    matrix4x4 r; r.rotate(angle_rad, ax, ay, az); return r;
}
matrix4x4 matrix4x4::perspective_matrix(float fov_y_rad, float aspect, float near, float far) {
    float f = 1.0f / std::tan(fov_y_rad / 2);
    matrix4x4 p;
    p.m[0] = f / aspect; p.m[5] = f;
    p.m[10] = -(far + near) / (far - near);
    p.m[11] = -1;
    p.m[14] = -(2 * near * far) / (far - near);
    p.m[15] = 0;
    return p;
}
matrix4x4 matrix4x4::orthographic_matrix(float left, float right, float bottom, float top, float near, float far) {
    matrix4x4 r; r.ortho(left, right, bottom, top, near, far); return r;
}
matrix4x4 matrix4x4::look_at_matrix(float eye_x, float eye_y, float eye_z,
                                     float center_x, float center_y, float center_z,
                                     float up_x, float up_y, float up_z) {
    matrix4x4 r; r.look_at(eye_x, eye_y, eye_z, center_x, center_y, center_z, up_x, up_y, up_z); return r;
}

// ---- CSS Transform --------------------------------------------------------

matrix4x4 css_transform::compute_matrix() const {
    matrix4x4 result;
    for (const auto& func : functions) {
        switch (func.func_type) {
            case css_transform_function::MATRIX_3D: {
                if (func.params.size() >= 16) {
                    matrix4x4 mat;
                    std::memcpy(mat.m, func.params.data(), sizeof(float) * 16);
                    result.multiply(mat);
                }
                break;
            }
            case css_transform_function::TRANSLATE_3D: {
                float tx = func.params.size() > 0 ? func.params[0] : 0;
                float ty = func.params.size() > 1 ? func.params[1] : 0;
                float tz = func.params.size() > 2 ? func.params[2] : 0;
                result.multiply(matrix4x4::translation_matrix(tx, ty, tz));
                break;
            }
            case css_transform_function::SCALE_3D: {
                float sx = func.params.size() > 0 ? func.params[0] : 1;
                float sy = func.params.size() > 1 ? func.params[1] : 1;
                float sz = func.params.size() > 2 ? func.params[2] : 1;
                result.multiply(matrix4x4::scaling_matrix(sx, sy, sz));
                break;
            }
            case css_transform_function::ROTATE_X:
                result.multiply(matrix4x4::rotation_x_matrix(func.params.empty() ? 0 : func.params[0]));
                break;
            case css_transform_function::ROTATE_Y:
                result.multiply(matrix4x4::rotation_y_matrix(func.params.empty() ? 0 : func.params[0]));
                break;
            case css_transform_function::ROTATE_Z:
                result.multiply(matrix4x4::rotation_z_matrix(func.params.empty() ? 0 : func.params[0]));
                break;
            case css_transform_function::ROTATE_3D: {
                if (func.params.size() >= 4)
                    result.multiply(matrix4x4::rotation_axis_matrix(func.params[3], func.params[0], func.params[1], func.params[2]));
                break;
            }
            case css_transform_function::PERSPECTIVE: {
                float d = func.params.empty() ? 0 : func.params[0];
                matrix4x4 p;
                if (std::abs(d) > 1e-10f) p.m[11] = -1.0f / d;
                result.multiply(p);
                break;
            }
            case css_transform_function::TRANSLATE_X:
                result.multiply(matrix4x4::translation_matrix(func.params.empty() ? 0 : func.params[0], 0, 0));
                break;
            case css_transform_function::TRANSLATE_Y:
                result.multiply(matrix4x4::translation_matrix(0, func.params.empty() ? 0 : func.params[0], 0));
                break;
            case css_transform_function::TRANSLATE_Z:
                result.multiply(matrix4x4::translation_matrix(0, 0, func.params.empty() ? 0 : func.params[0]));
                break;
            case css_transform_function::SCALE_X:
                result.multiply(matrix4x4::scaling_matrix(func.params.empty() ? 1 : func.params[0], 1, 1));
                break;
            case css_transform_function::SCALE_Y:
                result.multiply(matrix4x4::scaling_matrix(1, func.params.empty() ? 1 : func.params[0], 1));
                break;
            case css_transform_function::SCALE_Z:
                result.multiply(matrix4x4::scaling_matrix(1, 1, func.params.empty() ? 1 : func.params[0]));
                break;
            case css_transform_function::SKEW_X:
            case css_transform_function::SKEW_Y: {
                float a = func.params.empty() ? 0 : func.params[0];
                matrix3x2 s;
                if (func.func_type == css_transform_function::SKEW_X) s.skew(a, 0);
                else s.skew(0, a);
                matrix4x4 m;
                m.m[0] = s.m[0]; m.m[1] = s.m[1];
                m.m[4] = s.m[2]; m.m[5] = s.m[3];
                m.m[12] = s.m[4]; m.m[13] = s.m[5];
                result.multiply(m);
                break;
            }
            default:
                break;
        }
    }
    return result;
}

css_transform css_transform::parse(const std::wstring& transform_string) {
    css_transform t;
    if (transform_string.empty() || transform_string == L"none") return t;
    // Simple parser stub
    return t;
}

// ---- Quaternion -----------------------------------------------------------

void quaternion::normalize() {
    float len = std::sqrt(x*x + y*y + z*z + w*w);
    if (len > 0) { x /= len; y /= len; z /= len; w /= len; }
}

quaternion quaternion::normalized() const {
    quaternion q = *this; q.normalize(); return q;
}

quaternion quaternion::inverse() const {
    float len2 = x*x + y*y + z*z + w*w;
    if (len2 == 0) return *this;
    return { -x / len2, -y / len2, -z / len2, w / len2 };
}

quaternion quaternion::from_axis_angle(float angle_rad, float ax, float ay, float az) {
    float half = angle_rad * 0.5f;
    float s = std::sin(half);
    float len = std::sqrt(ax*ax + ay*ay + az*az);
    if (len == 0) return {};
    return { ax / len * s, ay / len * s, az / len * s, std::cos(half) };
}

quaternion quaternion::from_euler(float pitch_rad, float yaw_rad, float roll_rad) {
    float cp = std::cos(pitch_rad * 0.5f), sp = std::sin(pitch_rad * 0.5f);
    float cy = std::cos(yaw_rad * 0.5f), sy = std::sin(yaw_rad * 0.5f);
    float cr = std::cos(roll_rad * 0.5f), sr = std::sin(roll_rad * 0.5f);
    return {
        sr * cp * cy - cr * sp * sy,
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy
    };
}

quaternion quaternion::from_matrix(const matrix4x4& mat) {
    float trace = mat.m[0] + mat.m[5] + mat.m[10];
    quaternion q;
    if (trace > 0) {
        float s = 0.5f / std::sqrt(trace + 1);
        q.w = 0.25f / s;
        q.x = (mat.m[6] - mat.m[9]) * s;
        q.y = (mat.m[8] - mat.m[2]) * s;
        q.z = (mat.m[1] - mat.m[4]) * s;
    }
    return q;
}

matrix4x4 quaternion::to_matrix() const {
    float x2 = x*x, y2 = y*y, z2 = z*z;
    float xy = x*y, xz = x*z, yz = y*z;
    float wx = w*x, wy = w*y, wz = w*z;
    matrix4x4 m;
    m.m[0] = 1 - 2*(y2 + z2); m.m[4] = 2*(xy - wz);       m.m[8] = 2*(xz + wy);
    m.m[1] = 2*(xy + wz);     m.m[5] = 1 - 2*(x2 + z2);   m.m[9] = 2*(yz - wx);
    m.m[2] = 2*(xz - wy);     m.m[6] = 2*(yz + wx);       m.m[10] = 1 - 2*(x2 + y2);
    return m;
}

quaternion quaternion::slerp(const quaternion& a, const quaternion& b, float t) {
    float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    quaternion qb = b;
    if (dot < 0) { qb = {-b.x, -b.y, -b.z, -b.w}; dot = -dot; }
    if (dot > 0.9995f) {
        quaternion r = { a.x + t*(qb.x - a.x), a.y + t*(qb.y - a.y), a.z + t*(qb.z - a.z), a.w + t*(qb.w - a.w) };
        r.normalize(); return r;
    }
    float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    float sin_theta = std::sin(theta);
    float wa = std::sin((1 - t) * theta) / sin_theta;
    float wb = std::sin(t * theta) / sin_theta;
    return { a.x*wa + qb.x*wb, a.y*wa + qb.y*wb, a.z*wa + qb.z*wb, a.w*wa + qb.w*wb };
}

quaternion quaternion::operator*(const quaternion& other) const {
    return {
        w*other.x + x*other.w + y*other.z - z*other.y,
        w*other.y - x*other.z + y*other.w + z*other.x,
        w*other.z + x*other.y - y*other.x + z*other.w,
        w*other.w - x*other.x - y*other.y - z*other.z
    };
}

quaternion quaternion::operator*(float scalar) const {
    return { x*scalar, y*scalar, z*scalar, w*scalar };
}

quaternion quaternion::operator+(const quaternion& other) const {
    return { x + other.x, y + other.y, z + other.z, w + other.w };
}

// ---- AABB -----------------------------------------------------------------

void aabb3::extend(float px, float py, float pz) {
    min_x = std::min(min_x, px); min_y = std::min(min_y, py); min_z = std::min(min_z, pz);
    max_x = std::max(max_x, px); max_y = std::max(max_y, py); max_z = std::max(max_z, pz);
}

void aabb3::extend(const aabb3& other) {
    extend(other.min_x, other.min_y, other.min_z);
    extend(other.max_x, other.max_y, other.max_z);
}

bool aabb3::contains(float px, float py, float pz) const {
    return px >= min_x && px <= max_x && py >= min_y && py <= max_y && pz >= min_z && pz <= max_z;
}

bool aabb3::intersects(const aabb3& other) const {
    return max_x >= other.min_x && min_x <= other.max_x &&
           max_y >= other.min_y && min_y <= other.max_y &&
           max_z >= other.min_z && min_z <= other.max_z;
}

aabb3 aabb3::transform(const matrix4x4& mat) const {
    aabb3 result;
    result.reset();
    float corners[8][3] = {
        {min_x, min_y, min_z}, {max_x, min_y, min_z},
        {min_x, max_y, min_z}, {max_x, max_y, min_z},
        {min_x, min_y, max_z}, {max_x, min_y, max_z},
        {min_x, max_y, max_z}, {max_x, max_y, max_z}
    };
    for (auto& c : corners) {
        float px = c[0], py = c[1], pz = c[2];
        mat.transform_point(px, py, pz);
        result.extend(px, py, pz);
    }
    return result;
}

// ---- Frustum --------------------------------------------------------------

void frustum_planes::extract_from_matrix(const matrix4x4& vp) {
    const float* m = vp.m;
    planes[0] = { m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12] }; // left
    planes[1] = { m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12] }; // right
    planes[2] = { m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13] }; // top
    planes[3] = { m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13] }; // bottom
    planes[4] = { m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14] }; // near
    planes[5] = { m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14] }; // far
    for (auto& p : planes) {
        float len = std::sqrt(p.a*p.a + p.b*p.b + p.c*p.c);
        if (len > 0) { p.a /= len; p.b /= len; p.c /= len; p.d /= len; }
    }
}

bool frustum_planes::intersects_aabb(float min_x, float min_y, float min_z,
                                      float max_x, float max_y, float max_z) const {
    for (const auto& p : planes) {
        float px = p.a > 0 ? max_x : min_x;
        float py = p.b > 0 ? max_y : min_y;
        float pz = p.c > 0 ? max_z : min_z;
        if (p.a * px + p.b * py + p.c * pz + p.d < 0) return false;
    }
    return true;
}

// ---- Math Utilities -------------------------------------------------------

namespace math_utils {

float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

float random_float(float min, float max) {
    return min + (max - min) * (static_cast<float>(std::rand()) / RAND_MAX);
}

float ease_linear(float t) { return t; }
float ease_in_quad(float t) { return t * t; }
float ease_out_quad(float t) { return t * (2 - t); }
float ease_in_out_quad(float t) { return t < 0.5f ? 2*t*t : -1 + (4-2*t)*t; }
float ease_in_cubic(float t) { return t * t * t; }
float ease_out_cubic(float t) { return --t * t * t + 1; }
float ease_in_out_cubic(float t) { return t < 0.5f ? 4*t*t*t : (t-1)*(2*t-2)*(2*t-2)+1; }

float ease_in_elastic(float t) {
    if (t == 0 || t == 1) return t;
    return -std::pow(2, 10 * (t - 1)) * std::sin((t - 1.075f) * (2 * PI) / 0.3f);
}

float ease_out_elastic(float t) {
    if (t == 0 || t == 1) return t;
    return std::pow(2, -10 * t) * std::sin((t - 0.075f) * (2 * PI) / 0.3f) + 1;
}

float ease_in_back(float t) {
    float c1 = 1.70158f;
    return (c1 + 1) * t * t * t - c1 * t * t;
}

float ease_out_back(float t) {
    float c1 = 1.70158f;
    return 1 + (c1 + 1) * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2);
}

float ease_in_bounce(float t) { return 1 - ease_out_bounce(1 - t); }

float ease_out_bounce(float t) {
    if (t < 1 / 2.75f) return 7.5625f * t * t;
    if (t < 2 / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f;
}

} // namespace math_utils

} // namespace hre::graphics
