- MSCKF

      ������ A Multi-State Constraint Kalman Filter for Vision-aided Inertial Navigation �ĺ����㷨ʵ��

- MSCKF_Simulation

      ʹ��ģ������������� MSCKF

- ����ϵ

      �豸����ϵ��Gyro �� Accelerometer������
      �� iPhone ��ֱ���ã������Ļ

           y
           |
           |
           +-----+
           |     |
           |     |
           |     |
           |  o  |
           +-----+---- x
          /
         /
        z

      �������ϵ����
      �� iPhone ������ã������Ļ��Home��λ���Ҳ�

           z
          /
         /
        +--------------+---- x
        |              |
        |            O |
        |              |
        +--------------+
        |
        |
        y

      R_imu_to_img = [ -UnitY(), -UnitX(), -UnitZ() ];
      p_img_in_imu = [ 0.0065, 0.0638, 0.0000 ]; // ֻ��Ŀ��

- Accelerometer ����

      ��оƬ�Ե�·��ʩ�ӵļ��ٶȣ���/оƬ��������
      ��˵� iPhone ƽ��ʱ��оƬ��Ե�·��ʩ�����µ�������Ӧ���ٶ�Ϊ�豸����ϵ�� -z ����
      �� iPhone ���豸����ϵĳ���������ʱ��оƬ�Ե�·��ʩ�ӷ�����������Զ�����Ҫȡ����