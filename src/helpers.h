#ifndef HELPERS_H
#define HELPERS_H

#include <math.h>
#include "json.hpp"
#include <iostream>
#include <string>
#include <vector>
// for convenience
using nlohmann::json;
using std::string;
using std::vector;
using Eigen::MatrixXd;
using Eigen::VectorXd;

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
//   else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

//
// Helper functions related to waypoints and converting from XY to Frenet
//   or vice versa
//

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Calculate distance between two points
double distance(double x1, double y1, double x2, double y2) {
  return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}

// Calculate closest waypoint to current x, y position
int ClosestWaypoint(double x, double y, const vector<double> &maps_x, 
                    const vector<double> &maps_y) {
  double closestLen = 100000; //large number
  int closestWaypoint = 0;

  for (int i = 0; i < maps_x.size(); ++i) {
    double map_x = maps_x[i];
    double map_y = maps_y[i];
    double dist = distance(x,y,map_x,map_y);
    if (dist < closestLen) {
      closestLen = dist;
      closestWaypoint = i;
    }
  }

  return closestWaypoint;
}

// Returns next waypoint of the closest waypoint
int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, 
                 const vector<double> &maps_y) {
  int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

  double map_x = maps_x[closestWaypoint];
  double map_y = maps_y[closestWaypoint];

  double heading = atan2((map_y-y),(map_x-x));

  double angle = fabs(theta-heading);
  angle = std::min(2*pi() - angle, angle);

  if (angle > pi()/2) {
    ++closestWaypoint;
    if (closestWaypoint == maps_x.size()) {
      closestWaypoint = 0;
    }
  }

  return closestWaypoint;
}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, 
                         const vector<double> &maps_x, 
                         const vector<double> &maps_y) {
  int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

  int prev_wp;
  prev_wp = next_wp-1;
  if (next_wp == 0) {
    prev_wp  = maps_x.size()-1;
  }

  double n_x = maps_x[next_wp]-maps_x[prev_wp];
  double n_y = maps_y[next_wp]-maps_y[prev_wp];
  double x_x = x - maps_x[prev_wp];
  double x_y = y - maps_y[prev_wp];

  // find the projection of x onto n
  double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
  double proj_x = proj_norm*n_x;
  double proj_y = proj_norm*n_y;

  double frenet_d = distance(x_x,x_y,proj_x,proj_y);

  //see if d value is positive or negative by comparing it to a center point
  double center_x = 1000-maps_x[prev_wp];
  double center_y = 2000-maps_y[prev_wp];
  double centerToPos = distance(center_x,center_y,x_x,x_y);
  double centerToRef = distance(center_x,center_y,proj_x,proj_y);

  if (centerToPos <= centerToRef) {
    frenet_d *= -1;
  }

  // calculate s value
  double frenet_s = 0;
  for (int i = 0; i < prev_wp; ++i) {
    frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
  }

  frenet_s += distance(0,0,proj_x,proj_y);

  return {frenet_s,frenet_d};
}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<double> &maps_s, 
                     const vector<double> &maps_x, 
                     const vector<double> &maps_y) {
  int prev_wp = -1;

  while (s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1))) {
    ++prev_wp;
  }

  int wp2 = (prev_wp+1)%maps_x.size();

  double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),
                         (maps_x[wp2]-maps_x[prev_wp]));
  // the x,y,s along the segment
  double seg_s = (s-maps_s[prev_wp]);

  double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
  double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

  double perp_heading = heading-pi()/2;

  double x = seg_x + d*cos(perp_heading);
  double y = seg_y + d*sin(perp_heading);

  return {x,y};
}
vector<double> JMT(vector<double> &start, vector<double> &end, double T) {
   MatrixXd tMatrix = MatrixXd(3,3);
   MatrixXd sMatrix = MatrixXd(3,1);
   MatrixXd result = MatrixXd(3,1);
   tMatrix << pow(T, 3), pow(T, 4), pow(T, 5),
        3*pow(T,2), 4*pow(T,3), 5*pow(T, 4),
        6*T, 12*pow(T,2), 20*pow(T,3);
        
   sMatrix << end[0] - (start[0] + start[1]*T + 0.5*start[2]*pow(T,2)),
            end[1] - (start[1] + start[2]*T),
            end[2] - start[2];
            
    MatrixXd inverseT = tMatrix.inverse();
    result = inverseT * sMatrix;
    vector<double>r{start[0],start[1],0.5*start[2],result.data()[0],result.data()[1], result.data()[2]};
    return r;
}

double generate_s(vector<double>& a, double t){
  return a[0] + a[1] * t + a[2] * pow(t,2) + a[3] * pow(t,3) + a[4] * pow(t,4) + a[5] * pow(t,5);
}

void change_lane(int& lane_num, json& sensor_fusion, int prev_path_size, double car_speed, double car_s, int new_lane){
  bool can_change = true;
  for(int j = 0; j < sensor_fusion.size(); j++){
      float temp_d_value = sensor_fusion[j][6];
      if(temp_d_value < (4*new_lane + 4) && temp_d_value > (4*new_lane)){
        double tvx = sensor_fusion[j][3];
        double tvy = sensor_fusion[j][4];
        double tcheck_speed = sqrt(pow(tvx, 2) + pow(tvy, 2));
        double tcheck_car_s = sensor_fusion[j][5];
        tcheck_car_s += double(prev_path_size) * 0.02 * tcheck_speed;
        double temp_car_s = car_s + double(prev_path_size) * 0.02 * car_speed;
        std::cout << fabs(temp_car_s - tcheck_car_s) << std::endl;
        if(fabs(temp_car_s - tcheck_car_s) < 50) {
          can_change = false;
          break;
        }
      }
  }
  if(can_change) {
    lane_num = new_lane;
  }
}
#endif  // HELPERS_H