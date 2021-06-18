#include "../include/calib_odom/Odom_Calib.hpp"


//设置数据长度,即多少数据计算一次
void OdomCalib::Set_data_len(int len)
{
    data_len = len;
    A.conservativeResize(len*3,9);
    b.conservativeResize(len*3);
    A.setZero();
    b.setZero();
}


/*
输入:里程计和激光数据

TODO:
构建最小二乘需要的超定方程组
Ax = b

*/
bool OdomCalib::Add_Data(Eigen::Vector3d Odom,Eigen::Vector3d scan)
{

    if(now_len<INT_MAX)
    {

        //TODO: 构建超定方程组
        int num = now_len % data_len;
        A(num * 3, 0) = Odom(0);
        A(num * 3, 1) = Odom(1);
        A(num * 3, 2) = Odom(2);
        A(num * 3 + 1, 3) = Odom(0);
        A(num * 3 + 1, 4) = Odom(1);
        A(num * 3 + 1, 5) = Odom(2);
        A(num * 3 + 2, 6) = Odom(0);
        A(num * 3 + 2, 7) = Odom(1);
        A(num * 3 + 2, 8) = Odom(2);

        b(num * 3) = scan(0);
        b(num * 3 + 1) = scan(1);
        b(num * 3 + 2) = scan(2);

        //end of TODO
        now_len++;
        return true;
    }
    else
    {
        return false;
    }
}

/*
 * TODO:
 * 求解线性最小二乘Ax=b
 * 返回得到的矫正矩阵
*/
Eigen::Matrix3d OdomCalib::Solve()
{
    Eigen::Matrix3d correct_matrix;

    //TODO: 求解线性最小二乘

    Eigen::VectorXd vector_1 = A.colPivHouseholderQr().solve(b);

    correct_matrix << vector_1(0), vector_1(1), vector_1(2),
                      vector_1(3), vector_1(4), vector_1(5),
                      vector_1(6), vector_1(7), vector_1(8);
    //end of TODO

    return correct_matrix;
}

/* 用于判断数据是否满
 * 数据满即可以进行最小二乘计算
*/
bool OdomCalib::is_full()
{
    if(now_len%data_len==0&&now_len>=1)
    {
        now_len = data_len;
        return true;
    }
    else
        return false;
}

/*
 * 数据清零
*/
void OdomCalib::set_data_zero()
{
    A.setZero();
    b.setZero();
}