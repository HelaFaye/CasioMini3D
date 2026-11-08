#include "types.hpp"

Quaternion::Quaternion() : x(0), y(0), z(0), w(FLOAT_TO_FIXED(1.0f)) {
    UpdateTrigCache();
}

Quaternion::~Quaternion() {}

Quaternion::Quaternion(fixed_t x, fixed_t y, fixed_t z, fixed_t w) : x(x), y(y), z(z), w(w) {
    UpdateTrigCache();
}

Quaternion::Quaternion(float x, float y, float z, float w) {
    Set(x, y, z, w);
}

Quaternion::Quaternion(Vector3<fixed_t> eulerAngles) {
    Set(eulerAngles);
}

Quaternion::Quaternion(Vector3<float> eulerAngles) {
    Set(eulerAngles);
}

void Quaternion::Set(fixed_t x, fixed_t y, fixed_t z, fixed_t w) {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
    UpdateTrigCache();
}

void Quaternion::Set(float x, float y, float z, float w) {
    Set(FLOAT_TO_FIXED(x), FLOAT_TO_FIXED(y), FLOAT_TO_FIXED(z), FLOAT_TO_FIXED(w));
}

void Quaternion::Set(Vector3<fixed_t> euler) {
    fixed_t half = FLOAT_TO_FIXED(0.5f);
    fixed_t cy = fcos_approx(fmul(euler.y, half));
    fixed_t sy = fsin_approx(fmul(euler.y, half));
    fixed_t cp = fcos_approx(fmul(euler.x, half));
    fixed_t sp = fsin_approx(fmul(euler.x, half));
    fixed_t cr = fcos_approx(fmul(euler.z, half));
    fixed_t sr = fsin_approx(fmul(euler.z, half));

    w = fmul(cr, fmul(cp, cy)) + fmul(sr, fmul(sp, sy));
    x = fmul(sr, fmul(cp, cy)) - fmul(cr, fmul(sp, sy));
    y = fmul(cr, fmul(sp, cy)) + fmul(sr, fmul(cp, sy));
    z = fmul(cr, fmul(cp, sy)) - fmul(sr, fmul(sp, cy));

    UpdateTrigCache();
}

void Quaternion::Set(Vector3<float> euler) {
    Set(Vector3<fixed_t>{FLOAT_TO_FIXED(euler.x), FLOAT_TO_FIXED(euler.y), FLOAT_TO_FIXED(euler.z)});
}

void Quaternion::UpdateTrigCache() {
    Vector3<fixed_t> euler = ToEulerAngles();
    cos = {
        fcos_approx(euler.x),
        fcos_approx(euler.y),
        fcos_approx(euler.z)
    };
    sin = {
        fsin_approx(euler.x),
        fsin_approx(euler.y),
        fsin_approx(euler.z)
    };
}

Vector3<fixed_t> Quaternion::ToEulerAngles() const {
    Vector3<fixed_t> angles;

    fixed_t sinr_cosp = fmul(FLOAT_TO_FIXED(2), fmul(w, x) + fmul(y, z));
    fixed_t cosr_cosp = FLOAT_TO_FIXED(1) - fmul(FLOAT_TO_FIXED(2), fmul(x, x) + fmul(y, y));
    angles.z = fatan2_approx(sinr_cosp, cosr_cosp);

    fixed_t sinp = fmul(FLOAT_TO_FIXED(2), fmul(w, y) - fmul(z, x));
    if (abs(sinp) >= FLOAT_TO_FIXED(1))
        angles.x = fcopysign(FLOAT_TO_FIXED(M_PI_2), sinp);
    else
        angles.x = fasin_approx(sinp);

    fixed_t siny_cosp = fmul(FLOAT_TO_FIXED(2), fmul(w, z) + fmul(x, y));
    fixed_t cosy_cosp = FLOAT_TO_FIXED(1) - fmul(FLOAT_TO_FIXED(2), fmul(y, y) + fmul(z, z));
    angles.y = fatan2_approx(siny_cosp, cosy_cosp);

    return angles;
}

Quaternion Quaternion::Identity() {
    return Quaternion(0, 0, 0, FLOAT_TO_FIXED(1.0f));
}

Quaternion Quaternion::AngleAxis(float angleRad, Vector3<float> axis) {
    float half = angleRad * 0.5f;
    float s = sinf(half);
    return Quaternion(
        FLOAT_TO_FIXED(axis.x * s),
        FLOAT_TO_FIXED(axis.y * s),
        FLOAT_TO_FIXED(axis.z * s),
        FLOAT_TO_FIXED(cosf(half))
    );
}

Quaternion Quaternion::AngleAxis(fixed_t angleRad, Vector3<fixed_t> axis) {
    fixed_t half = angleRad / 2;
    fixed_t s = fsin_approx(half);
    return Quaternion(
        fmul(axis.x, s),
        fmul(axis.y, s),
        fmul(axis.z, s),
        fcos_approx(half)
    );
}

Quaternion Quaternion::operator*(const Quaternion& q) const {
    return Quaternion(
        fmul(w, q.x) + fmul(x, q.w) + fmul(y, q.z) - fmul(z, q.y),
        fmul(w, q.y) - fmul(x, q.z) + fmul(y, q.w) + fmul(z, q.x),
        fmul(w, q.z) + fmul(x, q.y) - fmul(y, q.x) + fmul(z, q.w),
        fmul(w, q.w) - fmul(x, q.x) - fmul(y, q.y) - fmul(z, q.z)
    );
}

Quaternion& Quaternion::operator*=(const Quaternion& q) {
    *this = *this * q;
    return *this;
}

bool Quaternion::operator==(const Quaternion& q) const {
    return x == q.x && y == q.y && z == q.z && w == q.w;
}

bool Quaternion::operator!=(const Quaternion& q) const {
    return !(*this == q);
}

Vector3<fixed_t> Quaternion::Rotate(const Vector3<fixed_t>& v) const {
    fixed_t two = FLOAT_TO_FIXED(2.0f);

    fixed_t xx = fmul(x, x);
    fixed_t yy = fmul(y, y);
    fixed_t zz = fmul(z, z);
    fixed_t xy = fmul(x, y);
    fixed_t xz = fmul(x, z);
    fixed_t yz = fmul(y, z);
    fixed_t wx = fmul(w, x);
    fixed_t wy = fmul(w, y);
    fixed_t wz = fmul(w, z);

    return Vector3<fixed_t>{
        fmul((FLOAT_TO_FIXED(1) - fmul(two, yy + zz)), v.x)
        + fmul(two, xy - wz) * v.y
        + fmul(two, xz + wy) * v.z,

        fmul(two, xy + wz) * v.x
        + fmul((FLOAT_TO_FIXED(1) - fmul(two, xx + zz)), v.y)
        + fmul(two, yz - wx) * v.z,

        fmul(two, xz - wy) * v.x
        + fmul(two, yz + wx) * v.y
        + fmul((FLOAT_TO_FIXED(1) - fmul(two, xx + yy)), v.z)
    };
}
