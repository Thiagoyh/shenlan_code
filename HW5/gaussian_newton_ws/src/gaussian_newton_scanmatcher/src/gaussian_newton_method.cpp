#include <map.h>
#include "gaussian_newton_method.h"

const double GN_PI = 3.1415926;

//进行角度正则化．
double GN_NormalizationAngle(double angle)
{
    if (angle > GN_PI)
        angle -= 2 * GN_PI;
    else if (angle < -GN_PI)
        angle += 2 * GN_PI;

    return angle;
}

//将(x,y,\theta)转换为矩阵形式
Eigen::Matrix3d GN_V2T(Eigen::Vector3d vec)
{
    Eigen::Matrix3d T;
    T << cos(vec(2)), -sin(vec(2)), vec(0),
        sin(vec(2)), cos(vec(2)), vec(1),
        0, 0, 1;

    return T;
}

//对某一个点进行转换．
Eigen::Vector2d GN_TransPoint(Eigen::Vector2d pt, Eigen::Matrix3d T)
{
    Eigen::Vector3d tmp_pt(pt(0), pt(1), 1);
    tmp_pt = T * tmp_pt;
    return Eigen::Vector2d(tmp_pt(0), tmp_pt(1));
}

//用激光雷达数据创建势场．
map_t *CreateMapFromLaserPoints(Eigen::Vector3d map_origin_pt,
                                std::vector<Eigen::Vector2d> laser_pts,
                                double resolution)
{
    map_t *map = map_alloc();

    map->origin_x = map_origin_pt(0);
    map->origin_y = map_origin_pt(1);
    map->resolution = resolution;

    //固定大小的地图，必要时可以扩大．
    map->size_x = 10000;
    map->size_y = 10000;

    map->cells = (map_cell_t *)malloc(sizeof(map_cell_t) * map->size_x * map->size_y);

    //高斯平滑的sigma－－固定死
    map->likelihood_sigma = 0.5;

    Eigen::Matrix3d Trans = GN_V2T(map_origin_pt);

    //设置障碍物
    for (int i = 0; i < laser_pts.size(); i++)
    {
        Eigen::Vector2d tmp_pt = GN_TransPoint(laser_pts[i], Trans);

        int cell_x, cell_y;
        cell_x = MAP_GXWX(map, tmp_pt(0));
        cell_y = MAP_GYWY(map, tmp_pt(1));

        map->cells[MAP_INDEX(map, cell_x, cell_y)].occ_state = CELL_STATUS_OCC;
    }

    //进行障碍物的膨胀--最大距离固定死．
    map_update_cspace(map, 0.5);

    return map;
}

/**
 * @brief InterpMapValueWithDerivatives
 * 在地图上的进行插值，得到coords处的势场值和对应的关于位置的梯度．
 * 返回值为Eigen::Vector3d ans
 * ans(0)表示市场值
 * ans(1:2)表示梯度
 * @param map
 * @param coords
 * @return
 */
Eigen::Vector3d InterpMapValueWithDerivatives(map_t *map, Eigen::Vector2d &coords)
{
    Eigen::Vector3d ans;
    //TODO
    ans << 0, 0, 0;

    int cell_x, cell_y;
    cell_x = MAP_GXWX(map, coords(0));
    cell_y = MAP_GYWY(map, coords(1));

    if (!MAP_VALID(map, cell_x, cell_y))
        return ans;

    double delta_x = (coords(0) - map->origin_x) / map->resolution + 0.5 + map->size_x / 2 - cell_x;
    double delta_y = (coords(1) - map->origin_y) / map->resolution + 0.5 + map->size_y / 2 - cell_y;

    std::vector<double> score(4, 0);

    int tmp_x[4] = {0, 1, 1, 0};
    int tmp_y[4] = {0, 0, 1, 1};
    for (int i = 0; i != 4; ++i)
        score[i] = map->cells[MAP_INDEX(map, cell_x + tmp_x[i], cell_y + tmp_y[i])].score;

    ans << (1 - delta_y) * (delta_x * score[1] + (1 - delta_x) * score[0]) +
               delta_y * (delta_x * score[2] + (1 - delta_x) * score[3]),
        (delta_y * (score[2] - score[3]) + (1 - delta_y) * (score[1] - score[0])) / map->resolution,
        (delta_x * (score[2] - score[1]) + (1 - delta_x) * (score[3] - score[0])) / map->resolution;
    //END OF TODO
    return ans;
}

/**
 * @brief ComputeCompleteHessianAndb
 * 计算H*dx = b中的H和b
 * @param map
 * @param now_pose
 * @param laser_pts
 * @param H
 * @param b
 */
//调用线性插值函数
void ComputeHessianAndb(map_t *map, Eigen::Vector3d now_pose,
                        std::vector<Eigen::Vector2d> &laser_pts,
                        Eigen::Matrix3d &H, Eigen::Vector3d &b)
{
    H = Eigen::Matrix3d::Zero();
    b = Eigen::Vector3d::Zero();

    //TODO
    //Eigen::Vector2d GN_TransPoint(Eigen::Vector2d pt, Eigen::Matrix3d T)
    //将now_pose转成矩阵形式
    Eigen::Matrix3d Trans = GN_V2T(now_pose);

    for (std::vector<Eigen::Vector2d>::size_type i = 0; i != laser_pts.size(); ++i)
    {
        Eigen::Vector2d laser_pt = laser_pts[i];
        Eigen::Vector2d laser_pts_i = GN_TransPoint(laser_pt, Trans);
        Eigen::Vector3d ans_interp = InterpMapValueWithDerivatives(map, laser_pts_i);

        //(1 - ans_interp(0)) <=> [1 - M(S_i(T))]
        b[0] += ans_interp(1) * (1 - ans_interp(0));
        b[1] += ans_interp(2) * (1 - ans_interp(0));

        double theta = now_pose(2);
        double rot = ((-sin(theta) * laser_pt(0) - cos(theta) * laser_pt(1))) *
                         ans_interp(1) +
                     (cos(theta) * laser_pt(0) - sin(theta) * laser_pt(1)) *
                         ans_interp(2);

        b[2] += rot * (1 - ans_interp(0));

        H(0, 0) += ans_interp(1) * ans_interp(1);
        H(1, 1) += ans_interp(2) * ans_interp(2);
        H(2, 2) += rot * rot;

        H(0, 1) += ans_interp(1) * ans_interp(2);
        H(0, 2) += ans_interp(1) * rot;
        H(1, 2) += ans_interp(2) * rot;
    }

    H(1, 0) = H(0, 1);
    H(2, 0) = H(0, 2);
    H(2, 1) = H(1, 2);

    //END OF TODO
}

/**
 * @brief GaussianNewtonOptimization
 * 进行高斯牛顿优化．
 * @param map
 * @param init_pose
 * @param laser_pts
 */
//调用求海塞矩阵、雅克比矩阵函数
void GaussianNewtonOptimization(map_t *map, Eigen::Vector3d &init_pose, std::vector<Eigen::Vector2d> &laser_pts)
{
    int maxIteration = 20;
    Eigen::Vector3d now_pose = init_pose;
    Eigen::Matrix3d H;
    Eigen::Vector3d b;

    for (int i = 0; i < maxIteration; i++)
    {
        //TODO
        ComputeHessianAndb(map, now_pose, laser_pts, H, b);
        if (H(0, 0) && H(1, 1) && H(2, 2))
        {
            Eigen::Vector3d res(H.inverse() * b);

            if (res[2] > 0.20)
                res[2] = 0.20;
            if (res[2] < -0.20)
                res[2] = -0.20;
            now_pose += res;
        }
        //END OF TODO
    }
    init_pose = now_pose;
}
