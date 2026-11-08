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

void Quaternion::Normalize() {
    fixed_t lenSq = fmul(x, x) + fmul(y, y) + fmul(z, z) + fmul(w, w);
    if (lenSq == 0) return;

    fixed_t invLen = finv(sqrt_fixed(lenSq)); // ou une approximation rapide
    x = fmul(x, invLen);
    y = fmul(y, invLen);
    z = fmul(z, invLen);
    w = fmul(w, invLen);
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
    const fixed_t pitch = -euler.x;
    const fixed_t yaw   = -euler.y;
    const fixed_t roll  = -euler.z;

    const fixed_t cX = fcos_approx(pitch / 2);
    const fixed_t sX = fsin_approx(pitch / 2);
    const fixed_t cY = fcos_approx(yaw / 2);
    const fixed_t sY = fsin_approx(yaw / 2);
    const fixed_t cZ = fcos_approx(roll / 2);
    const fixed_t sZ = fsin_approx(roll / 2);

    w = fmul(cY, fmul(cX, cZ)) + fmul(sY, fmul(sX, sZ));
    x = fmul(cY, fmul(sX, cZ)) + fmul(sY, fmul(cX, sZ));
    y = fmul(cY, fmul(cX, sZ)) - fmul(sY, fmul(sX, cZ));
    z = fmul(sY, fmul(cX, cZ)) - fmul(cY, fmul(sX, sZ));

    UpdateTrigCache();
}

void Quaternion::Set(Vector3<float> euler) {
    Set(Vector3<fixed_t>{FLOAT_TO_FIXED(euler.x), FLOAT_TO_FIXED(euler.y), FLOAT_TO_FIXED(euler.z)});
}

void Quaternion::UpdateTrigCache() {
    Normalize();
    euler = ToEulerAngles();
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

    // Pitch (X)
    fixed_t sinp = fmul(FLOAT_TO_FIXED(2), fmul(w, x) - fmul(y, z));
    if (abs(sinp) >= FLOAT_TO_FIXED(1))
        angles.x = -fcopysign(FLOAT_TO_FIXED(M_PI_2), sinp); // ±90°
    else
        angles.x = -fasin_approx(sinp);

    // Yaw (Y) [avec inversion finale]
    fixed_t siny_cosp = fmul(FLOAT_TO_FIXED(2), fmul(w, y) + fmul(x, z));
    fixed_t cosy_cosp = FLOAT_TO_FIXED(1) - fmul(FLOAT_TO_FIXED(2), fmul(x, x) + fmul(y, y));
    angles.y = -fatan2_approx(siny_cosp, cosy_cosp);

    // Roll (Z) [avec inversion finale]
    fixed_t sinr_cosp = fmul(FLOAT_TO_FIXED(2), fmul(w, z) + fmul(x, y));
    fixed_t cosr_cosp = FLOAT_TO_FIXED(1) - fmul(FLOAT_TO_FIXED(2), fmul(z, z) + fmul(x, x));
    angles.z = -fatan2_approx(sinr_cosp, cosr_cosp);

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
    Vector3<fixed_t> qvec = {x, y, z};

    Vector3<fixed_t> uv = cross(qvec, v);
    Vector3<fixed_t> uuv = cross(qvec, uv);

    uv  = fmul(uv, fmul(w, FLOAT_TO_FIXED(2.0f)));
    uuv = fmul(uuv, FLOAT_TO_FIXED(2.0f));

    return v + uv + uuv;
}

