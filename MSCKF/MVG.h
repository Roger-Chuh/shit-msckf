/*
    MVG.h - Multi-View Geometry
    ����ͼ������صĺ���
*/

#pragma once
#include <vector>
#include <Eigen/Eigen>
#include "JPL.h"

// ���ǻ����ο�[5]
// �ú������ p ʹ������Ĺ�ϵ����ȫ���� i ����
//     Proj(Rs[i]*p+Ts[i]) = xs[i]
// ������Ϊ�������� Linear LS ��ȡ��ֵ��Ȼ��������С�����Ż�
//Eigen::Vector3d Triangulation(const std::vector<Eigen::Vector2d> &xs, const std::vector<Eigen::Matrix3d> &Rs, const std::vector<Eigen::Vector3d> &Ts);

// ʹ�� Linear LS �����������ǻ����ο�[5]
// xs �� states �Ӻ���ǰ��Ӧ��xs �ĳ��Ȳ����� states �ĳ���
Eigen::Vector3d LinearLSTriangulation(const std::vector<Eigen::Vector2d> &xs, const std::vector<std::pair<Eigen::Matrix3d, Eigen::Vector3d>> &states);

// ʹ�� Linear LS �����������ǻ����ο�[5]
// xs.second ���� states �еĶ�Ӧ��
Eigen::Vector3d LinearLSTriangulation(const std::vector<std::pair<Eigen::Vector2d, size_t>> &xs, const std::vector<std::pair<Eigen::Matrix3d, Eigen::Vector3d>> &states);
//Eigen::Vector3d LinearLSTriangulation(const std::vector<Eigen::Vector2d> &xs, const std::vector<Eigen::Matrix3d> &Rs, const std::vector<Eigen::Vector3d> &Ts);

// ���ݳ�ʼֵ p0 �����ǻ�������С�����Ż�
//Eigen::Vector3d RefineTriangulation(const Eigen::Vector3d &p0, const std::vector<Eigen::Vector2d> &xs, const std::vector<Eigen::Matrix3d> &Rs, const std::vector<Eigen::Vector3d> &Ts);