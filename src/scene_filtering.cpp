// ROS headers
#include <ros/ros.h>
#include <ros/console.h>
#include <pcl_ros/point_cloud.h>
#include <std_srvs/Empty.h>
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud_conversion.h>
//PCL
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/common/transforms.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
//Eigen
#include <Eigen/Dense>
//tf
// #include <tf2_sensor_msgs/tf2_sensor_msgs.h>
// #include <tf2_ros/transform_listener.h>
// #include <geometry_msgs/TransformStamped.h>
// #include <geometry_msgs/Twist.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_datatypes.h>
#include <tf_conversions/tf_eigen.h>

//general utilities
#include <string>
#include <stdlib.h>

class sceneFilter
{
  public:
    sceneFilter();
    //Node handle
    ros::NodeHandle nh;
  private:

    //Service Server
    // ros::ServiceServer srv_acquire_;
    
    //Message Subscriber
    ros::Subscriber sub_stream_;
    
    //Message Publisher
    ros::Publisher pub_stream_;
    
    //Message callback, gets executed when a new message is available on topic
    void new_cloud_in_stream(const sensor_msgs::PointCloud2::ConstPtr& message);

    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr scene_stream_;

    //parameters
    bool filter_, downsample_, keep_organized_, change_frame_, erase_plane_;
    double xmin,xmax,ymin,ymax,zmin,zmax,leaf_;
    std::string new_frame_, camera_frame_;

    // geometry_msgs::TransformStamped tf_transform_;
    Eigen::Affine3d transform_;

    //Transformation 

};

//Constructor
sceneFilter::sceneFilter()
{
  nh = ros::NodeHandle("scene_filter_node");
  pcl::PointCloud<pcl::PointXYZRGBA> a;
  scene_stream_ = a.makeShared();

  //subscribe to depth_registered pointclouds topic
  std::string topic = nh.resolveName("/camera/depth_registered/points");
  sub_stream_ = nh.subscribe(topic, 1, &sceneFilter::new_cloud_in_stream, this);

  pub_stream_ = nh.advertise<pcl::PointCloud<pcl::PointXYZRGBA> > ("/scene_filter/scene",1);

  //load parameters
  nh.param<bool>("/scene_filter/filter", filter_, "false");
  nh.param<bool>("/scene_filter/downsample", downsample_, "false");
  nh.param<bool>("/scene_filter/keep_organized", keep_organized_, "false");
  nh.param<bool>("/scene_filter/change_frame", change_frame_, "false");
  nh.param<bool>("/scene_filter/erase_plane", erase_plane_, "false");
  nh.param<double>("/scene_filter/xmin", xmin, -100);
  nh.param<double>("/scene_filter/xmax", xmax, 100);
  nh.param<double>("/scene_filter/ymin", ymin, -100);
  nh.param<double>("/scene_filter/ymax", ymax, 100);
  nh.param<double>("/scene_filter/zmin", zmin, -100);
  nh.param<double>("/scene_filter/zmax", zmax, 100);
  nh.param<double>("/scene_filter/leaf_s", leaf_, 0.005);
  nh.param<std::string>("/scene_filter/camera_frame", camera_frame_, "camera_rgb_optical_frame");

  transform_ = Eigen::Affine3d::Identity();

  if (change_frame_)
  {

    nh.param<std::string>("/scene_filter/new_frame", new_frame_, "");
    ROS_INFO("Waiting for transform from %s to %s", new_frame_.c_str(), camera_frame_.c_str());
    tf::TransformListener listener_transform;
    tf::StampedTransform transformToBaseLink;
    if (listener_transform.waitForTransform(new_frame_.c_str(), camera_frame_.c_str() , ros::Time::now(), ros::Duration(10)))
    {
      listener_transform.lookupTransform(new_frame_.c_str(), camera_frame_.c_str() , ros::Time(0), transformToBaseLink);
      ROS_INFO("Transform from %s to %s exists", new_frame_.c_str(), camera_frame_.c_str());
    }
    else
    {
      ROS_INFO("Transform from %s to %s does not exists, Identity will be used", new_frame_.c_str(), camera_frame_.c_str());
    }
    tf::transformTFToEigen(transformToBaseLink, transform_);
  }

}

void sceneFilter::new_cloud_in_stream(const sensor_msgs::PointCloud2::ConstPtr& message)
{
  pcl::PointCloud<pcl::PointXYZRGBA>::Ptr tmp (new pcl::PointCloud<pcl::PointXYZRGBA>);
  //constantly copy cloud from stream into class scene_stream_ to be accessible for service callback
  pcl::fromROSMsg (*message, *tmp);

  if (change_frame_)
  {
    pcl::transformPointCloud (*tmp, *scene_stream_, transform_);
    pcl::copyPointCloud(*scene_stream_, *tmp);
    tmp->header.frame_id = new_frame_.c_str();
  }

  //check if we need to filter stream
  nh.getParam("/scene_filter/filter", filter_);
  if (filter_)
  {
    nh.getParam("/scene_filter/xmin", xmin);
    nh.getParam("/scene_filter/xmax", xmax);
    nh.getParam("/scene_filter/ymin", ymin);
    nh.getParam("/scene_filter/zmin", zmin);
    nh.getParam("/scene_filter/ymax", ymax);
    nh.getParam("/scene_filter/zmax", zmax);
    nh.getParam("/scene_filter/keep_organized", keep_organized_);
    pcl::PassThrough<pcl::PointXYZRGBA> pass;
    if (keep_organized_)
    {
      pass.setKeepOrganized(true);
    }
    pass.setInputCloud (tmp);
    pass.setFilterFieldName ("z");
    pass.setFilterLimits (zmin, zmax);
    pass.filter (*tmp);
    pass.setInputCloud (tmp);
    pass.setFilterFieldName ("y");
    pass.setFilterLimits (ymin, ymax);
    pass.filter (*tmp);
    pass.setInputCloud (tmp);
    pass.setFilterFieldName ("x");
    pass.setFilterLimits (xmin, xmax);
    pass.filter (*scene_stream_);
    pcl::copyPointCloud(*scene_stream_ , *tmp);
  }

  if (erase_plane_)
  {
      // Create the segmentation object for the planar model and set all the parameters
    pcl::SACSegmentation<pcl::PointXYZRGBA> seg;
    pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
    pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);

    seg.setOptimizeCoefficients (true);
    seg.setModelType (pcl::SACMODEL_PLANE);
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setMaxIterations (100);
    seg.setDistanceThreshold (0.005);

    // Segment the largest planar component from the remaining cloud
    seg.setInputCloud (tmp);
    seg.segment (*inliers, *coefficients);

    if (inliers->indices.size () == 0)
      {
        ROS_INFO( "Could not estimate a planar model for the given dataset." );

      }

    pcl::ExtractIndices<pcl::PointXYZRGBA> extract;
    extract.setInputCloud (tmp);
    extract.setIndices (inliers);
    extract.setNegative (true);
    extract.filter (*scene_stream_);
    
    pcl::copyPointCloud(*scene_stream_ , *tmp);
  }



  //check if we need to downsample stream
  nh.getParam("/scene_filter/downsample", downsample_);
  if (downsample_)
  {
    nh.getParam("/scene_filter/leaf_s", leaf_);
    pcl::VoxelGrid<pcl::PointXYZRGBA> vg;
    vg.setInputCloud (tmp);
    vg.setLeafSize(leaf_, leaf_, leaf_);
    vg.filter (*scene_stream_);
  }

  pub_stream_.publish( *tmp ); //republish the modified scene
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "scene_filter_node");
  sceneFilter node;
  ROS_INFO("[sceneFilter] Node is ready");

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
