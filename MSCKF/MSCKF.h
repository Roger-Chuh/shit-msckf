/*
    MSCKF.h - Multi-State Constrained Kalman Filter
    ���ĵ��˲���ģ��
*/

#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include <Eigen/Eigen>
#include "JPL.h"
#include "MVG.h"

namespace Eigen {
    typedef Matrix<double, 15, 15> Matrix15d; // 15x15 ����
}

class MSCKF {
public:
    MSCKF();

    // ����������Э�����С
    void setNoiseCov(const Eigen::Matrix3d &cov_ng, const Eigen::Matrix3d &cov_nwg, const Eigen::Matrix3d &cov_na, const Eigen::Matrix3d &cov_nwa, double sigma_im_squared);

    // ��ʼ���˲���
    // ��Ҫ�ṩ��ʼ����ת�����ٶ�ƫ��ٶȡ����ٶ�ƫ�λ�á��������ٶ�
    void initialize(const JPL_Quaternion &q, const Eigen::Vector3d &bg, const Eigen::Vector3d &v, const Eigen::Vector3d &ba, const Eigen::Vector3d &p, double g = 9.81);

    // �� t ʱ�̵Ľ��ٶȺͼ��ٶ������л���
    void propagate(double t, const Eigen::Vector3d &w, const Eigen::Vector3d &a);

    // ʹ�� GPS �źŽ��и���
    void track(double t, const Eigen::Vector3d &position, double sigma_p);

    // ������������٣���������������
    // features[���ٵ�����֡�е�����id] = <�����ٵ�����֡�е�����id����������֡�е�ͶӰλ�ã��Ѿ������ڲΣ�>
    void track(double t, const std::unordered_map<size_t, std::pair<size_t, Eigen::Vector2d>> &matches);

    // ��õ�ǰ����������������ϵ�еĳ���
    Eigen::Quaterniond orientation() const { return JPL_toHamilton(m_q).conjugate(); }

    // ��õ�ǰ����������������ϵ�е�λ��
    Eigen::Vector3d position() const { return m_p; }

    Eigen::Matrix3d positionCovariance() const { return m_PII.block<3, 3>(12, 12); }

    // ��õ�ǰ�������������ϵ�еĳ���
    Eigen::Quaterniond cameraOrientation() const { return JPL_toHamilton(JPL_Multiply(m_q_imu_to_cam, m_q)).conjugate(); }

    // ��õ�ǰ�������������ϵ�е�λ��
    Eigen::Vector3d cameraPosition() const { return m_p + JPL_CT(m_q)*m_p_cam_in_imu; }

private:
    // MotionSystem ��Ҫ�ڻ���ʱ��õ�ǰ�ĸ��ֲ���
    friend class MotionSystem;

    size_t m_state_limit;            // ���״̬�ĸ�������
    JPL_Quaternion m_q_imu_to_cam;   // ��IMU����ϵ���������ϵ����ת
    Eigen::Vector3d m_p_cam_in_imu;  // ���������IMU����ϵ�е�����
    double m_sigma_im_squared;       // ͶӰ����ľ�����

    Eigen::Matrix3d m_cov_ng;        // ���ٶȴ�����Э����
    Eigen::Matrix3d m_cov_nwg;       // ���ٶ�ƫ������Э����
    Eigen::Matrix3d m_cov_na;        // ���ٶȴ�����Э����
    Eigen::Matrix3d m_cov_nwa;       // ���ٶ�ƫ������Э����

    JPL_Quaternion m_q;              // ��������ϵ���豸����ϵ����ת
    Eigen::Vector3d m_bg;            // �豸����ϵ�н��ٶ�ƫ��
    Eigen::Vector3d m_v;             // ��������ϵ�е��ٶ�
    Eigen::Vector3d m_ba;            // �豸����ϵ�м��ٶ�ƫ��
    Eigen::Vector3d m_p;             // ��������ϵ�е�λ��
    double m_g;                      // �������ٶȴ�С

    Eigen::Matrix15d m_PII;          // IMU״̬��Э����
    std::vector<Eigen::MatrixXd> m_PIC; // �ֿ��ʾ�� PIC (IMU�����״̬֮���Э����)
    std::vector<std::vector<Eigen::MatrixXd>> m_PCC; // �ֿ��ʾ�� PCC �������� (���״̬��Э����)

    bool m_has_old = false;
    double m_t_old;
    Eigen::Vector3d m_w_old;
    Eigen::Vector3d m_a_old;

    std::vector<CameraState> m_states; // ��� state������ʹ�� R �� T ���������е� q �� p
    std::unordered_map<size_t, std::vector<Eigen::Vector2d>> m_tracks; // ���� tracks
};
