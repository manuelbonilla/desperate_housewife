#ifndef GRID_H
#define GRID_H

#include <ros/ros.h>
#include <ros/console.h>

class grid
{
public:
  void gridspace(const std_msgs::Float64MultiArray::ConstPtr &msg);
  KDL::Vector GetRepulsiveWithTable();
  KDL::Vector GetRepulsiveWithObstacles();
  void InfoGeometry(const desperate_housewife::fittedGeometriesArray::ConstPtr& msg);
  grid();
  ~grid(){};
private:
  ros::Subscribe sub_grid_,obstacles_subscribe_;
  std::vector<double> Object_radius;
  std::vector<double> Object_height;
  std::vector<KDL::Frame> Object_position;
   ros::Publisher marker_pub;


};



int main(int argc, char **argv)
{
  ros::init(argc, argv, "desperate_grid_node");
  grid node;
  // node.SendVitoHome();

  ROS_INFO("[grid] Node is ready");

  double spin_rate = 10;
  ros::param::get("~spin_rate",spin_rate);
  ROS_DEBUG( "Spin Rate %lf", spin_rate);

  ros::Rate rate(spin_rate); 

  while (node.nh.ok())
  {
    ros::spinOnce(); 
    rate.sleep();
  }
  return 0;
}


grid::grid()
{
    //grid
    sub_grid_ = nh_.subscribe("gridspace", 1, &PotentialFieldControl::gridspace, this);
    obstacles_subscribe_ = n.subscribe(obstacle_avoidance.c_str(), 1, &PotentialFieldControl::InfoGeometry, this); 
    marker_pub = n.advertise<visualization_msgs::Marker>("visualization_marker", 1);
}

void grid::InfoGeometry(const desperate_housewife::fittedGeometriesArray::ConstPtr& msg)
{
      Object_radius.clear();
      Object_height.clear();
      Object_position.clear();
      Time_traj_rep = 0;
      
      //get info for calculates objects surface
      for(unsigned int i=0; i < msg->geometries.size(); i++)
      {
        KDL::Frame frame_obj;
        Object_radius.push_back(msg->geometries[i].info[0]);  //radius
        Object_height.push_back(msg->geometries[i].info[1]);  //height

        tf::poseMsgToKDL(msg->geometries[i].pose, frame_obj);
        Object_position.push_back(frame_obj); 
      }
}


void gridspace(const std_msgs::Float64MultiArray::ConstPtr &msg)
{
  double x,y,z,x_res,y_res,z_res;
  double xmin: -1.2
  double xmax: 0.0
  double ymin: -0.6
  double ymax: 0.4
  double zmin: 0.0
  double zmax: 1.0

  KDL::Vector point_pos(msg->data[0],msg->data[1],msg->data[2]);
  // x =  msg->data[0];
  // y =  msg->data[1];
  // z =  msg->data[2];
  x_res =  msg->data[3];
  y_res =  msg->data[4];
  z_res =  msg->data[5];

  vector<vector<vector< KDL::Vector> > > array3D;

  for(int i = xmin, i <= xmax , i = i + x_res)
  {
   for(int j = ymin, j <= ymax , j = j + y_res)
   {    
      for(int k = zmin, k <= zmax , k = k + k_res)
      {
        KDL::Vector point_(i,j,k);
        array3D[i][j][k] = GetRepulsiveWithTable(point_) + GetRepulsiveWithObstacles(point_);
        DrawArrow(array3D[i][j][k], point_);
      } 
    }
  }


}

DrawArrow( KDL::Vector &gridspace_Force, KDL::Vector &gridspace_point )
{
    uint32_t shape = visualization_msgs::Marker::ARROW;
    visualization_msgs::Marker marker;
    // Set the frame ID and timestamp.  See the TF tutorials for information on these.
    marker.header.frame_id = "/my_frame";
    marker.header.stamp = ros::Time::now();
    marker.ns = "basic_shapes";
    marker.id = Force_0;
    marker.type = shape;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.x = gridspace_point.x;
    marker.pose.position.y = gridspace_point.y;
    marker.pose.position.z = gridspace_point.z;
    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.pose.orientation.w = 1.0;
    marker.scale.x = 1.0;
    marker.scale.y = 1.0;
    marker.scale.z = 1.0;
    marker.color.r = 0.0f;
    marker.color.g = 1.0f;
    marker.color.b = 0.0f;
    marker.color.a = 1.0;
    marker.lifetime = ros::Duration();
    

}


KDL::Vector grid::GetRepulsiveWithTable(KDL::Vector &point_pos)
{
  //repulsive with table
  KDL::Vector Table_position(0,0,0.15);
      double Rep_inf_table = 0.10;
      Eigen::Matrix<double,6,1> Force = Eigen::Matrix<double,6,1>::Zero();
      std::vector<double distance_local_obj;
      std::vector<double> distance_influence;
      Eigen::Vector3d vec_Temp(0,0,0);
      std::vector<int> index_infl;
      int index_dist = 0;
      Eigen::Matrix<double,7,1> F_rep = Eigen::Matrix<double,7,1>::Zero();
      KDL::Vector vect_force(0,0,0);

      distance_local_obj.push_back(- Table_position.z()+ point_pos.z());
      
        if(distance_local_obj[i] < Rep_inf_table )
        {
          distance_influence.push_back(distance_local_obj[i] );
          index_infl.push_back(i);

        }
        else
        {
          continue;
        }
      
      
      //std::cout<<"distance_influence.size(): "<<distance_influence.size()<<std::endl;
      
      if(distance_influence.size() != 0)
      {
        //min element in the vector and index
        std::vector<double>::iterator result = std::min_element(std::begin(distance_influence), std::end(distance_influence));
        index_dist = std::distance(std::begin(distance_influence), result);
        double min_distance;
        min_distance = distance_influence[index_dist];
            
        double Ni_ = 0.1; //repulsive gain
        Eigen::Vector3d distance_der_partial(0,0,1);  //direction of the field
        
        int index_jacobian = index_infl[index_dist] % 1; 
        // std::cout<<"index_jacobian: "<<index_jacobian<<std::endl;
        
        vec_Temp = (Ni_/pow(min_distance,2)) * (1/min_distance - 1/Rep_inf_table) * distance_der_partial;

          Force.row(0) << vec_Temp[0];
          Force.row(1) << vec_Temp[1];
          Force.row(2) << vec_Temp[2];
          Force.row(3) <<  0;
          Force.row(4) <<  0;
          Force.row(5) <<  0;

          vect_force.x = vec_Temp[0];
          vect_force.y = vec_Temp[1];
          vect_force.z = vec_Temp[2]);
      }


      return vect_force;
}


KDL::Vector grid::GetRepulsiveWithObstacles(KDL::Vector &Pos_)
{
    std::vector<double> distance_local_obj;
    Eigen::Matrix<double,7,1> F_rep_tot = Eigen::Matrix<double,7,1>::Zero();
    std::vector<Eigen::Matrix<double,7,1> > F_rep;      
    int index_dist;
    KDL::Vector vect_force(0,0,0);
    //F_rep_total = SUM(F_rep_each_ostacles)
    // std::cout<<"Object_position.size(): "<<Object_position.size()<<std::endl;
    for(unsigned int i=0; i < Object_position.size();i++)
    {
        std::vector<double>  min_d;
        std::vector<unsigned int> index_infl;
        double influence = Object_radius[i] +  0.20;
        // std::cout<<"influence: "<<influence<<std::endl;
         distance_local_obj.push_back( (diff(Object_position[i].p,Pos_)).Norm() );
        // std::cout<<"distance_local_obj.size(): "<<distance_local_obj.size()<<std::endl;
        for(unsigned int j=0; j< distance_local_obj.size(); j++ )
        {
          if( distance_local_obj[j] <= influence )
          {
            min_d.push_back(distance_local_obj[j]);
            index_infl.push_back(j);
          }
        }

        if(min_d.size() > 0)
        {

          std::vector<double>::iterator result = std::min_element(std::begin(min_d), std::end(min_d));
          index_dist = std::distance(std::begin(min_d), result);
          double min_distance;
          min_distance = min_d[index_dist];

            //double min_distance = distance_local_obj[j];
            // std::cout<<"min_distance: "<<min_distance<<std::endl;
             
            Eigen::Vector3d distance_der_partial(0,0,0);
            Eigen::Vector3d vec_Temp(0,0,0);
              
            int index_obj = i; //floor(index_infl[index_dist]/2) ;
            // std::cout<<"index_obj: "<<index_obj<<std::endl;
            int index_jac = index_infl[index_dist] % 1; 
            // index_infl[index_dist] % 5;
            // std::cout<<"index_jac: "<<index_jac<<std::endl;
            Eigen::Matrix<double,6,1> Force = Eigen::Matrix<double,6,1>::Zero();
             // distance_der_partial = x^2/radius + y^2 / radius + 2*(z^2n) /l
      
            distance_der_partial[0] = (Object_position[index_obj].p.x()*2 / Object_radius[index_obj] );
            // std::cout<<"distance_der_partial[0]: "<<distance_der_partial[0]<<std::endl;
            distance_der_partial[1] = (Object_position[index_obj].p.y()*2 / Object_radius[index_obj] );
            // std::cout<<"distance_der_partial[1]: "<<distance_der_partial[1]<<std::endl;
            // distance_der_partial[2] = (pow(Object_position[index_obj].p.z(),7)*16 / Object_height[index_obj] ); //n=2
            distance_der_partial[2] = (Object_position[index_obj].p.z()*4 / Object_height[index_obj] ); //n=1
            // std::cout<<"distance_der_partial[2]: "<<distance_der_partial[2]<<std::endl;

            double Ni_ = 1.0;
            
            // vec_Temp = (Ni_/pow(min_distance,2)) * (1/min_distance - 1/influence) * distance_der_partial;
            // std::cout<<"qui"<<std::endl;
                  
            Force(0) = (Ni_/pow(min_distance,2)) * (1.0/min_distance - 1.0/influence) * distance_der_partial[0];
            Force(1) = (Ni_/pow(min_distance,2)) * (1.0/min_distance - 1.0/influence) * distance_der_partial[1];
            Force(2) = (Ni_/pow(min_distance,2)) * (1.0/min_distance - 1.0/influence) * distance_der_partial[2];
            Force(3) = 0;
            Force(4) = 0;
            Force(5) = 0;

            vect_force.x = Force(0);
            vect_force.y = Force(1);
            vect_force.z = Force(2);
        }
    }
    
    return vect_force;
}






























#endif

