#ifndef GAUSSIAN_NEWTON_H
#define GAUSSIAN_NEWTON_H

#include <vector>
#include <eigen3/Eigen/Core>

typedef struct edge
{
  int xi, xj;
  Eigen::Vector3d measurement;
  Eigen::Matrix3d infoMatrix;
} Edge;

Eigen::Matrix3d PoseToTrans(Eigen::Vector3d x);
Eigen::Vector3d TransToPose(Eigen::Matrix3d trans);

Eigen::VectorXd LinearizeAndSolve(std::vector<Eigen::Vector3d> &Vertexs,
                                  std::vector<Edge> &Edges);

double ComputeError(std::vector<Eigen::Vector3d> &Vertexs,
                    std::vector<Edge> &Edges);

#endif
