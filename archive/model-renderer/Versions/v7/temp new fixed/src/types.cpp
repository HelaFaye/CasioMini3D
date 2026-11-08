#include "types.hpp"

Quaternion::Quaternion() : x(fixed_t(0)), y(fixed_t(0)), z(fixed_t(0)), w(fixed_t(1)) {
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
    fixed_t lenSq = (x * x) + (y * y) + (z * z) + (w * w);
    if (lenSq == fixed_t(0)) return;

    fixed_t invLen = fixed_t::inv(fixed_t::sqrt(lenSq)); // ou une approximation rapide
    x = (x * invLen);
    y = (y * invLen);
    z = (z * invLen);
    w = (w * invLen);
}

void Quaternion::Set(fixed_t x, fixed_t y, fixed_t z, fixed_t w) {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
    UpdateTrigCache();
}

void Quaternion::Set(float x, float y, float z, float w) {
    Set(fixed_t(x), fixed_t(y), fixed_t(z), fixed_t(w));
}

void Quaternion::Set(Vector3<fixed_t> euler) {
    const fixed_t pitch = -euler.x;
    const fixed_t yaw   = -euler.y;
    const fixed_t roll  = -euler.z;

    const fixed_t cX = fixed_t::cos(pitch / 2);
    const fixed_t sX = fixed_t::sin(pitch / 2);
    const fixed_t cY = fixed_t::cos(yaw / 2);
    const fixed_t sY = fixed_t::sin(yaw / 2);
    const fixed_t cZ = fixed_t::cos(roll / 2);
    const fixed_t sZ = fixed_t::sin(roll / 2);

    w = (cY * (cX * cZ)) + (sY * (sX * sZ));
    x = (cY * (sX * cZ)) + (sY * (cX * sZ));
    y = (cY * (cX * sZ)) - (sY * (sX * cZ));
    z = (sY * (cX * cZ)) - (cY * (sX * sZ));

    UpdateTrigCache();
}

void Quaternion::Set(Vector3<float> euler) {
    Set(Vector3<fixed_t>{fixed_t(euler.x), fixed_t(euler.y), fixed_t(euler.z)});
}

void Quaternion::UpdateTrigCache() {
    Normalize();
    euler = ToEulerAngles();
    cos = {
        fixed_t::cos(euler.x),
        fixed_t::cos(euler.y),
        fixed_t::cos(euler.z)
    };
    sin = {
        fixed_t::sin(euler.x),
        fixed_t::sin(euler.y),
        fixed_t::sin(euler.z)
    };
}

Vector3<fixed_t> Quaternion::ToEulerAngles() const {
    Vector3<fixed_t> angles;

    // Pitch (X)
    fixed_t sinp = (fixed_t(2) * (w * x) - (y * z));
    if (sinp.abs() >= fixed_t(1))
        angles.x = -fixed_t::copysign(fixed_t(M_PI_2), sinp); // ±90°
    else
        angles.x = -fixed_t::asin(sinp);

    // Yaw (Y) [avec inversion finale]
    fixed_t siny_cosp = (fixed_t(2) * (w * y) + (x * z));
    fixed_t cosy_cosp = fixed_t(1) - (fixed_t(2) * (x * x) + (y * y));
    angles.y = -fixed_t::atan2(siny_cosp, cosy_cosp);

    // Roll (Z) [avec inversion finale]
    fixed_t sinr_cosp = (fixed_t(2) * (w * z) + (x * y));
    fixed_t cosr_cosp = fixed_t(1) - (fixed_t(2) * (z * z) + (x * x));
    angles.z = -fixed_t::atan2(sinr_cosp, cosr_cosp);

    return angles;
}


Quaternion Quaternion::Identity() {
    return Quaternion(fixed_t(0), fixed_t(0), fixed_t(0), fixed_t(1));
}

Quaternion Quaternion::AngleAxis(float angleRad, Vector3<float> axis) {
    float half = angleRad * 0.5f;
    float s = sinf(half);
    return Quaternion(
        fixed_t(axis.x * s),
        fixed_t(axis.y * s),
        fixed_t(axis.z * s),
        fixed_t(cosf(half))
    );
}

Quaternion Quaternion::AngleAxis(fixed_t angleRad, Vector3<fixed_t> axis) {
    fixed_t half = angleRad / 2;
    fixed_t s = fixed_t::sin(half);
    return Quaternion(
        (axis.x * s),
        (axis.y * s),
        (axis.z * s),
        fixed_t::cos(half)
    );
}

Quaternion Quaternion::operator*(const Quaternion& q) const {
    return Quaternion(
        (w * q.x) + (x * q.w) + (y * q.z) - (z * q.y),
        (w * q.y) - (x * q.z) + (y * q.w) + (z * q.x),
        (w * q.z) + (x * q.y) - (y * q.x) + (z * q.w),
        (w * q.w) - (x * q.x) - (y * q.y) - (z * q.z)
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

    Vector3<fixed_t> uv = qvec.cross(v);
    Vector3<fixed_t> uuv = qvec.cross(uv);

    uv  = (uv * (w * fixed_t(2.0f)));
    uuv = (uuv * fixed_t(2.0f));

    return v + uv + uuv;
}

