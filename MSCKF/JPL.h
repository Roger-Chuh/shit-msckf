/*
    JPL.h - JPL's Quaternion
    �������� JPL ��Ԫ������ĺ������ο� [3]
*/

#pragma once
#include <Eigen/Eigen>

// JPL ��Ԫ��ʵ����ʹ��һ����ά������ʾ
typedef Eigen::Vector4d JPL_Quaternion;

// ��׼��һ�� JPL ��Ԫ��
// Ϊ�˱�ʾ��ת����Ԫ����ҪΪ��λ��
// ͬʱΪ�˸��õ���ֵ�ȶ��ԣ���֤ w ������
inline JPL_Quaternion JPL_Normalize(const JPL_Quaternion &q) {
    JPL_Quaternion r = q.normalized();
    if (r.w() < 0) r = -r;
    return r;
}

// ��һ�� JPL ��Ԫ��ת���� Hamilton ��Ԫ��
inline Eigen::Quaterniond JPL_toHamilton(const JPL_Quaternion &q) {
    return Eigen::Quaterniond(q.w(), -q.x(), -q.y(), -q.z());
}

// ��һ�� Hamilton ��Ԫ��ת��Ϊ JPL ��Ԫ��
inline JPL_Quaternion HamiltonToJPL(const Eigen::Quaterniond& q) {
    return JPL_Quaternion(-q.x(), -q.y(), -q.z(), q.w());
}

// ���һ�� JPL ��Ԫ����Ӧ����ת
inline Eigen::Matrix3d JPL_C(const JPL_Quaternion &q) {
    return JPL_toHamilton(q).toRotationMatrix();
}

// ���һ�� JPL ��Ԫ����Ӧ����ת��ת��
inline Eigen::Matrix3d JPL_CT(const JPL_Quaternion &q) {
    // �ȼ��㹲�������ת����
    return Eigen::Quaterniond(q.w(), q.x(), q.y(), q.z()).toRotationMatrix();
}

// �������� JPL ��Ԫ���ĳ˻�
inline JPL_Quaternion JPL_Multiply(const JPL_Quaternion &q, const JPL_Quaternion &p) {
    return JPL_Quaternion(
        q(3)*p(0) + q(2)*p(1) - q(1)*p(2) + q(0)*p(3),
        -q(2)*p(0) + q(3)*p(1) + q(0)*p(2) + q(1)*p(3),
        q(1)*p(0) - q(0)*p(1) + q(3)*p(2) + q(2)*p(3),
        -q(0)*p(0) - q(1)*p(1) - q(2)*p(2) + q(3)*p(3)
        );
}

// [3] �ж���� [ * x] ����
inline Eigen::Matrix3d JPL_Cross(const Eigen::Vector3d &w) {
    return (Eigen::Matrix3d() << 0.0, -w.z(), w.y(),
        w.z(), 0.0, -w.x(),
        -w.y(), w.x(), 0.0).finished();
}

// [3] �ж���� Omega ����
inline Eigen::Matrix4d JPL_Omega(const Eigen::Vector3d& w) {
    return (Eigen::Matrix4d() << 0.0, w.z(), -w.y(), w.x(),
        -w.z(), 0.0, w.x(), w.y(),
        w.y(), -w.x(), 0.0, w.z(),
        -w.x(), -w.y(), -w.z(), 0.0).finished();
}