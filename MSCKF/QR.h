/*
    QR.h - QR Decomposition Related
    һЩ���� QR �ֽ�Ĺ��ߣ�����������ʽ�ֽ���ӿռ�ͶӰ
*/

#pragma once
#include <Eigen/Eigen>

// ���� Givens ��ת�õ���ϵ��
// ���� a Ϊ��Ҫ�����Ԫ�أ� b Ϊ���������Ԫ��
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

