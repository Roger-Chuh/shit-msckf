/*
    MVG.h - Multi-View Geometry
    ����ͼ������صĺ���
*/

#pragma once
#include <vector>
#include <Eigen/Eigen>
#include "JPL.h"

struct CameraState {
    Eigen::Matrix3d R;
    Eigen::Vector3d T;
};

// ʹ�� Linear LS �����������ǻ����ο�[5]
// �ú������ p ʹ������Ĺ�ϵ����ȫ���� i ����
//     Proj(Rs[i]*p+Ts[i]) = xs[i]
// xs �� states �Ӻ���ǰ��Ӧ��xs �ĳ��Ȳ����� states �ĳ���
Eigen::Vector3d LinearLSTriangulation(const std::vector<Eigen::Vector2d> &xs, const std::vector<CameraState> &states);

// ʹ�� Linear LS �����������ǻ����ο�[5]
// �ú������ p ʹ������Ĺ�ϵ����ȫ���� i ����
//     Proj(Rs[i]*p+Ts[i]) = xs[i]
// xs.second ���� states �еĶ�Ӧ��
Eigen::Vector3d LinearLSTriangulation(const std::vector<std::pair<Eigen::Vector2d, size_t>> &xs, const std::vector<CameraState> &states);

// ���ݳ�ʼֵ p0 �����ǻ�������С�����Ż�
Eigen::Vector3d RefineTriangulation(const Eigen::Vector3d &p0, const std::vector<std::pair<Eigen::Vector2d, size_t>> &xs, const std::vector<CameraState> &states);
