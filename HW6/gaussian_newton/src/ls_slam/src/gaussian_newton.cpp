#include "gaussian_newton.h"
#include <eigen3/Eigen/Jacobi>
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Householder>
#include <eigen3/Eigen/Cholesky>
#include <eigen3/Eigen/SparseCholesky>
#include <eigen3/Eigen/LU>
#include <ctime>
#include <time.h>

#include <iostream>

//位姿-->转换矩阵
Eigen::Matrix3d PoseToTrans(Eigen::Vector3d x)
{
    Eigen::Matrix3d trans;
    trans << cos(x(2)), -sin(x(2)), x(0),
        sin(x(2)), cos(x(2)), x(1),
        0, 0, 1;

    return trans;
}

//转换矩阵－－＞位姿
Eigen::Vector3d TransToPose(Eigen::Matrix3d trans)
{
    Eigen::Vector3d pose;
    pose(0) = trans(0, 2);
    pose(1) = trans(1, 2);
    pose(2) = atan2(trans(1, 0), trans(0, 0));

    return pose;
}

//计算整个pose-graph的误差
double ComputeError(std::vector<Eigen::Vector3d> &Vertexs,
                    std::vector<Edge> &Edges)
{
    double sumError = 0;
    for (int i = 0; i < Edges.size(); i++)
    {
        Edge tmpEdge = Edges[i];
        Eigen::Vector3d xi = Vertexs[tmpEdge.xi];
        Eigen::Vector3d xj = Vertexs[tmpEdge.xj];
        Eigen::Vector3d z = tmpEdge.measurement;
        Eigen::Matrix3d infoMatrix = tmpEdge.infoMatrix;

        Eigen::Matrix3d Xi = PoseToTrans(xi);
        Eigen::Matrix3d Xj = PoseToTrans(xj);
        Eigen::Matrix3d Z = PoseToTrans(z);

        Eigen::Matrix3d Ei = Z.inverse() * Xi.inverse() * Xj;

        Eigen::Vector3d ei = TransToPose(Ei);

        sumError += ei.transpose() * infoMatrix * ei;
    }
    return sumError;
}

/**
 * @brief CalcJacobianAndError
 *         计算jacobian矩阵和error
 * @param xi    fromIdx
 * @param xj    toIdx
 * @param z     观测值:xj相对于xi的坐标
 * @param ei    计算的误差
 * @param Ai    相对于xi的Jacobian矩阵
 * @param Bi    相对于xj的Jacobian矩阵
 */
void CalcJacobianAndError(Eigen::Vector3d xi, Eigen::Vector3d xj, Eigen::Vector3d z,
                          Eigen::Vector3d &ei, Eigen::Matrix3d &Ai, Eigen::Matrix3d &Bi)
{
    //TODO--Start
    Eigen::Matrix3d Xi = PoseToTrans(xi);
    Eigen::Matrix3d Xj = PoseToTrans(xj);
    Eigen::Matrix3d Z = PoseToTrans(z);

    Eigen::Matrix3d Ei = Z.inverse() * Xi.inverse() * Xj;

    ei = TransToPose(Ei);

    //Eigen::Matrix3d Aij, Bij;

    Ai.setZero();
    Bi.setZero();

    Eigen::Matrix2d Ri = Xi.block(0, 0, 2, 2);
    Eigen::Matrix2d Rj = Xj.block(0, 0, 2, 2);
    Eigen::Matrix2d Rij = Z.block(0, 0, 2, 2);

    Eigen::Vector2d ti;
    ti << xi(0), xi(1);
    Eigen::Vector2d tj;
    tj << xj(0), xj(1);
    Eigen::Vector2d tij;
    tij << z(0), z(1);

    Eigen::Matrix2d translation;
    translation << -sin(xi(2)), cos(xi(2)), -cos(xi(2)), -sin(xi(2));
    Eigen::Vector2d tmpt = Rij.transpose() * translation * (tj - ti);

    Ai.block(0, 0, 2, 2) = -Rij.transpose() * Ri.transpose();
    Ai(2, 2) = -1;
    Ai(0, 2) = tmpt(0);
    Ai(1, 2) = tmpt(1);

    Bi.block(0, 0, 2, 2) = Rij.transpose() * Ri.transpose();
    Bi(2, 2) = 1;

    //TODO--end
}

/**
 * @brief LinearizeAndSolve
 *        高斯牛顿方法的一次迭代．
 * @param Vertexs   图中的所有节点
 * @param Edges     图中的所有边
 * @return          位姿的增量
 */
Eigen::VectorXd LinearizeAndSolve(std::vector<Eigen::Vector3d> &Vertexs,
                                  std::vector<Edge> &Edges)
{
    //申请内存
    Eigen::MatrixXd H(Vertexs.size() * 3, Vertexs.size() * 3);
    Eigen::VectorXd b(Vertexs.size() * 3);

    H.setZero();
    b.setZero();

    //固定第一帧
    Eigen::Matrix3d I;
    I.setIdentity();
    H.block(0, 0, 3, 3) += I;

    //构造H矩阵　＆ b向量
    for (int i = 0; i != Edges.size(); i++)
    {
        //提取信息
        Edge tmpEdge = Edges[i];
        Eigen::Vector3d xi = Vertexs[tmpEdge.xi];
        Eigen::Vector3d xj = Vertexs[tmpEdge.xj];
        Eigen::Vector3d z = tmpEdge.measurement;
        Eigen::Matrix3d infoMatrix = tmpEdge.infoMatrix;

        //计算误差和对应的Jacobian
        Eigen::Vector3d ei;
        Eigen::Matrix3d Ai;
        Eigen::Matrix3d Bi;
        CalcJacobianAndError(xi, xj, z, ei, Ai, Bi);

        //TODO--Start
        Eigen::Vector3d delta_A = (ei.transpose() * infoMatrix * Ai).transpose();
        Eigen::Vector3d delta_B = (ei.transpose() * infoMatrix * Bi).transpose();
        for (int index = 0; index != 3; ++index)
        {
            b(3 * tmpEdge.xi + index) += delta_A(index);
            b(3 * tmpEdge.xj + index) += delta_B(index);
        }

        H.block(tmpEdge.xi * 3, tmpEdge.xi * 3, 3, 3) += Ai.transpose() * infoMatrix * Ai;
        H.block(tmpEdge.xi * 3, tmpEdge.xj * 3, 3, 3) += Ai.transpose() * infoMatrix * Bi;
        H.block(tmpEdge.xj * 3, tmpEdge.xi * 3, 3, 3) += Bi.transpose() * infoMatrix * Ai;
        H.block(tmpEdge.xj * 3, tmpEdge.xj * 3, 3, 3) += Bi.transpose() * infoMatrix * Bi;

        //TODO--End
    }

    //求解
    Eigen::VectorXd dx;

    //TODO--Start

    std::cout << "dx begins to calculate!!!" << std::endl;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
    Eigen::SparseMatrix<double> sp_H = H.sparseView();
    solver.compute(sp_H);
    if (solver.info() != Eigen::Success)
    {
        std::cout << "decomposition failed" << std::endl;
    }
    dx = solver.solve(-b);
    if (solver.info() != Eigen::Success)
    {
        std::cout << "decomposition failed" << std::endl;
    }

    //TODO-End

    return dx;
}
