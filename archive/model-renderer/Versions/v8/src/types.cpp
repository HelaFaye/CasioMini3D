#include "types.hpp"

Quaternion::Quaternion() : x(0), y(0), z(0), w(1.0f) {
    UpdateTrigCache();
}

Quaternion::~Quaternion() {}

Quaternion::Quaternion(fixed_t x, fixed_t y, fixed_t z, fixed_t w) : x(x), y(y), z(z), w(w) {
    UpdateTrigCache();
}

Quaternion::Quaternion(Vector3<fixed_t> eulerAngles) {
    Set(eulerAngles);
}

void Quaternion::Normalize() {
    fixed_t lenSq = x * x + y * y + z * z + w * w;
    if (lenSq == 0) return;

    fixed_t invLen = fixed_t::inv_sqrt(lenSq);
    x *= invLen;
    y *= invLen;
    z *= invLen;
    w *= invLen;
}

void Quaternion::Set(fixed_t x, fixed_t y, fixed_t z, fixed_t w) {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
    UpdateTrigCache();
}

void Quaternion::Set(Vector3<fixed_t> euler) {
    fixed_t pitch = -euler.x;
    fixed_t yaw   = -euler.y;
    fixed_t roll  = -euler.z;

    fixed_t cX = fixed_t::cos(pitch / 2);
    fixed_t sX = fixed_t::sin(pitch / 2);
    fixed_t cY = fixed_t::cos(yaw / 2);
    fixed_t sY = fixed_t::sin(yaw / 2);
    fixed_t cZ = fixed_t::cos(roll / 2);
    fixed_t sZ = fixed_t::sin(roll / 2);

    w = cY * cX * cZ + sY * sX * sZ;
    x = cY * sX * cZ + sY * cX * sZ;
    y = cY * cX * sZ - sY * sX * cZ;
    z = sY * cX * cZ - cY * sX * sZ;

    UpdateTrigCache();
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

    fixed_t sinp = fixed_t(2) * (w * x - y * z);
    if (sinp.abs() >= 1)
        angles.x = -fixed_t::copysign(fixed_t(M_PI_2), sinp);
    else
        angles.x = -fixed_t::asin(sinp);

    fixed_t siny_cosp = fixed_t(2) * (w * y + x * z);
    fixed_t cosy_cosp = fixed_t(1) - fixed_t(2) * (x * x + y * y);
    angles.y = -fixed_t::atan2(siny_cosp, cosy_cosp);

    fixed_t sinr_cosp = fixed_t(2) * (w * z + x * y);
    fixed_t cosr_cosp = fixed_t(1) - fixed_t(2) * (z * z + x * x);
    angles.z = -fixed_t::atan2(sinr_cosp, cosr_cosp);

    return angles;
}

Quaternion Quaternion::Identity() {
    return Quaternion(0, 0, 0, 1.0f);
}


Quaternion Quaternion::AngleAxis(fixed_t angleRad, Vector3<fixed_t> axis) {
    fixed_t half = angleRad / 2;
    fixed_t s = fixed_t::sin(half);
    return Quaternion(
        axis.x * s,
        axis.y * s,
        axis.z * s,
        fixed_t::cos(half)
    );
}

Quaternion Quaternion::operator*(const Quaternion& q) const {
    return Quaternion(
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y - x * q.z + y * q.w + z * q.x,
        w * q.z + x * q.y - y * q.x + z * q.w,
        w * q.w - x * q.x - y * q.y - z * q.z
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

    uv  *= w * fixed_t(2);
    uuv *= fixed_t(2);

    return v + uv + uuv;
}