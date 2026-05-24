#pragma once

#include <cmath>
#include <cstdint>
#include <array>
#include <vector>
#include <string>

namespace hre::graphics {

// ---- 2D Transform (CSS transform compatible) ------------------------------

struct matrix3x2 {
    float m[6] = {1, 0, 0, 1, 0, 0}; // [a, b, c, d, tx, ty]

    void identity() { m[0]=1; m[1]=0; m[2]=0; m[3]=1; m[4]=0; m[5]=0; }
    void translate(float tx, float ty) { m[4] += tx; m[5] += ty; }
    void scale(float sx, float sy) { m[0] *= sx; m[1] *= sx; m[2] *= sy; m[3] *= sy; }
    void rotate(float angle_rad);
    void skew(float ax, float ay);
    void multiply(const matrix3x2& other);

    float determinant() const { return m[0] * m[3] - m[1] * m[2]; }
    matrix3x2 inverse() const;

    void transform_point(float& x, float& y) const;
    void transform_rect(float& x, float& y, float& w, float& h) const;

    static matrix3x2 translation(float tx, float ty);
    static matrix3x2 scaling(float sx, float sy);
    static matrix3x2 rotation(float angle_rad);
    static matrix3x2 skew_x(float angle_rad);
    static matrix3x2 skew_y(float angle_rad);
};

// ---- 4x4 Matrix (3D transforms, WebGL, perspective) -----------------------

struct matrix4x4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    void identity();
    void zero();

    // Translation
    void translate(float tx, float ty, float tz);
    void translate_x(float tx) { translate(tx, 0, 0); }
    void translate_y(float ty) { translate(0, ty, 0); }
    void translate_z(float tz) { translate(0, 0, tz); }

    // Rotation
    void rotate_x(float angle_rad);
    void rotate_y(float angle_rad);
    void rotate_z(float angle_rad);
    void rotate(float angle_rad, float axis_x, float axis_y, float axis_z);

    // Scale
    void scale(float sx, float sy, float sz);
    void scale_uniform(float s) { scale(s, s, s); }

    // Skew
    void skew(float angle_x, float angle_y);

    // Perspective
    void perspective(float d);
    void frustum(float left, float right, float bottom, float top, float near, float far);
    void ortho(float left, float right, float bottom, float top, float near, float far);
    void look_at(float eye_x, float eye_y, float eye_z,
                 float center_x, float center_y, float center_z,
                 float up_x, float up_y, float up_z);

    // Multiply by another matrix (this = this * other)
    void multiply(const matrix4x4& other);

    // Multiply in place: this = other * this
    void premultiply(const matrix4x4& other);

    // Decompose to TRS
    struct decompose_result {
        float translate[3];
        float scale[3];
        float skew[2];
        float perspective[4];
        float quaternion[4]; // x, y, z, w
    };
    decompose_result decompose() const;

    // Transform a 3D point
    void transform_point(float& x, float& y, float& z) const;
    void transform_point_w(float& x, float& y, float& z, float& w) const;

    // Transform a 2D point (z=0, w=1, then project)
    void transform_point_2d(float& x, float& y) const;

    // Matrix properties
    float determinant() const;
    matrix4x4 inverse() const;
    matrix4x4 transpose() const;
    bool is_identity() const;
    bool is_affine() const;
    bool is_invertible() const { return determinant() != 0; }

    // Accessors (column-major for WebGL)
    const float* data() const { return m; }
    float* data() { return m; }

    float at(int row, int col) const { return m[col * 4 + row]; }
    void set(int row, int col, float v) { m[col * 4 + row] = v; }

    // Static constructors
    static matrix4x4 identity_matrix();
    static matrix4x4 translation_matrix(float tx, float ty, float tz);
    static matrix4x4 scaling_matrix(float sx, float sy, float sz);
    static matrix4x4 rotation_x_matrix(float angle_rad);
    static matrix4x4 rotation_y_matrix(float angle_rad);
    static matrix4x4 rotation_z_matrix(float angle_rad);
    static matrix4x4 rotation_axis_matrix(float angle_rad, float ax, float ay, float az);
    static matrix4x4 perspective_matrix(float fov_y_rad, float aspect, float near, float far);
    static matrix4x4 orthographic_matrix(float left, float right, float bottom, float top, float near, float far);
    static matrix4x4 look_at_matrix(float eye_x, float eye_y, float eye_z,
                                    float center_x, float center_y, float center_z,
                                    float up_x, float up_y, float up_z);
};

// ---- CSS 3D Transform Support --------------------------------------------

struct css_transform_function {
    enum type {
        MATRIX,
        MATRIX_3D,
        TRANSLATE,
        TRANSLATE_3D,
        TRANSLATE_X,
        TRANSLATE_Y,
        TRANSLATE_Z,
        SCALE,
        SCALE_3D,
        SCALE_X,
        SCALE_Y,
        SCALE_Z,
        ROTATE,
        ROTATE_3D,
        ROTATE_X,
        ROTATE_Y,
        ROTATE_Z,
        SKEW,
        SKEW_X,
        SKEW_Y,
        PERSPECTIVE
    } func_type;

    std::vector<float> params;
};

struct css_transform {
    std::vector<css_transform_function> functions;
    bool is_none() const { return functions.empty(); }
    void clear() { functions.clear(); }
    void add(const css_transform_function& f) { functions.push_back(f); }

    // Compute combined 4x4 matrix from transform list
    matrix4x4 compute_matrix() const;

    // Parse a CSS transform string
    static css_transform parse(const std::wstring& transform_string);
};

// ---- Quaternion ----------------------------------------------------------

struct quaternion {
    float x = 0, y = 0, z = 0, w = 1;

    void identity() { x = 0; y = 0; z = 0; w = 1; }
    void normalize();
    quaternion normalized() const;

    quaternion conjugate() const { return {-x, -y, -z, w}; }
    quaternion inverse() const;

    // Construct from axis-angle
    static quaternion from_axis_angle(float angle_rad, float ax, float ay, float az);
    static quaternion from_euler(float pitch_rad, float yaw_rad, float roll_rad);
    static quaternion from_matrix(const matrix4x4& mat);

    // Convert to rotation matrix
    matrix4x4 to_matrix() const;

    // Spherical linear interpolation
    static quaternion slerp(const quaternion& a, const quaternion& b, float t);

    quaternion operator*(const quaternion& other) const;
    quaternion operator*(float scalar) const;
    quaternion operator+(const quaternion& other) const;
};

// ---- Ray / Intersection ---------------------------------------------------

struct ray3 {
    float origin[3];
    float direction[3];
};

struct plane {
    float a, b, c, d; // ax + by + cz + d = 0
};

struct frustum_planes {
    plane planes[6]; // left, right, top, bottom, near, far

    void extract_from_matrix(const matrix4x4& view_projection);
    bool intersects_aabb(float min_x, float min_y, float min_z,
                         float max_x, float max_y, float max_z) const;
};

// ---- AABB (Axis-Aligned Bounding Box) ------------------------------------

struct aabb3 {
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;

    void reset() {
        min_x = min_y = min_z = 1e30f;
        max_x = max_y = max_z = -1e30f;
    }

    void extend(float x, float y, float z);
    void extend(const aabb3& other);
    bool contains(float x, float y, float z) const;
    bool intersects(const aabb3& other) const;
    aabb3 transform(const matrix4x4& mat) const;
};

// ---- Utility functions ----------------------------------------------------

namespace math_utils {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;

    inline float deg_to_rad(float deg) { return deg * DEG_TO_RAD; }
    inline float rad_to_deg(float rad) { return rad * RAD_TO_DEG; }

    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
    inline float clamp(float v, float min, float max) { return v < min ? min : (v > max ? max : v); }

    inline float sign(float v) { return v > 0 ? 1.0f : (v < 0 ? -1.0f : 0.0f); }
    inline float smoothstep(float edge0, float edge1, float x);

    float random_float(float min, float max);

    // Easing functions
    float ease_linear(float t);
    float ease_in_quad(float t);
    float ease_out_quad(float t);
    float ease_in_out_quad(float t);
    float ease_in_cubic(float t);
    float ease_out_cubic(float t);
    float ease_in_out_cubic(float t);
    float ease_in_elastic(float t);
    float ease_out_elastic(float t);
    float ease_in_back(float t);
    float ease_out_back(float t);
    float ease_in_bounce(float t);
    float ease_out_bounce(float t);
}

} // namespace hre::graphics
