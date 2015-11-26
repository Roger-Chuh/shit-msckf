#include "MSCKF.h"
#include "RK.h"
#include "MVG.h"

using namespace std;
using namespace Eigen;

// ���� Givens ��ת�õ���ϵ��
// ���� a Ϊ���������Ԫ�أ� b Ϊ��Ҫ�����Ԫ��
// ���� c Ϊ��Ӧ�� cos ������s Ϊ sin ����
inline void Givens(double a, double b, double &c, double &s) {
    if (abs(b)<1.0e-15) {
        c = copysign(1.0, a);
        s = 0;
    }
    else if (abs(a)<1.0e-15) {
        c = 0;
        s = -copysign(1.0, b);
    }
    else if (abs(b) > abs(a)) {
        double t = a / b;
        double u = copysign(sqrt(1 + t*t), b);
        s = -1.0 / u;
        c = -s*t;
    }
    else {
        double t = b / a;
        double u = copysign(sqrt(1 + t*t), a);
        c = 1.0 / u;
        s = -c*t;
    }
}

MSCKF::MSCKF() {
    // ���ֲ����ĳ�ʼ��
    m_state_limit = 10;
    Matrix3d R_imu_to_cam;
    R_imu_to_cam << -Vector3d::UnitY(), -Vector3d::UnitX(), -Vector3d::UnitZ();
    m_q_imu_to_cam = HamiltonToJPL(Quaterniond(R_imu_to_cam));
    m_p_cam_in_imu << 0.0065, 0.0638, 0.0;

    m_cov_ng.setIdentity();
    m_cov_nwg.setIdentity();
    m_cov_na.setIdentity();
    m_cov_nwa.setIdentity();
    m_sigma_im_squared = 1.0;
}

void MSCKF::setNoiseCov(const Matrix3d &cov_ng, const Matrix3d &cov_nwg, const Matrix3d &cov_na, const Matrix3d &cov_nwa, double sigma_im_squared) {
    m_cov_ng = cov_ng;
    m_cov_nwg = cov_nwg;
    m_cov_na = cov_na;
    m_cov_nwa = cov_nwa;
    m_sigma_im_squared = sigma_im_squared;
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

    // eq. after (11)
    for (size_t i = 0; i < m_PIC.size(); ++i) {
        m_PIC[i] = Phi*m_PIC[i];
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
        if (m_tracks.count(f.second.first)>0) { // ����������ٵ������е�����
            continued_tracks[f.first].swap(m_tracks.at(f.second.first)); // �����ٵ��ľ� track �ŵ� continued_tracks ��
        }
        useful_state_length = max(useful_state_length, continued_tracks[f.first].size());
        continued_tracks[f.first].push_back(f.second.second); // �����ٵ���������λ�ü��뵽 track ��
    }

    vector<vector<Vector2d>> lost_tracks; // ���а����˶�ʧ�� track
    for (auto &t : m_tracks) {
        lost_tracks.emplace_back(vector<Vector2d>());
        lost_tracks.back().swap(t.second);
        useful_state_length = max(useful_state_length, lost_tracks.back().size());
    }

    //
    // ȥ���Ͼ����õ� states
    //
    if (useful_state_length < m_states.size()) {
        size_t num_to_erase = m_states.size() - useful_state_length;
        m_states.erase(m_states.begin(), m_states.begin() + num_to_erase);
        m_PIC.erase(m_PIC.begin(), m_PIC.begin() + num_to_erase);
        m_PCC.erase(m_PCC.begin(), m_PCC.begin() + num_to_erase);
        for (auto &p : m_PCC) {
            p.erase(p.begin(), p.begin() + num_to_erase);
        }
    }

    //
    // ���״̬�����ɴﵽ���ޣ��ڼ����µ� state ǰ������Ҫɾ��һЩ
    //

    // ��ɾ������ state �� track ��¼������
    vector<size_t> remaining_states_id;
    unordered_map<size_t, vector<Vector2d>> new_tracks;

    // ����ɾ���� track ��¼���������Ҫ��� m_state ���� update
    unordered_map<size_t, vector<pair<Vector2d, size_t>>> jumping_tracks;
    if (m_states.size() == m_state_limit) {
        for (size_t i = 0; i < m_states.size(); ++i) {
            if (i % 3 != 1) {
                remaining_states_id.push_back(i);
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

    // ������ track ͳһ��������ͬһ���߼�����
    vector<vector<pair<Vector2d, size_t>>> track_for_update; // ���� update ������ track
    vector<Vector3d> point_for_update; // �������� update ����ά��
    for (size_t i = 0; i < lost_tracks.size(); ++i) {
        if (lost_tracks[i].size() > 1) {
            Vector3d p = LinearLSTriangulation(lost_tracks[i], m_states);
            Vector3d q = m_states.back().first*p + m_states.back().second;
            if (q.z()>0.1 && q.z() < 100.0) {
                point_for_update.push_back(p);
                track_for_update.emplace_back(vector<pair<Vector2d, size_t>>());
                size_t jstart = m_states.size() - lost_tracks[i].size();
                for (size_t j = 0; j < lost_tracks[i].size(); ++j) {
                    track_for_update.back().emplace_back(lost_tracks[i][j], jstart + j);
                }
            }
        }
    }

    for (auto &t : jumping_tracks) {
        if (t.second.size() > 1) {
            Vector3d p = LinearLSTriangulation(t.second, m_states);
            size_t tid = t.second.back().second;
            Vector3d q = m_states[tid].first*p + m_states[tid].second;
            if (q.z()>0.1 && q.z() < 100.0) {
                point_for_update.push_back(p);
                track_for_update.emplace_back(move(t.second));
            }
        }
    }

    for (size_t i = 0; i < track_for_update.size(); ++i) {
        point_for_update[i] = RefineTriangulation(point_for_update[i], track_for_update[i], m_states);
    }

    //
    // ���� EKF ����
    //

    int Hrows = 0;
    int Hcols = (int)m_states.size() * 6;

    // Ԥ�ȼ���� HX �Ĵ�С
    for (size_t i = 0; i < track_for_update.size(); ++i) {
        Hrows += (int)track_for_update[i].size() * 2 - 3;
    }

    if (Hrows > 0) {
        MatrixXd HX(Hrows, Hcols);
        VectorXd ro(Hrows, 1);

        int row_start = 0;
        for (size_t j = 0; j < track_for_update.size(); ++j) {
            int nrow = (int)track_for_update[j].size() * 2;
            MatrixXd HXj(nrow, Hcols);
            MatrixXd Hfj(nrow, 3);
            VectorXd rj(nrow);
            HXj.setZero();
            Hfj.setZero();
            const Vector3d &pj = point_for_update[j];
            for (size_t i = 0; i < track_for_update[j].size(); ++i) {
                const size_t ii = track_for_update[j][i].second;
                const Vector2d &zij = track_for_update[j][i].first;
                const Matrix3d &Ri = m_states[ii].first;
                const Vector3d &Ti = m_states[ii].second;
                Vector3d Xji = Ri*pj + Ti;                                       // eq. after (20)
                Vector2d zij_triangulated(Xji.x() / Xji.z(), Xji.y() / Xji.z()); // eq. after (20)
                MatrixXd Jij(2, 3);                                              // eq. after (23)
                Jij.setIdentity();                                               // eq. after (23)
                Jij.col(2) = -zij_triangulated;                                  // eq. after (23)
                Jij /= Xji.z();                                                  // eq. after (23)
                MatrixXd Hfij = Jij*Ri;                                          // (23)
                HXj.block<2, 3>(i * 2, ii * 6) = Jij*JPL_Cross(Xji);             // (22)
                HXj.block<2, 3>(i * 2, ii * 6 + 3) = -Hfij;                      // (22)
                Hfj.block<2, 3>(i * 2, 0) = Hfij;                                // (23)
                rj.block<2, 1>(i * 2, 0) = zij - zij_triangulated;               // (20)
            }

            // (25)(26)���� HXj ͶӰ�� Hfj ����ռ䣬ͨ�� Givens ��ת�� Hfj ���� QR �ֽ����
            for (int col = 0; col < 3; ++col) {
                for (int row = (int)Hfj.rows() - 1; row > col; --row) {
                    if (abs(Hfj(row, col)) > 1e-15) {
                        double c, s;
                        Givens(Hfj(row - 1, col), Hfj(row, col), c, s);
                        for (int k = 0; k < 3; ++k) { // ��������ֻ�ں��������ǣ����½ǿ���ֱ��������ߺ���
                            double a = c*Hfj(row - 1, k) - s*Hfj(row, k);
                            double b = s*Hfj(row - 1, k) + c*Hfj(row, k);
                            Hfj(row - 1, k) = a;
                            Hfj(row, k) = b;
                        }
                        for (int k = 0; k < HXj.cols(); ++k) {
                            double a = c*HXj(row - 1, k) - s*HXj(row, k);
                            double b = s*HXj(row - 1, k) + c*HXj(row, k);
                            HXj(row - 1, k) = a;
                            HXj(row, k) = b;
                        }
                        double a = c*rj(row - 1) - s*rj(row);
                        double b = s*rj(row - 1) + c*rj(row);
                        rj(row - 1) = a;
                        rj(row) = b;
                    }
                }
            }
            HX.block(row_start, 0, nrow - 3, Hcols) = HXj.block(3, 0, nrow - 3, Hcols);
            ro.block(row_start, 0, nrow - 3, 1) = rj.block(3, 0, nrow - 3, 1);
            row_start += nrow - 3;
        }

        // (28)(29)���� HX ͶӰ������ range��ͬ��ʹ�� Givens ��ת���� QR �ֽ�
        for (int col = 0; col < HX.cols(); ++col) {
            for (int row = (int)HX.rows() - 1; row > col; --row) {
                if (abs(HX(row, col)) > 1e-15) {
                    double c, s;
                    Givens(HX(row - 1, col), HX(row, col), c, s);
                    for (int k = 0; k < HX.cols(); ++k) { // ��������ͬ��ֻ�����������ǣ������ಿ����Ҫ����
                        double a = c*HX(row - 1, k) - s*HX(row, k);
                        double b = s*HX(row - 1, k) + c*HX(row, k);
                        HX(row - 1, k) = a;
                        HX(row, k) = b;
                    }
                    double a = c*ro(row - 1) - s*ro(row);
                    double b = s*ro(row - 1) + c*ro(row);
                    ro(row - 1) = a;
                    ro(row) = b;
                }
            }
        }

        // ׼��������Э�������
        MatrixXd P(m_states.size() * 6 + 15, m_states.size() * 6 + 15);
        P.block<15, 15>(0, 0) = m_PII;
        for (int i = 0; i < m_PIC.size(); ++i) {
            P.block<15, 6>(0, 15 + 6 * i) = m_PIC[i];
            P.block<6, 15>(15 + 6 * i, 0) = m_PIC[i].transpose();
            for (int j = 0; j <= i; ++j) {
                P.block<6, 6>(15 + 6 * j, 15 + 6 * i) = m_PCC[i][j];
                if (i != j) {
                    P.block<6, 6>(15 + 6 * i, 15 + 6 * j) = m_PCC[i][j].transpose();
                }
            }
        }

        int Trows = min(Hrows, Hcols);

        // ��ʵ�� TH ��� 15 ��Ϊ 0
        MatrixXd TH(Trows, 15 + Hcols);
        TH.block(0, 0, Trows, 15).setZero();
        TH.block(0, 15, Trows, Hcols) = HX.block(0, 0, Trows, Hcols);

        // (31)�����㿨��������
        MatrixXd PTHT = P*TH.transpose();
        MatrixXd PR = TH*PTHT;
        for (int i = 0; i < PR.rows(); ++i) {
            PR(i, i) += m_sigma_im_squared;
        }
        MatrixXd K = PTHT*PR.inverse();
        // (32)��EKF״̬����
        VectorXd dX = K*ro.block(0, 0, Trows, 1);

        m_q = JPL_Correct(m_q, dX.block<3, 1>(0, 0));
        m_bg += dX.block<3, 1>(3, 0);
        m_v += dX.block<3, 1>(6, 0);
        m_ba += dX.block<3, 1>(9, 0);
        m_p += dX.block<3, 1>(12, 0);

        for (size_t i = 0; i < m_states.size(); ++i) {
            JPL_Quaternion q = HamiltonToJPL(Quaterniond(m_states[i].first));
            q = JPL_Correct(q, dX.block<3, 1>(15 + i * 6, 0));
            Vector3d p = -(m_states[i].first.transpose()*m_states[i].second);
            p += dX.block<3, 1>(18 + i * 6, 0);
            m_states[i].first = JPL_toHamilton(q).toRotationMatrix();
            m_states[i].second = -m_states[i].first*p;
        }

        // (33)��EKF�������
        MatrixXd KTH = K*TH;
        for (int i = 0; i < KTH.rows(); ++i) {
            KTH(i, i) -= 1;
        }
        MatrixXd Pnew = KTH*P*KTH.transpose() + (m_sigma_im_squared*K)*K.transpose();

        // �� P ���Ϊ�ڲ��ı��
        m_PII = Pnew.block<15, 15>(0, 0);
        for (int i = 0; i < m_PIC.size(); ++i) {
            m_PIC[i] = Pnew.block<15, 6>(0, 15 + 6 * i);
            for (int j = 0; j <= i; ++j) {
                m_PCC[i][j] = Pnew.block<6, 6>(15 + 6 * j, 15 + 6 * i);
            }
        }
    }

    //
    // ����״̬����
    //

    if (m_states.size() == m_state_limit) {
        m_tracks.swap(new_tracks);
        vector<pair<Matrix3d, Vector3d>> new_states(remaining_states_id.size());
        vector<MatrixXd> new_PIC(remaining_states_id.size());
        vector<vector<MatrixXd>> new_PCC(remaining_states_id.size());
        for (size_t i = 0; i < remaining_states_id.size(); ++i) {
            new_states[i] = m_states[remaining_states_id[i]];
            new_PIC[i] = m_PIC[remaining_states_id[i]];
            new_PCC[i].resize(i+1);
            for (size_t j = 0; j < new_PCC[i].size(); ++j) {
                new_PCC[i][j] = m_PCC[remaining_states_id[i]][remaining_states_id[j]];
            }
        }
        m_states.swap(new_states);
        m_PIC.swap(new_PIC);
        m_PCC.swap(new_PCC);
    }
    else { // �������û���м�ɾ���κ�״̬������track����continued_tracks��
        m_tracks.swap(continued_tracks);
    }

    // ���㵱ǰ�����̬
    // (14)
    JPL_Quaternion q_world_to_cam = JPL_Multiply(m_q_imu_to_cam, m_q);
    Vector3d imu_to_cam_shift_in_world = JPL_CT(m_q)*m_p_cam_in_imu;
    Vector3d p_cam_in_world = m_p + imu_to_cam_shift_in_world;

    // (16)
    Matrix<double, 6, 15> Jc; 
    Jc.setZero();
    Jc.block<3, 3>(0, 0) = JPL_C(m_q_imu_to_cam);
    Jc.block<3, 3>(3, 0) = JPL_Cross(imu_to_cam_shift_in_world);
    Jc.block<3, 3>(3, 12).setIdentity();
    Matrix<double, 15, 6> JcT = Jc.transpose();
    
    // (15)
    m_PCC.push_back(vector<MatrixXd>());
    for (size_t i = 0; i < m_PIC.size(); ++i) {
        m_PCC.back().push_back((Jc*m_PIC[i]).transpose());
    }
    m_PCC.back().push_back(Jc*m_PII*JcT);
    m_PIC.push_back(m_PII*JcT);

    // �����µ�״̬
    Matrix3d R_world_to_cam = JPL_C(q_world_to_cam);
    Vector3d T_world_to_cam = -R_world_to_cam*p_cam_in_world;
    m_states.emplace_back(R_world_to_cam, T_world_to_cam);
}
