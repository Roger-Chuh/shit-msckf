#include "MSCKF.h"
#include "RK.h"
#include "MVG.h"

#include <iostream>

using namespace std;
using namespace Eigen;

MSCKF::MSCKF() {
    // ���ֲ����ĳ�ʼ��
    m_state_limit = 30;
    Matrix3d R_imu_to_cam;
    R_imu_to_cam << -Vector3d::UnitY(), -Vector3d::UnitX(), -Vector3d::UnitZ();
    m_q_imu_to_cam = HamiltonToJPL(Quaterniond(R_imu_to_cam));
    m_p_cam_in_imu << 0.0065, 0.0638, 0.0;

    m_cov_ng.setIdentity();
    m_cov_nwg.setIdentity();
    m_cov_na.setIdentity();
    m_cov_nwa.setIdentity();
}

void MSCKF::setNoiseCov(const Matrix3d &cov_ng, const Matrix3d &cov_nwg, const Matrix3d &cov_na, const Matrix3d &cov_nwa) {
    m_cov_ng = cov_ng;
    m_cov_nwg = cov_nwg;
    m_cov_na = cov_na;
    m_cov_nwa = cov_nwa;
}

void MSCKF::initialize(const JPL_Quaternion &q, const Vector3d &bg, const Vector3d &v, const Vector3d &ba, const Vector3d &p, double g) {
    m_q = q;
    m_bg = bg;
    m_v = v;
    m_ba = ba;
    m_p = p;
    m_g = g;
}

// [1] �е� (9)(12)��(13)��bg �� ba �ǹ̶��ģ�����û�а����������PII��Phi����Ҫ����ͬ��ʱ���ڽ��л������԰�����������
// StateVector := q, v, p, PII, Phi
typedef tuple<Vector4d, Vector3d, Vector3d, Matrix15d, Matrix15d> StateVector;

// StateVector �����ˣ����� RK ���л���
inline StateVector operator*(double s, const StateVector &v2) {
    return make_tuple(s*get<0>(v2), s*get<1>(v2), s*get<2>(v2), s*get<3>(v2), s*get<4>(v2));
}

// StateVector �ļӷ������� RK ���л���
inline void operator+=(StateVector &v, const StateVector &v1) {
    get<0>(v) += get<0>(v1);
    get<1>(v) += get<1>(v1);
    get<2>(v) += get<2>(v1);
    get<3>(v) += get<3>(v1);
    get<4>(v) += get<4>(v1);
}

class MotionSystem {
public:
    MotionSystem(const MSCKF &filter, double t, const Vector3d &w, const Vector3d &a) :
        dt(t - filter.m_t_old),
        w0(filter.m_w_old - filter.m_bg),
        dw(w - filter.m_w_old),
        a0(filter.m_a_old - filter.m_ba),
        da(a - filter.m_a_old),
        filter(filter)
    {}

    StateVector operator() (double t, StateVector s) {
        StateVector r;
        double tt = t - filter.m_t_old;
        Vector3d wt = w0 + dw*tt / dt;
        Vector3d at = a0 + da*tt / dt;

        const JPL_Quaternion& sq = get<0>(s);
        const Vector3d& sv = get<1>(s);
        const Vector3d& sp = get<2>(s);
        const Matrix15d& sPII = get<3>(s);
        const Matrix15d& sPhi = get<4>(s);

        JPL_Quaternion& rq = get<0>(r);
        Vector3d& rv = get<1>(r);
        Vector3d& rp = get<2>(r);
        Matrix15d& rPII = get<3>(r);
        Matrix15d& rPhi = get<4>(r);

        Matrix3d cqt = JPL_CT(sq);
        Matrix3d neg_cross_w = -JPL_Cross(wt);
        Matrix3d cross_a = JPL_Cross(at);

        // (9)
        rq = 0.5*(JPL_Omega(wt)*sq);
        rv = cqt*at; rv.z() += filter.m_g;
        rp = sv;

        // (12)
        Matrix15d FxPII;
        multiplyFM(FxPII, neg_cross_w, -cqt, cross_a, sPII);
        rPII = FxPII + FxPII.transpose();
        rPII.block<3, 3>(0, 0) += filter.m_cov_ng;
        rPII.block<3, 3>(3, 3) += filter.m_cov_nwg;
        rPII.block<3, 3>(6, 6) += filter.m_cov_na;
        rPII.block<3, 3>(9, 9) += filter.m_cov_nwa;

        // (13)
        multiplyFM(rPhi, neg_cross_w, -cqt, cross_a, sPhi);

        return r;
    }
private:
    void multiplyFM(Matrix15d &Result, const Matrix3d &neg_cross_w, const Matrix3d &neg_cqt, const Matrix3d &cross_a, const Matrix15d &M) {
        Result.setZero();
        for (int i = 0; i < 15; i += 3) {
            Result.block<3, 3>(0, i) = neg_cross_w*M.block<3, 3>(0, i) - M.block<3, 3>(3, i);
            Result.block<3, 3>(6, i) = neg_cqt*(cross_a*M.block<3, 3>(0, i) + M.block<3, 3>(9, i));
            Result.block<3, 3>(12, i) = M.block<3, 3>(6, i);
        }
    }

    double dt;
    Vector3d w0, dw;
    Vector3d a0, da;
    const MSCKF &filter;
};

RungeKutta<double, StateVector> rk;

void MSCKF::propagate(double t, const Vector3d &w, const Vector3d &a) {
    static const Matrix15d eye = Matrix15d::Identity();
    Matrix15d Phi;

    if (m_has_old) {
        MotionSystem system(*this, t, w, a);
        tie(m_q, m_v, m_p, m_PII, Phi) = rk.integrate(system, tie(m_q, m_v, m_p, m_PII, eye), m_t_old, t);
        m_q = JPL_Normalize(m_q);
    }
    m_t_old = t;
    m_w_old = w;
    m_a_old = a;
    m_has_old = true;
}

void MSCKF::track(double t, const unordered_map<size_t, pair<size_t, Vector2d>> &matches) {
    //
    // ���Ƚ��и��٣��ҳ���ǰ��ʧ�� track
    //
    unordered_map<size_t, vector<Vector2d>> continued_tracks; // ���а��������и��������� track
    size_t useful_state_length = 0; // ��¼���������� track ����󳤶�
    for (const auto & f : matches) {
        if (m_tracks.count(f.first)>0) { // ����������ٵ������е�����
            continued_tracks[f.second.first].swap(m_tracks.at(f.first)); // �����ٵ��ľ� track �ŵ� continued_tracks ��
        }
        useful_state_length = max(useful_state_length, continued_tracks[f.second.first].size());
        continued_tracks[f.second.first].push_back(f.second.second); // �����ٵ���������λ�ü��뵽 track ��
    }

    vector<vector<Vector2d>> lost_tracks; // ���а����˶�ʧ�� track
    for (auto &t : m_tracks) {
        if (t.second.size() > 1) { // ���ҽ��������ʧ�� track �����˳��������㣬���ǲſ�����������û������
            lost_tracks.emplace_back(move(t.second));
            useful_state_length = max(useful_state_length, lost_tracks.back().size());
        }
    }

    //
    // ȥ���Ͼ����õ� states
    //
    if (useful_state_length < m_states.size()) {
        m_states.erase(m_states.begin(), m_states.begin() + (m_states.size() - useful_state_length));
    }

    //
    // ���״̬�����ɴﵽ���ޣ��ڼ����µ� state ǰ������Ҫɾ��һЩ
    // TODO: Modify m_PIC and m_PCC

    // ��ɾ������ state �� track ��¼������
    vector<pair<Matrix3d, Vector3d>> new_states;
    unordered_map<size_t, vector<Vector2d>> new_tracks;

    // ����ɾ���� track ��¼���������Ҫ��� m_state ���� update
    unordered_map<size_t, vector<pair<Vector2d, size_t>>> jumping_tracks;
    if (m_states.size() == m_state_limit) {
        for (size_t i = 0; i < m_states.size(); ++i) {
            if (i % 3 != 1) {
                new_states.emplace_back(m_states[i].first, m_states[i].second);
            }
        }
        for (auto& t : continued_tracks) { // ���ÿ�������ϵ� track������ȥ���������������ɶ����� track
            size_t tbegin = m_states.size() - (t.second.size() - 1);
            vector<Vector2d>& nt = new_tracks[t.first];
            vector<pair<Vector2d, size_t>>& jt = jumping_tracks[t.first];
            for (size_t i = tbegin; i < m_states.size(); ++i) {
                if (i % 3 != 1) {
                    nt.emplace_back(t.second[i - tbegin]);
                }
                else {
                    jt.emplace_back(t.second[i - tbegin], i);
                }
            }
            nt.emplace_back(t.second.back());
        }
    }

    //
    // ���� EKF ����
    // TODO: (17)~(33) here

    for (size_t i = 0; i < lost_tracks.size(); ++i) {
        Vector3d p = LinearLSTriangulation(lost_tracks[i], m_states);
    }
    for (auto &t : jumping_tracks) {
        if (t.second.size() > 1) {
            Vector3d p = LinearLSTriangulation(t.second, m_states);
        }
    }

    //
    // ����µ�״̬
    // TODO: (14)~(16) here

    if (m_states.size() == m_state_limit) {
        m_states.swap(new_states);
        m_tracks.swap(new_tracks);
    }
    else { // �������û���м�ɾ���κ�״̬������track����continued_tracks��
        m_tracks.swap(continued_tracks);
    }

    // ���㵱ǰ�����̬
    // (14)
    JPL_Quaternion q_world_to_cam = JPL_Multiply(m_q_imu_to_cam, m_q);
    Vector3d imu_to_cam_shift_in_world = JPL_CT(m_q)*m_p_cam_in_imu;
    Vector3d p_cam_in_world = m_p + imu_to_cam_shift_in_world;

    // �����µ�״̬
    // (15)
    // CHECK
    Matrix<double, 6, 15> Jc; // (16) �� J �������� 15 ����Ϊ Jc ���м���
    Jc.setZero();
    Jc.block<3, 3>(0, 0) = JPL_C(m_q_imu_to_cam);
    Jc.block<3, 3>(3, 0) = JPL_Cross(imu_to_cam_shift_in_world);
    Jc.block<3, 3>(3, 12).setIdentity();
    Matrix<double, 15, 6> JcT = Jc.transpose();

    MatrixXd PIC(15, m_PIC.cols() + 6);
    PIC.block(0, 0, 15, m_PIC.cols()) = m_PIC;
    PIC.block(0, m_PIC.cols(), 15, 6) = m_PII*JcT;

    MatrixXd PCC(m_PCC.rows() + 6, m_PCC.cols() + 6);
    PCC.block(0, 0, m_PCC.rows(), m_PCC.cols()) = m_PCC;
    PCC.block(m_PCC.rows(), 0, 6, m_PCC.cols()) = Jc*m_PIC;
    PCC.block(0, m_PCC.cols(), m_PCC.rows(), 6) = PCC.block(m_PCC.rows(), 0, 6, m_PCC.cols()).transpose();
    PCC.block(m_PCC.rows(), m_PCC.cols(), 6, 6) = Jc*m_PII*JcT;

    m_PIC.swap(PIC);
    m_PCC.swap(PCC);

    Matrix3d R_world_to_cam = JPL_C(q_world_to_cam);
    Vector3d T_world_to_cam = -R_world_to_cam*p_cam_in_world;
    m_states.emplace_back(R_world_to_cam, T_world_to_cam);
}