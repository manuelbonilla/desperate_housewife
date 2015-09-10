//#include "camera_desperatehousewife.h"
#include "phobic_camera.h"
using Eigen::VectorXf ;


void phobic_scene::pointcloudCallback(sensor_msgs::PointCloud2 msg)
{
	//std::cout<<"sono nella callback"<<std::endl;
	// create a new pointcloud from msg
	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr scene (new pcl::PointCloud<pcl::PointXYZRGBA>);

	//if camera doesn't present (set this param during the launch)
	if(pc_save == 1)
	{
		pcl::io::loadPCDFile("/home/daniela/Desktop/dany_bicchi/test_pcd.pcd", *scene);
	}
	
	else
	{	
		pcl::fromROSMsg (msg, *scene);
	}


	

	//visualization(true , false);
	// ROS_INFO("Strating timer");
	// ros::Time begin = ros::Time::now();
	erase_environment(scene);
	/*pcl::copyPointCloud<pcl::PointXYZRGBA>(*scene, *cloud_scene);*/
	// if (scene->points.empty ())
	// {
	// 	ROS_INFO( "Empty point cloud" );
	// }

	// ros::Duration left = ros::Time::now() - begin;
	// ROS_INFO("Time to Erase environment %lf", left.toSec());
	 // visualization(true, false);
	// begin = ros::Time::now();
	erase_table();

	if (cloud->points.empty ())
	{
		ROS_INFO( "Empty point cloud" );
	}
	
	else
	{
		// left = ros::Time::now() - begin;
		// ROS_INFO("Time to Erase table environment %lf", left.toSec());
		//visualization(true, false);
		// begin = ros::Time::now();
		getcluster();
		// left = ros::Time::now() - begin;
		// ROS_INFO("Time to get clusters environment %lf", left.toSec());
		//visualization(false, false);
		// begin = ros::Time::now();
		if ( object_cluster.size() > 0)
		{
			fitting();
			// left = ros::Time::now() - begin;
			// ROS_INFO("Time to fitting environment %lf", left.toSec());

			// send_msg();

		}

		desperate_housewife::cyl_info msg;
		msg.dimension = cyl_list.size();

		for(int i=0; i<cyl_list.size();i++)
		{
			msg.length.push_back(cyl_list[i].height);
			msg.radius.push_back(cyl_list[i].radius);
			msg.Info.push_back(cyl_list[i].Info);
			msg.vol.push_back(cyl_list[i].vol);
			//msg.Transform.poses.push_back(cyl_list[i].Cyl_pose);
			std::cout<<"info radius "<< cyl_list[i].radius <<std::endl;
			ROS_INFO("cyl is empty (=0) or full (1): %g", cyl_list[i].vol);
			ROS_INFO("cylinder if standind (=0) or is lying: %g", cyl_list[i].Info);
			//std::cout<<"info empty "<< cyl_list[i].vol <<std::endl;	// 0 in empty 1 is full
		}
		
		num_cyl.publish(msg);
		msg.length.clear();
		msg.radius.clear();
		msg.Info.clear();
		msg.vol.clear();
		//msg.Transform.clear();

	}

}



void phobic_scene::erase_environment(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr  original_pc)
{
	//std::cout<<"funzione togli ambiente"<<std::endl;
	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZRGBA>);
	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_temp (new pcl::PointCloud<pcl::PointXYZRGBA>);
	
	//pcl::copyPointCloud<pcl::PointXYZ>(*original_pc, inliers, *environment);
	pcl::PassThrough<pcl::PointXYZRGBA> Filter_env;

	// x
	Filter_env.setInputCloud (original_pc);
	Filter_env.setFilterFieldName ("x");
	Filter_env.setFilterLimits (-0.3, 0.3);
	Filter_env.filter (*cloud_filtered);
	// The indices_x array indexes all points of cloud_in that have x between -0.2, 0.7
	
	//y
	Filter_env.setInputCloud (cloud_filtered);
	Filter_env.setFilterFieldName ("y");
	Filter_env.setFilterLimits (-1.0, 1.0);
	// The indices_x array indexes all points of cloud_in that have x between -1.0, 1.0
	Filter_env.filter (*cloud_temp);

	//z
	Filter_env.setInputCloud (cloud_temp);
	Filter_env.setFilterFieldName ("z");
	Filter_env.setFilterLimits (0.2, 1.0);
	// The indices_x array indexes all points of cloud_in that have x between 0.2, 1.0
	Filter_env.filter (*cloud);

	//std::cout<<"finito funzione togli ambiente"<<std::endl;

}


void phobic_scene::fitting ()
{	
	int a = 0;
	cyl_list.clear();
	while ( object_cluster.size() > 0 )
	{

		pcl::PointCloud<pcl::Normal>::Ptr cloud_normals (new pcl::PointCloud<pcl::Normal>);
		pcl::search::KdTree< pcl::PointXYZRGBA>::Ptr tree (new pcl::search::KdTree< pcl::PointXYZRGBA> ());
		pcl::NormalEstimation< pcl::PointXYZRGBA, pcl::Normal> ne;
		pcl::ExtractIndices< pcl::PointXYZRGBA> extract;
		pcl::PointIndices::Ptr inliers_cylinder (new pcl::PointIndices);
		pcl::ModelCoefficients::Ptr coefficients_cylinder (new pcl::ModelCoefficients);
		pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_first (new pcl::PointCloud<pcl::PointXYZRGBA>);

		pcl::copyPointCloud<pcl::PointXYZRGBA>( *object_cluster.begin(), *cloud_first);
		 // pcl::PointCloud<pcl::PointXYZRGBA>::Ptr msg (new pcl::PointCloud<pcl::PointXYZRGBA>);
		 // msg->header.frame_id = "camera_rgb_optical_frame";
		 //  msg->height = msg->width = 1;
		 //  msg->points.push_back (pcl::PointXYZ(1.0, 2.0, 3.0));

		cloud_first->header.frame_id = "camera_rgb_optical_frame";

		pub_cloud_object.publish( cloud_first  ); //for visualizer in rviz


		ne.setSearchMethod (tree);
		ne.setInputCloud (cloud_first);
		ne.setKSearch (100);
		ne.compute (*cloud_normals);

    	// std::cout << " number of input cloud" << cloud_normals->points.size() << std::endl;

		if (cloud_normals->points.empty ())
		{
			ROS_INFO( "Not possible to get normals from cluster" );
			object_cluster.pop_front();
			continue;
		}

		// Create the segmentation object for cylinder segmentation and set all the parameters
		pcl::SACSegmentationFromNormals< pcl::PointXYZRGBA, pcl::Normal> seg;
		seg.setOptimizeCoefficients (true);
		seg.setModelType (pcl::SACMODEL_CYLINDER);
		//seg.setMethodType (pcl::SAC_RANSAC);
		seg.setMethodType (pcl::SAC_LMEDS );
		// seg.setNormalDistanceWeight (0.1);
		seg.setMaxIterations (10000);
		seg.setDistanceThreshold (0.05);	//distance between two points
	    // seg.setRadiusLimits (0, 0.2);	//cilynder's radius 20cm
	 	seg.setInputCloud (cloud_first);
	  	seg.setInputNormals (cloud_normals);

		//// Obtain the cylinder inliers and coefficients
		seg.segment (*inliers_cylinder, *coefficients_cylinder);
		// std::cout << "Cylinder coefficients: " << *coefficients_cylinder << std::endl;

		if (inliers_cylinder->indices.size() == 0)
		{
		  	ROS_INFO( "Could not find a cylinder in cluster %d.", a );
		  	object_cluster.pop_front();
		  	continue;
		}
		
		else
		{
			// Write the cylinder inliers to disk
		  	extract.setInputCloud (cloud_first);
		  	extract.setIndices (inliers_cylinder);
		  	extract.setNegative (false);
		  	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_cylinder (new pcl::PointCloud<pcl::PointXYZRGBA> ());
		  	extract.filter (*cloud_cylinder);
			// 	if (cloud_cylinder->points.empty ())
			// {
			// 	ROS_INFO("empty pointcloud");
			// 	continue;
			// 	}
		  	// pcl::copyPointCloud<pcl::PointXYZRGBA>( *cloud_cylinder, *cloud);

			// take the coefficients and make a transformation matrix from camera_frame to axis_cylinder's_frame
		  	makeInfoCyl(coefficients_cylinder->values, cloud_cylinder, cloud_first);
		  	//std::cout << "Cluster"<< a <<"Cylinder coefficients: " << *coefficients_cylinder << std::endl;
		  	// send a pose_msg for the visualizer markker
		  	send_msg( a );
		  	// send a tf_msg for the motion planning 
		  	
		  	std::string cyl= "cilindro_" + std::to_string(a);
		  	tf_br.sendTransform(tf::StampedTransform(cyl_list[a].cyl_tf_pose, ros::Time::now(), "/camera_rgb_optical_frame", cyl.c_str()));

		  	a = a + 1;
		  	// remove the first elements in the list
			object_cluster.pop_front();
				
		}

		
			
	}
}

void phobic_scene::makeInfoCyl(std::vector<float> coeff , pcl::PointCloud<pcl::PointXYZRGBA>::Ptr pc_cyl, pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_scene)
{
	// cylinder's coefficients
	//Eigen::Vector3f position(coeff[3], coeff[4], coeff[5]);
	// double x = coeff[0], y = coeff[1], z = coeff[2];	// cordinate di un punto sull asse
	// double p_x = coeff[3], p_y = coeff[4], p_z = coeff[5];	// componenti dell asse realtive alla telecamera
	// double r = coeff[6];
	//create a new frame
	CYLINDER.radius = coeff[6];

	//k = kinect frame, c = cylinder frame
	Eigen::Matrix<double,4,4> Matrix_transform; //T_k_c  
	Matrix_transform = Cyl_Transform( coeff); 

	CYLINDER.Matrix_transform_inv = Matrix_transform.inverse(); //T_c_k
	
	//create a new cylinder pointcloud
	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr CYl_new_transf (new pcl::PointCloud<pcl::PointXYZRGBA>);
	
	pcl::transformPointCloud(*pc_cyl, *CYl_new_transf, CYLINDER.Matrix_transform_inv); // point cloud in T_c
	// pcl::copyPointCloud<pcl::PointXYZRGBA>( *CYl_new_transf, *cloud);


	//find height 
	// double z_min, z_max;
	std::vector<double> height_vect;
	height_vect = findHeight(CYl_new_transf);
	// height_vecté(0) --> Zmax;
	// height_vecté(1) --> z_min;

	// z_min= CYl_new_transf->points[0].z;
	// z_max= CYl_new_transf->points[0].z;
	

	// for (int i=1; i< CYl_new_transf->points.size(); i++)
	// {
	// 	if(z_min > CYl_new_transf->points[i].z )
	// 	{	
	// 		z_min = CYl_new_transf->points[i].z;
			
	// 	}

	// 	if(z_max < CYl_new_transf->points[i].z)
	// 	{	
	// 		z_max = CYl_new_transf->points[i].z;
			
	// 	}
	// }
	CYLINDER.height = height_vect[0] - height_vect [1]; 
	//CYLINDER.height = z_max - z_min; // findHeight(CYl_new_transf)
	//std::cout<< "altezza: "<< CYLINDER.height <<std::endl;

	// find a center
	CYLINDER.center.z = 1.*(height_vect[1] +  (CYLINDER.height/2));
	CYLINDER.center.x = 0;
	CYLINDER.center.y = 0;
	// g = center of the cylinder
	Eigen::Matrix4d M_center = Eigen::Matrix4d::Identity(); // T_c_g
	M_center(0,3) = CYLINDER.center.x ;
	M_center(1,3) = CYLINDER.center.y ;
	M_center(2,3) = CYLINDER.center.z ;


	//T_k_g = M_center*CYLINDER.Matrix_transform_inv;
	T_k_g = Matrix_transform * M_center; // T_k_g
	//////testing///////
	//T_k_g = Eigen::Matrix4d::Identity();
	Eigen::Matrix4d t_g_k;
	t_g_k= T_k_g.inverse();
	StandingOrLying(t_g_k);


	// Eigen::Vector3d u(0,-1,0);
	// Eigen::Vector3d plane_coeff_kinect_frame, temp_n2;
	// Eigen::Vector4d normal_plane_cyl_frame, temp_normal_plane;
	// plane_coeff_kinect_frame(0) = Plane_coeff[0];
	// plane_coeff_kinect_frame(1) = Plane_coeff[1];
	// plane_coeff_kinect_frame(2) = Plane_coeff[2];


	// if ((u.dot(plane_coeff_kinect_frame)) <0) // in kinect frame
	// {
	// 	plane_coeff_kinect_frame = plane_coeff_kinect_frame * -1;
	// }

	// // in cyl frame
	// temp_normal_plane << plane_coeff_kinect_frame, 1;
	// normal_plane_cyl_frame = t_g_k * temp_normal_plane;
	// normal_plane_cyl_frame.normalize();
	
	// Eigen::Vector3d axis_cyl_frame(0,0,1);
	// temp_n2(0) = normal_plane_cyl_frame(0);
	// temp_n2(1) = normal_plane_cyl_frame(1);
	// temp_n2(2) = normal_plane_cyl_frame(2);

	// temp_n2.normalize();
	// double dotproduct = temp_n2.dot(axis_cyl_frame);

	// double theta = std::acos(dotproduct);
	// std::cout<<"theta: "<<theta<<std::endl;
	// if(((theta >= 0) && (theta<30*(3.14/180))) || ((theta <0) && (theta > -30*(3.14/180))))
	// {
	// 	CYLINDER.Info = 0;
	// }
	// else 
	// {	
	// 	CYLINDER.Info = 1;
	// }




	//pcl::PointCloud<pcl::PointXYZRGBA>::Ptr CYl_test (new pcl::PointCloud<pcl::PointXYZRGBA>);

	//pcl::transformPointCloud(*CYl_new_transf, *CYl_test, CYLINDER.Matrix_transform_inv); //Point cloud in T_g
	//pcl::copyPointCloud<pcl::PointXYZRGBA>( *CYl_test, *cloud);


	if (CYLINDER.Info == 0) // solo sse è dritto guardo se è vuoto (mi servira per sapere dove mettere la mano)
	{	
		
		pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_test (new pcl::PointCloud<pcl::PointXYZRGBA>);
		pcl::transformPointCloud(*cloud_scene, *cloud_test, t_g_k);
		CYLINDER.vol = FullorEmpty(cloud_test);

	}
	else
	{
		CYLINDER.vol = 1;
	}


	
	bool ok_transf;
	ok_transf = fromEigenToPose( T_k_g , CYLINDER.Cyl_pose );
	// std::cout<<T_k_g <<"T_K_G" <<std::endl<<std::flush;
	std::cout<<CYLINDER.Cyl_pose.position<<"cyl_pose position" <<std::endl<<std::flush;
	std::cout<<CYLINDER.Cyl_pose.orientation<<"cyl_pose orientation" <<std::endl<<std::flush;
	

	tf::poseMsgToTF(CYLINDER.Cyl_pose, CYLINDER.cyl_tf_pose );
	std::cout<<CYLINDER.cyl_tf_pose.getOrigin().getX()<<CYLINDER.cyl_tf_pose.getOrigin().getY()<<CYLINDER.cyl_tf_pose.getOrigin().getZ() <<"cyl_tf trans" <<std::endl<<std::flush;
	std::cout<<CYLINDER.cyl_tf_pose.getRotation().getX()<< CYLINDER.cyl_tf_pose.getRotation().getY()<<CYLINDER.cyl_tf_pose.getRotation().getW()<<"cyl_tf rot" <<std::endl<<std::flush;
	

  	// pcl::ModelCoefficients cylinder_coeff;
	CYLINDER.cylinder_coeff.values.resize (7);    // We need 7 values
	CYLINDER.cylinder_coeff.values[0] = 0;
	CYLINDER.cylinder_coeff.values[1] = 0;
	CYLINDER.cylinder_coeff.values[2] = -CYLINDER.height/2;
	CYLINDER.cylinder_coeff.values[3] = 0;
	CYLINDER.cylinder_coeff.values[4] = 0;
	CYLINDER.cylinder_coeff.values[5] = 1;
	CYLINDER.cylinder_coeff.values[6] = CYLINDER.radius;

	cyl_list.push_back(CYLINDER);

}

void  phobic_scene::StandingOrLying(Eigen::Matrix4d &T_G_K)
{


	Eigen::Vector3d u(0,-1,0);
	Eigen::Vector3d plane_coeff_kinect_frame, temp_n2;
	Eigen::Vector4d normal_plane_cyl_frame, temp_normal_plane;
	plane_coeff_kinect_frame(0) = Plane_coeff[0];
	plane_coeff_kinect_frame(1) = Plane_coeff[1];
	plane_coeff_kinect_frame(2) = Plane_coeff[2];


	if ((u.dot(plane_coeff_kinect_frame)) <0) // in kinect frame
	{
		plane_coeff_kinect_frame = plane_coeff_kinect_frame * -1;
	}

	// in cyl frame
	temp_normal_plane << plane_coeff_kinect_frame, 1;
	normal_plane_cyl_frame = T_G_K * temp_normal_plane;
	normal_plane_cyl_frame.normalize();
	
	Eigen::Vector3d axis_cyl_frame(0,0,1);
	temp_n2(0) = normal_plane_cyl_frame(0);
	temp_n2(1) = normal_plane_cyl_frame(1);
	temp_n2(2) = normal_plane_cyl_frame(2);

	temp_n2.normalize();
	double dotproduct = temp_n2.dot(axis_cyl_frame);

	double theta = std::acos(dotproduct);
	std::cout<<"theta: "<<theta<<std::endl;
	if(((theta >= 0) && (theta<45*(3.14/180))) || ((theta <0) && (theta > -45*(3.14/180))))
	{
		CYLINDER.Info = 0;
	}
	else 
	{	
		CYLINDER.Info = 1;
	}
}


std::vector<double> findHeight(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_t)
{
	double z_min, z_max;
	std::vector<double> V_height;
	z_min= cloud_t->points[0].z;
	z_max= cloud_t->points[0].z;
	

	for (int i=1; i< cloud_t->points.size(); i++)
	{
		if(z_min > cloud_t->points[i].z )
		{	
			z_min = cloud_t->points[i].z;
			
		}

		if(z_max < cloud_t->points[i].z)
		{	
			z_max = cloud_t->points[i].z;
			
		}
	}

	V_height.push_back(z_max);
	V_height.push_back(z_min);

	return V_height;

}


int phobic_scene::FullorEmpty(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud22)
{
	pcl::PointXYZRGBA point_center; //punto in mezzo
	// point_center.x = CYLINDER.center.x;
	// point_center.y = CYLINDER.center.y;
	// // point_center.z = 1 + CYLINDER.height;
	// point_center.z = CYLINDER.center.z + CYLINDER.height/2;

	std::vector<double> height_vect;
	height_vect = findHeight(cloud22);
	// height_vect(0) --> Zmax;
	// height_vect(1) --> z_min;

	// double z_min, z_max;
	// z_min= cloud22->points[0].z;
	// z_max= cloud22->points[0].z;
	
	// for (int i=1; i< cloud22->points.size(); i++)
	// {
	// 	if(z_min > cloud22->points[i].z )
	// 	{	
	// 		z_min = cloud22->points[i].z;
			
	// 	}

	// 	if(z_max < cloud22->points[i].z)
	// 	{	
	// 		z_max = cloud22->points[i].z;
			
	// 	}
	// }
	
	point_center.x = CYLINDER.center.x;
	point_center.y = CYLINDER.center.y;
	point_center.z = height_vect[0];

	std::vector< int >  k_indices;
	std::vector< float > k_sqr_distances;
	int test_v;
	double dim_s;

	pcl::search::KdTree<pcl::PointXYZRGBA>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZRGBA>);
  	tree->setInputCloud (cloud22);

  	// if(CYLINDER.radius <= 0.05)
  	// {
  	// 	dim_s = CYLINDER.radius;
  	// }
  	// else
  	// {
  	// 	dim_s = CYLINDER.radius/3;
  	// }
  	dim_s = CYLINDER.radius*0.6; //less than half
  	
	tree->radiusSearch(point_center, dim_s, k_indices, k_sqr_distances,0 );
	ROS_INFO("k_indices %d", k_indices.size());

	if(k_indices.size() < (dim_s*300))
	{
		test_v = 0; // empty
	}
	else
	{
		test_v = 1; // full
	}

	return test_v;
}


bool phobic_scene::fromEigenToPose(Eigen::Matrix4d &tranfs_matrix, geometry_msgs::Pose &pose)
{
	Eigen::Matrix<double,3,3> Tmatrix;
	Tmatrix = tranfs_matrix.block<3,3>(0,0) ;
	Eigen::Quaterniond cyl_quat_eigen(Tmatrix);
	pose.orientation.x = cyl_quat_eigen.x();
	pose.orientation.y = cyl_quat_eigen.y();
	pose.orientation.z = cyl_quat_eigen.z();
	pose.orientation.w = cyl_quat_eigen.w();
	pose.position.x = tranfs_matrix(0,3);
	pose.position.y = tranfs_matrix(1,3);
	pose.position.z = tranfs_matrix(2,3);


	return true;
}


void  phobic_scene::send_msg( int cyl_id )
{
 	ROS_INFO("send a marker msg");

	visualization_msgs::Marker marker;
	marker.header.frame_id = "camera_rgb_optical_frame";
	marker.header.stamp = ros::Time();
	marker.ns = "";
	marker.id = cyl_id;
	marker.type = visualization_msgs::Marker::CYLINDER;
	marker.action = visualization_msgs::Marker::ADD;
	marker.pose.position.x = cyl_list[cyl_id].Cyl_pose.position.x;
	marker.pose.position.y = cyl_list[cyl_id].Cyl_pose.position.y;
	marker.pose.position.z = cyl_list[cyl_id].Cyl_pose.position.z;
	marker.pose.orientation.x = cyl_list[cyl_id].Cyl_pose.orientation.x;
	marker.pose.orientation.y = cyl_list[cyl_id].Cyl_pose.orientation.y;
	marker.pose.orientation.z = cyl_list[cyl_id].Cyl_pose.orientation.z;
	marker.pose.orientation.w = cyl_list[cyl_id].Cyl_pose.orientation.w;
	marker.scale.x = cyl_list[cyl_id].radius * 2.;
	marker.scale.y = cyl_list[cyl_id].radius * 2.;
	marker.scale.z = cyl_list[cyl_id].height;
	marker.color.a = 1.0; // for the clearness
	marker.color.r = 0.0;
	marker.color.g = 0.0;
	marker.color.b = 1.0;
	marker.lifetime = ros::Duration(1);
	
	phobic_talk.publish(marker); 
}


void phobic_scene::getcluster()
{
	// try with euclidean clusters
	// Creating the KdTree object for the search method of the extraction
	pcl::search::KdTree<pcl::PointXYZRGBA>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZRGBA>);

	// ROS_INFO("PointCloud before filtering has: %d data points.", cloud->points.size ());

	// Create the filtering object: downsample the dataset using a leaf size of 0.5cm
	pcl::VoxelGrid<pcl::PointXYZRGBA> vg;
	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZRGBA>);
	vg.setInputCloud (cloud);
	vg.setLeafSize (down_sample_size, down_sample_size, down_sample_size);
	vg.filter (*cloud_filtered);

	// ROS_INFO("PointCloud after filtering has: %d data points.", cloud_filtered->points.size ());
	if (cloud_filtered->points.empty())
	{
		ROS_INFO( "Empty cloud after downsampling");

		return;
	}

	tree->setInputCloud (cloud_filtered);

    ROS_INFO("PointCloud in getcluster() has: %d data points.", cloud_filtered->points.size ());
	std::vector<pcl::PointIndices> cluster_indices;
	pcl::EuclideanClusterExtraction<pcl::PointXYZRGBA> ec;
  	ec.setClusterTolerance (0.07); // 5cm
  	ec.setMinClusterSize (100);
  	ec.setMaxClusterSize (cloud_filtered->points.size());
  	ec.setSearchMethod (tree);
  	ec.setInputCloud (cloud_filtered);
  	ec.extract (cluster_indices);

  	// ROS_INFO( "Number of clusters in the scene %d", cluster_indices.size());

  	if (cluster_indices.size() > 0)
  	{

  		object_cluster.clear();

  		for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin (); it != cluster_indices.end (); ++it)
  		{
  			pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_cluster (new pcl::PointCloud<pcl::PointXYZRGBA>);

  			for (std::vector<int>::const_iterator pit = it->indices.begin (); pit != it->indices.end (); ++pit)
  			{	
  				cloud_cluster->points.push_back (cloud_filtered->points[*pit]);
  				cloud_cluster->width = cloud_cluster->points.size ();
  				cloud_cluster->height = 1;
  				cloud_cluster->is_dense = true;
  			}

  			object_cluster.push_back(*cloud_cluster);
  		}
  	}
  	// ROS_INFO( "Number of object clustered in the scene %d", object_cluster.size());
}


void phobic_scene::erase_table()
{
	// Create the segmentation object for the planar model and set all the parameters
  	pcl::SACSegmentation<pcl::PointXYZRGBA> seg;
  	pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
  	pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
  	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_plane (new pcl::PointCloud<pcl::PointXYZRGBA> ());

  	seg.setOptimizeCoefficients (true);
  	seg.setModelType (pcl::SACMODEL_PLANE);
  	seg.setMethodType (pcl::SAC_RANSAC);
  	seg.setMaxIterations (100);
  	seg.setDistanceThreshold (0.005);

  	// Segment the largest planar component from the remaining cloud
  	seg.setInputCloud (cloud);
  	seg.segment (*inliers, *coefficients);

  	if (inliers->indices.size () == 0)
  	{
  		ROS_INFO( "Could not estimate a planar model for the given dataset." );

  	}

  	pcl::PointCloud<pcl::PointXYZRGBA> tmp;
    // Extract the planar inliers from the input cloud
  	pcl::ExtractIndices<pcl::PointXYZRGBA> extract;
  	extract.setInputCloud (cloud);
  	extract.setIndices (inliers);
  	extract.setNegative (true);

    // Get the points associated without the planar surface
  	extract.filter (tmp);
  	pcl::copyPointCloud(tmp, *cloud);
  	Plane_coeff=coefficients->values;

}


Eigen::Matrix4d phobic_scene::Cyl_Transform (const std::vector<float> coeff)
{
 	double x = coeff[0], y = coeff[1], z = coeff[2];
  	double p_x = coeff[3], p_y = coeff[4], p_z = coeff[5];
  	double r = coeff[6]; 

	Eigen::Vector3d position(x,y,z); //random position on the axis
	// w is the axis of the cylinder which will be aligned with the z reference frame of the cylinder
	Eigen::Vector3d w(p_x, p_y, p_z);
	Eigen::Vector3d u(0, -1, 0); // -y kinect 	

	// is object if twisted the z axis is the same to x axis. we change the x axis with a orthogonal vector
	
	if((u.dot(w) < 0))
	{
		w = w * -1; //change sign
	}


	Eigen::Vector3d x_new = (w.cross(u)).normalized();
	Eigen::Vector3d y_new = (x_new.cross(w)).normalized();
	
	Eigen::Matrix3d rotation;
	rotation.col(0) = - x_new;  // x
	rotation.col(1) = y_new;  // y
	rotation.col(2) = w;  // z

	// Compose the transformation and return it
	Eigen::Matrix<double,4,4> tranfs_matrix;
	tranfs_matrix.row(0) << rotation.row(0), position[0];
	tranfs_matrix.row(1) << rotation.row(1), position[1];
	tranfs_matrix.row(2) << rotation.row(2), position[2]; 
	tranfs_matrix.row(3) << 0,0,0, 1;


	return tranfs_matrix;
}

// void  phobic_scene::visualization(bool testing, bool circle)
// {
// 	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer (new pcl::visualization::PCLVisualizer ("3D Viewer"));
// 	viewer->setBackgroundColor (0, 255, 0);
// 	pcl::PointCloud<pcl::PointXYZRGBA>::Ptr room_object  (new pcl::PointCloud<pcl::PointXYZRGBA>);

// 	double PI= 3.14159265;


// 	if(circle == false)
// 	{
// 		if (testing == true )
// 		{
// 			pcl::copyPointCloud<pcl::PointXYZRGBA>( *cloud, *room_object);
// 		}

// 		else
// 		{
// 			pcl::copyPointCloud<pcl::PointXYZRGBA>( *object_cluster.begin(), *room_object);
// 		}


//   		pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZRGBA> single_color(room_object, 0, 0, 0);
//   		viewer->addPointCloud<pcl::PointXYZRGBA> (room_object, single_color, "sample cloud");
// 		//viewer->addPointCloud<pcl::PointXYZRGBA > (room_object, "nome");
// 		viewer->addCoordinateSystem(0.1);
//   	}

//   	else
//   	{
//   		pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZRGBA> single_color(cloud, 0, 0, 0);
//   		viewer->addPointCloud<pcl::PointXYZRGBA> (cloud, single_color, "sample cloud");
//   		viewer->addCoordinateSystem(0.1);

//   		viewer-> addCylinder(CYLINDER.cylinder_coeff, "cyl");
//   		viewer->addLine(CYLINDER.info_disegno_cyl_up, CYLINDER.info_disegno_cyl_dw, "line" );

//   	}



//   	while (!viewer->wasStopped ())
// 	{
//    		viewer->spinOnce (100);
//    		boost::this_thread::sleep (boost::posix_time::microseconds (100000));
// 	}
// }