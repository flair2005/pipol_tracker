#include "people_tracking_rai_alg_node.h"

PeopleTrackingRaiAlgNode::PeopleTrackingRaiAlgNode(void) :
  algorithm_base::IriBaseAlgorithm<PeopleTrackingRaiAlgorithm>(), it_(this->public_node_handle_)
{
      int intParam;
           
      //init class attributes if necessary
	//this->loop_rate_ = 2;//in [Hz]
	//hogFile.open("/home/andreu/Desktop/hogFile.txt");
	
	//relax 
	sleep(1);
      
      //general prupose variables
      tldMessageFilled = false;
      exeMode = MULTI_TRACKING;
      std::cout << "TRACKER EXE MODE INIT TO: " << exeMode << std::endl;
      tracker.setFollowMeTargetId(-1);//indicates no follow me target yet
	
      //init user parameters. general running parameters
      this->public_node_handle_.getParam("verbose_mode", intParam); this->verboseMode = (bool)intParam;
      
      //init user parameters. filter parameters
      this->public_node_handle_.getParam("num_particles", intParam); this->filterParams.numParticles = (unsigned int)intParam;
      this->public_node_handle_.getParam("init_delta_xy", this->filterParams.initDeltaXY);
      this->public_node_handle_.getParam("init_delta_vxy", this->filterParams.initDeltaVxy);
      this->public_node_handle_.getParam("sigma_resampling_xy", this->filterParams.sigmaResamplingXY);
      this->public_node_handle_.getParam("sigma_resampling_vxy", this->filterParams.sigmaResamplingVxy);
      this->public_node_handle_.getParam("person_radius", this->filterParams.personRadius);
      this->public_node_handle_.getParam("matching_legs_alpha", this->filterParams.matchingLegsAlpha);
      this->public_node_handle_.getParam("matching_legs_beta", this->filterParams.matchingLegsBeta);
      this->public_node_handle_.getParam("matching_bearing_alpha", this->filterParams.matchingBearingAlpha);
      this->public_node_handle_.getParam("matching_bearing_beta", this->filterParams.matchingBearingBeta);
//    this->public_node_handle_.getParam("power_rgb_cos", intParam); this->filterParams.powerRGBcos = (unsigned int)intParam;

      //init user parameters. tracker parameters
      this->public_node_handle_.getParam("minimum_distance_between_people", this->trackerParams.minDistanceBetweenPeople);
      this->public_node_handle_.getParam("minimum_association_prob", this->trackerParams.minAssociationProb);
      this->public_node_handle_.getParam("max_detection_distance_accepted", this->trackerParams.maxDetectionDistance);
      this->public_node_handle_.getParam("min_detection_distance_accepted", this->trackerParams.minDetectionDistance);
      this->public_node_handle_.getParam("max_detection_azimut_accepted", this->trackerParams.maxDetectionAzimut);      
      this->trackerParams.maxDetectionAzimut = this->trackerParams.maxDetectionAzimut*M_PI/180.;
      this->public_node_handle_.getParam("max_consecutive_uncorrected_iterations", intParam); this->trackerParams.maxConsecutiveUncorrected = (unsigned int)intParam;
      this->public_node_handle_.getParam("minimum_iterations_to_be_target", intParam); this->trackerParams.minIterationsToBeTarget = (unsigned int)intParam;
      this->public_node_handle_.getParam("minimum_iterations_to_be_visually_confirmed", intParam); this->trackerParams.iterationsToBeVisuallyConfirmed = (unsigned int)intParam;
      this->public_node_handle_.getParam("minimum_iterations_to_be_friend", intParam); this->trackerParams.iterationsToBeFriend = (unsigned int)intParam;                
      this->public_node_handle_.getParam("minimum_appearance_region_size", intParam); this->trackerParams.minAppearanceRegionSize = (unsigned int)intParam;

      //visualization parameters
      this->public_node_handle_.getParam("view_body_detections", intParam); this->viewBodyDetections = (bool)intParam;
      this->public_node_handle_.getParam("ratio_particles_displayed", this->ratioParticlesDisplayed);
      
      //set parameters to tracker
      this->tracker.setParameters(trackerParams);
      this->tracker.setFilterParameters(filterParams);

      //initializes random generator;
      srand ( time(NULL) ); 
      
      //debug counters
      frameCount = 0;
      hogDetCount = 0;
	
      //initializes camera mouting position and camera matrix
      //initCamera();	

  // [init publishers]
	this->particleSet_publisher_ = this->public_node_handle_.advertise<visualization_msgs::MarkerArray>("debug", 100);
	this->peopleSet_publisher_ = this->public_node_handle_.advertise<iri_perception_msgs::peopleTrackingArray>("peopleSet", 100);
	this->image_pub_ = it_.advertise("image_out", 1);
      this->tldBB_publisher_ = this->public_node_handle_.advertise<tld_msgs::Target>("tld_init", 1000, true);
  
  // [init subscribers]
      this->followMe_subscriber_ = this->public_node_handle_.subscribe("followMe", 100, &PeopleTrackingRaiAlgNode::followMe_callback, this);      
      this->tldDetections_subscriber_ = this->public_node_handle_.subscribe("tldDetections", 100, &PeopleTrackingRaiAlgNode::tldDetections_callback, this);
      this->faceDetections_subscriber_ = this->public_node_handle_.subscribe("faceDetections", 100, &PeopleTrackingRaiAlgNode::faceDetections_callback, this);
	this->bodyDetections_subscriber_ = this->public_node_handle_.subscribe("bodyDetections", 100, &PeopleTrackingRaiAlgNode::bodyDetections_callback, this);
	this->odometry_subscriber_ = this->public_node_handle_.subscribe("odometry", 100, &PeopleTrackingRaiAlgNode::odometry_callback, this);
	this->legDetections_subscriber_ = this->public_node_handle_.subscribe("legDetections", 100, &PeopleTrackingRaiAlgNode::legDetections_callback, this);
	this->image_sub_ = it_.subscribe("image_in", 1, &PeopleTrackingRaiAlgNode::image_callback, this);	
  
  // [init services]
  
  // [init clients]
  
  // [init action servers]
  
  // [init action clients]
}

PeopleTrackingRaiAlgNode::~PeopleTrackingRaiAlgNode(void)
{
  // [free dynamic memory]
  
  //close files
  //hogFile.close();
}

void PeopleTrackingRaiAlgNode::initCamera()
{
      tf::StampedTransform stf;
      Cposition3d camINbase;
      tf::TransformListener tfListener;
      
      //sleep(1);//??
      
      tfListener.waitForTransform("/base_link", "/left_stereo_optical_frame", ros::Time(0), ros::Duration(10.0),ros::Duration(1.0));
      tfListener.lookupTransform("/base_link", "/left_stereo_optical_frame", ros::Time(0), stf);
      camINbase.setXYZ(stf.getOrigin().x(), stf.getOrigin().y(), stf.getOrigin().z());
      camINbase.setQuaternion(stf.getRotation().getW(),stf.getRotation().getX(),stf.getRotation().getY(),stf.getRotation().getZ());
      tracker.setOnBoardCamPose(camINbase);
      //tracker->setOnBoardCamCalMatrix();//to do
}

void PeopleTrackingRaiAlgNode::fillMessages()
{
	std::list<CpersonTarget>::iterator iiF;
	std::list<CpersonParticle>::iterator iiP;
	std::list<CbodyObservation>::iterator iiB;
	std::list<Cpoint3dObservation>::iterator iiL;
	filterEstimate iiEst;
	ostringstream markerText;
      CbodyObservation tldDet;
	double bodyBearing, transpFactor;
	unsigned int ii=0;//, jj=0;
      unsigned int bbx,bby,bbw,bbh;
	
	//1. Main output message: peopleTrackingArray
	peopleTrackingArray_msg_.peopleSet.clear();
	peopleTrackingArray_msg_.header.frame_id = "/base_link";
	peopleTrackingArray_msg_.header.stamp = ros::Time::now();
	std::list<CpersonTarget> & targets = tracker.getTargetList();
	peopleTrackingArray_msg_.peopleSet.resize(targets.size());
	for (iiF=targets.begin(); iiF!=targets.end(); iiF++)
	{
		if ( iiF->getMaxStatus() > CANDIDATE )
		{
                        iiF->getEstimate(iiEst);
                        peopleTrackingArray_msg_.peopleSet[ii].targetId = iiF->getId();
                        peopleTrackingArray_msg_.peopleSet[ii].targetStatus = iiF->getStatus();
                        peopleTrackingArray_msg_.peopleSet[ii].x = iiEst.position.getX();
                        peopleTrackingArray_msg_.peopleSet[ii].y = iiEst.position.getY();
                        peopleTrackingArray_msg_.peopleSet[ii].vx = iiEst.velocity.getX();
                        peopleTrackingArray_msg_.peopleSet[ii].vy = iiEst.velocity.getY();
                        peopleTrackingArray_msg_.peopleSet[ii].covariances[0] = iiEst.position.getMatrixElement(0,0);
                        peopleTrackingArray_msg_.peopleSet[ii].covariances[5] = iiEst.position.getMatrixElement(1,1);
                        peopleTrackingArray_msg_.peopleSet[ii].covariances[10] = iiEst.velocity.getMatrixElement(0,0);
                        peopleTrackingArray_msg_.peopleSet[ii].covariances[15] = iiEst.velocity.getMatrixElement(1,1);
                        ii++;
		}
	}
	
	//erase message data if previous iteration had greater array size
	peopleTrackingArray_msg_.peopleSet.erase(peopleTrackingArray_msg_.peopleSet.begin()+ii,peopleTrackingArray_msg_.peopleSet.end());
	
	//2. VISUALIZATION MESSAGE: Marker array
	if (this->verboseMode) std::cout << std::endl << "*** Filling Debug Markers Message" << std::endl;
	std::list<Cpoint3dObservation> & laserDetSet = tracker.getLaserDetSet();
	std::list<CbodyObservation> & bodyDetSet = tracker.getBodyDetSet();
	MarkerArray_msg_.markers.clear();
	//MarkerArray_msg_.markers.resize( laserDetSet.size() + bodyDetSet.size()*4 + 2*targets.size() + filterParams.numParticles*targets.size() );
      MarkerArray_msg_.markers.resize( laserDetSet.size() + bodyDetSet.size()*4 + targets.size() + filterParams.numParticles*targets.size() +1 ); //ALERT: add particleRatioToBeDrawn ???        
	ii = 0;
	//if (this->verboseMode) std::cout << "\tlaserDetSet.size(): " << laserDetSet.size() << "\tbodyDetSet.size(): " << bodyDetSet.size() << "\ttargets.size(): " << targets.size() << "\tMarkerArray_msg_.markers.size() " << MarkerArray_msg_.markers.size() << std::endl; 

	//2a. laser detections
	for (iiL=laserDetSet.begin(); iiL!=laserDetSet.end(); iiL++)
	{
		//if (this->verboseMode) std::cout << "mainNodeThread: " << __LINE__ << std::endl;
		this->MarkerArray_msg_.markers[ii].header.frame_id = "/base_link";
		this->MarkerArray_msg_.markers[ii].header.stamp = ros::Time::now();
		this->MarkerArray_msg_.markers[ii].id = ii;
		this->MarkerArray_msg_.markers[ii].type = visualization_msgs::Marker::CYLINDER;
		this->MarkerArray_msg_.markers[ii].action = visualization_msgs::Marker::ADD;
		this->MarkerArray_msg_.markers[ii].pose.position.x = iiL->point.getX();
		this->MarkerArray_msg_.markers[ii].pose.position.y = iiL->point.getY();
		this->MarkerArray_msg_.markers[ii].pose.position.z = MARKER_Z;
		this->MarkerArray_msg_.markers[ii].scale.x = MARKER_SIZE/3;
		this->MarkerArray_msg_.markers[ii].scale.y = MARKER_SIZE/3;
		this->MarkerArray_msg_.markers[ii].scale.z = MARKER_SIZE/3;
		this->MarkerArray_msg_.markers[ii].color.a = MARKER_TRANSPARENCY;
		this->MarkerArray_msg_.markers[ii].color.r = 1;
		this->MarkerArray_msg_.markers[ii].color.g = 0.2;
		this->MarkerArray_msg_.markers[ii].color.b = 0.2;
		this->MarkerArray_msg_.markers[ii].lifetime = ros::Duration(MARKER_DURATION);
		ii++;				
	}
	
	//2b. body detections
	if (this->viewBodyDetections)
	{
		for (iiB=bodyDetSet.begin(); iiB!=bodyDetSet.end(); iiB++)
		{
                  //if (this->verboseMode) std::cout << "mainNodeThread: " << __LINE__ << std::endl;
                  //arrow indicating bearing and rgb eigen
                  this->MarkerArray_msg_.markers[ii].header.frame_id = "/base_link";
                  this->MarkerArray_msg_.markers[ii].header.stamp = ros::Time::now();
                  this->MarkerArray_msg_.markers[ii].id = ii;
                  this->MarkerArray_msg_.markers[ii].type = visualization_msgs::Marker::ARROW;
                  this->MarkerArray_msg_.markers[ii].action = visualization_msgs::Marker::ADD;
                  this->MarkerArray_msg_.markers[ii].pose.position.x = 0;
                  this->MarkerArray_msg_.markers[ii].pose.position.y = 0;
                  this->MarkerArray_msg_.markers[ii].pose.position.z = MARKER_Z;
                  this->MarkerArray_msg_.markers[ii].scale.x = MARKER_SIZE;
                  this->MarkerArray_msg_.markers[ii].scale.y = MARKER_SIZE;
                  this->MarkerArray_msg_.markers[ii].scale.z = MARKER_SIZE*6;
                  this->MarkerArray_msg_.markers[ii].color.a = MARKER_TRANSPARENCY;
                  this->MarkerArray_msg_.markers[ii].color.r = 0;
                  this->MarkerArray_msg_.markers[ii].color.g = 1;
                  this->MarkerArray_msg_.markers[ii].color.b = 1;
                  this->MarkerArray_msg_.markers[ii].lifetime = ros::Duration(MARKER_DURATION*3);
                  bodyBearing = atan2(iiB->direction.getY(),iiB->direction.getX());
                  geometry_msgs::Quaternion bearingQuaternion = tf::createQuaternionMsgFromYaw(bodyBearing);
                  this->MarkerArray_msg_.markers[ii].pose.orientation.x = bearingQuaternion.x;
                  this->MarkerArray_msg_.markers[ii].pose.orientation.y = bearingQuaternion.y;
                  this->MarkerArray_msg_.markers[ii].pose.orientation.z = bearingQuaternion.z;
                  this->MarkerArray_msg_.markers[ii].pose.orientation.w = bearingQuaternion.w;
                  ii++;
			
			//three cylinders indicating RGB clusters
                        /*
			for (jj=0; (jj<iiB->rgbCenters.size()) || (jj<3); jj++)
			{
				//if (this->verboseMode) std::cout << "rgbCenters.size(): " << iiB->rgbCenters.size() << std::endl;
				this->MarkerArray_msg_.markers[ii].header.frame_id = "/base_link";
				this->MarkerArray_msg_.markers[ii].header.stamp = ros::Time::now();
				this->MarkerArray_msg_.markers[ii].id = ii;
				this->MarkerArray_msg_.markers[ii].type = visualization_msgs::Marker::CYLINDER;
				this->MarkerArray_msg_.markers[ii].action = visualization_msgs::Marker::ADD;
				this->MarkerArray_msg_.markers[ii].pose.position.x = 2*cos(bodyBearing);
				this->MarkerArray_msg_.markers[ii].pose.position.y = 2*sin(bodyBearing);
				this->MarkerArray_msg_.markers[ii].pose.position.z = jj*MARKER_SIZE;
				this->MarkerArray_msg_.markers[ii].scale.x = MARKER_SIZE/2;
				this->MarkerArray_msg_.markers[ii].scale.y = MARKER_SIZE/2;
				this->MarkerArray_msg_.markers[ii].scale.z = MARKER_SIZE;
				this->MarkerArray_msg_.markers[ii].color.a = MARKER_TRANSPARENCY;
				this->MarkerArray_msg_.markers[ii].color.r = iiB->rgbCenters.at(jj).getX();
				this->MarkerArray_msg_.markers[ii].color.g = iiB->rgbCenters.at(jj).getY();
				this->MarkerArray_msg_.markers[ii].color.b = iiB->rgbCenters.at(jj).getZ();
				this->MarkerArray_msg_.markers[ii].lifetime = ros::Duration(MARKER_DURATION*3);
				ii++;
			}
			*/
		}
	}

	//2c. target positions, target Id's and particles
	for (iiF=targets.begin(); iiF!=targets.end(); iiF++)
      {
            //if (this->verboseMode) std::cout << "mainNodeThread: " << __LINE__ << std::endl;
            if ( iiF->getMaxStatus() > CANDIDATE )
            {
                  iiF->getEstimate(iiEst);
                  
                  //cylinder: target position
                  this->MarkerArray_msg_.markers[ii].header.frame_id = "/base_link";
                  this->MarkerArray_msg_.markers[ii].header.stamp = ros::Time::now();
                  this->MarkerArray_msg_.markers[ii].id = ii;
                  this->MarkerArray_msg_.markers[ii].type = visualization_msgs::Marker::CYLINDER;
                  this->MarkerArray_msg_.markers[ii].action = visualization_msgs::Marker::ADD;
                  this->MarkerArray_msg_.markers[ii].pose.position.x = iiEst.position.getX();
                  this->MarkerArray_msg_.markers[ii].pose.position.y = iiEst.position.getY();
                  if (iiF->pOcclusion > 0.5) this->MarkerArray_msg_.markers[ii].pose.position.z = MARKER_Z;
                  else this->MarkerArray_msg_.markers[ii].pose.position.z = MARKER_Z;
                  this->MarkerArray_msg_.markers[ii].scale.x = MARKER_SIZE;
      
                  //marker size depending on occlusion
                  this->MarkerArray_msg_.markers[ii].scale.y = MARKER_SIZE;
                  if (iiF->pOcclusion > 0.5) this->MarkerArray_msg_.markers[ii].scale.z = MARKER_SIZE*2;
                  else this->MarkerArray_msg_.markers[ii].scale.z = MARKER_SIZE;
                  
                  //marker transp depending on status
                  switch(iiF->getMaxStatus())
                  {
                        case LEGGED_TARGET: transpFactor = 0.1; break;
                        case VISUALLY_CONFIRMED: transpFactor = 0.5; break;
                        case FRIEND_IN_SIGHT: transpFactor = 1; break;
                        //case FRIEND_OUT_OF_RANGE: transpFactor = 1; break;
                        default: transpFactor = 0.1; break; 
                  }
                  this->MarkerArray_msg_.markers[ii].color.a = MARKER_TRANSPARENCY*transpFactor;
                  this->MarkerArray_msg_.markers[ii].color.r = 0;
                  this->MarkerArray_msg_.markers[ii].color.g = 0;
                  this->MarkerArray_msg_.markers[ii].color.b = 1.0;
                  this->MarkerArray_msg_.markers[ii].lifetime = ros::Duration(MARKER_DURATION);
                  ii++;
                  
                  //text: target ID
                  this->MarkerArray_msg_.markers[ii].header.frame_id = "/base_link";
                  this->MarkerArray_msg_.markers[ii].header.stamp = ros::Time::now();
                  this->MarkerArray_msg_.markers[ii].id = ii;
                  this->MarkerArray_msg_.markers[ii].type = visualization_msgs::Marker::TEXT_VIEW_FACING;
                  this->MarkerArray_msg_.markers[ii].action = visualization_msgs::Marker::ADD;
                  this->MarkerArray_msg_.markers[ii].pose.position.x = iiEst.position.getX()+0.4;
                  this->MarkerArray_msg_.markers[ii].pose.position.y = iiEst.position.getY()+0.4;
                  this->MarkerArray_msg_.markers[ii].pose.position.z = MARKER_Z;
                  this->MarkerArray_msg_.markers[ii].scale.z = MARKER_TEXT_SIZE;
                  this->MarkerArray_msg_.markers[ii].color.a = 1;
                  this->MarkerArray_msg_.markers[ii].color.r = 0.0;
                  this->MarkerArray_msg_.markers[ii].color.g = 0.0;
                  this->MarkerArray_msg_.markers[ii].color.b = 0.0;
                  this->MarkerArray_msg_.markers[ii].lifetime = ros::Duration(MARKER_DURATION);
                  markerText.str("");
                  markerText << iiF->getId() << "/";
                  for (unsigned int jj=0; jj<NUM_DETECTORS; jj++)
                  {
                        for (unsigned int kk=0; kk<iiF->aDecisions[jj].size(); kk++)
                        {
                              if ( iiF->aDecisions[jj].at(kk) )
                              {
                                    markerText << jj;
                                    break;
                              }
                        }
                  }
                  this->MarkerArray_msg_.markers[ii].text = markerText.str();
                  ii++;

                  //text: debugging ! Velocities
//                   this->MarkerArray_msg_.markers[ii].header.frame_id = "/base_link";
//                   this->MarkerArray_msg_.markers[ii].header.stamp = ros::Time::now();
//                   this->MarkerArray_msg_.markers[ii].id = ii;
//                   this->MarkerArray_msg_.markers[ii].type = visualization_msgs::Marker::TEXT_VIEW_FACING;
//                   this->MarkerArray_msg_.markers[ii].action = visualization_msgs::Marker::ADD;
//                   this->MarkerArray_msg_.markers[ii].pose.position.x = iiEst.position.getX()+0.4;
//                   this->MarkerArray_msg_.markers[ii].pose.position.y = iiEst.position.getY()-0.4;
//                   this->MarkerArray_msg_.markers[ii].pose.position.z = MARKER_Z;
//                   this->MarkerArray_msg_.markers[ii].scale.z = MARKER_TEXT_SIZE;
//                   this->MarkerArray_msg_.markers[ii].color.a = 1;
//                   this->MarkerArray_msg_.markers[ii].color.r = 0.0;
//                   this->MarkerArray_msg_.markers[ii].color.g = 0.0;
//                   this->MarkerArray_msg_.markers[ii].color.b = 0.0;
//                   this->MarkerArray_msg_.markers[ii].lifetime = ros::Duration(MARKER_DURATION);
//                   markerText.precision(2);
//                   markerText.str("");
//                   markerText << "(" << iiEst.velocity.getX() << "," << iiEst.velocity.getY() << ")";
//                   this->MarkerArray_msg_.markers[ii].text = markerText.str();
//                   ii++;                  
                  
                  //particles
                  std::list<CpersonParticle> & pSet = iiF->getParticleSet();
                  //if (this->verboseMode) std::cout << "mainNodeThread: " << __LINE__ << "; pSet.size(): " << pSet.size() << std::endl;
                  for (iiP=pSet.begin();iiP!=pSet.end();iiP++)
                  {
                        //if (this->verboseMode) std::cout << "mainNodeThread: " << __LINE__ << "; ii: " << ii << std::endl;
                        double rnd = (double)rand()/(double)RAND_MAX;
                        if ( rnd < ratioParticlesDisplayed )
                        {
                              this->MarkerArray_msg_.markers[ii].header.frame_id = "/base_link";
                              this->MarkerArray_msg_.markers[ii].header.stamp = ros::Time::now();
                              this->MarkerArray_msg_.markers[ii].id = ii;
                              this->MarkerArray_msg_.markers[ii].type = visualization_msgs::Marker::CYLINDER;
                              this->MarkerArray_msg_.markers[ii].action = visualization_msgs::Marker::ADD;
                              this->MarkerArray_msg_.markers[ii].pose.position.x = iiP->position.getX();
                              this->MarkerArray_msg_.markers[ii].pose.position.y = iiP->position.getY();
                              this->MarkerArray_msg_.markers[ii].pose.position.z = MARKER_Z;
                              this->MarkerArray_msg_.markers[ii].scale.x = MARKER_SIZE/10;
                              this->MarkerArray_msg_.markers[ii].scale.y = MARKER_SIZE/10;
                              this->MarkerArray_msg_.markers[ii].scale.z = MARKER_SIZE/10;
                              this->MarkerArray_msg_.markers[ii].color.a = MARKER_TRANSPARENCY;
                              this->MarkerArray_msg_.markers[ii].color.r = 0.5;
                              this->MarkerArray_msg_.markers[ii].color.g = 1.5;
                              this->MarkerArray_msg_.markers[ii].color.b = 0.0;
                              this->MarkerArray_msg_.markers[ii].lifetime = ros::Duration(MARKER_DURATION);
                              ii++;
                        }
                  }		
            }
      }
      
      //2d. Arrow mark for the current TLD detection
      tracker.getTLDdetection(tldDet);
      if (tldDet.bbW != 0) //check if a bounding box is set
      {
            this->MarkerArray_msg_.markers[ii].header.frame_id = "/base_link";
            this->MarkerArray_msg_.markers[ii].header.stamp = ros::Time::now();
            this->MarkerArray_msg_.markers[ii].id = ii;
            this->MarkerArray_msg_.markers[ii].type = visualization_msgs::Marker::ARROW;
            this->MarkerArray_msg_.markers[ii].action = visualization_msgs::Marker::ADD;
            this->MarkerArray_msg_.markers[ii].pose.position.x = 0;
            this->MarkerArray_msg_.markers[ii].pose.position.y = 0;
            this->MarkerArray_msg_.markers[ii].pose.position.z = MARKER_Z;
            this->MarkerArray_msg_.markers[ii].scale.x = MARKER_SIZE;
            this->MarkerArray_msg_.markers[ii].scale.y = MARKER_SIZE;
            this->MarkerArray_msg_.markers[ii].scale.z = MARKER_SIZE*3;
            this->MarkerArray_msg_.markers[ii].color.r = 240./255.;
            this->MarkerArray_msg_.markers[ii].color.g = 180./255.;
            this->MarkerArray_msg_.markers[ii].color.b = 20./255.;      
            this->MarkerArray_msg_.markers[ii].color.a = MARKER_TRANSPARENCY;
            this->MarkerArray_msg_.markers[ii].lifetime = ros::Duration(MARKER_DURATION*3);
            bodyBearing = atan2(tldDet.direction.getY(),tldDet.direction.getX());
            geometry_msgs::Quaternion bearingQuaternion = tf::createQuaternionMsgFromYaw(bodyBearing);
            this->MarkerArray_msg_.markers[ii].pose.orientation.x = bearingQuaternion.x;
            this->MarkerArray_msg_.markers[ii].pose.orientation.y = bearingQuaternion.y;
            this->MarkerArray_msg_.markers[ii].pose.orientation.z = bearingQuaternion.z;
            this->MarkerArray_msg_.markers[ii].pose.orientation.w = bearingQuaternion.w;
            ii++;
      }
      
      //3. TLD message
      if ( (exeMode == SHOOT_TLD) && (cv_ptr!=NULL) )
      {
            tldBB_msg_.bb.header.frame_id = "/base_link";
            tldBB_msg_.bb.header.stamp = ros::Time::now();
            tracker.getTLDbb(bbx,bby,bbw,bbh);
            tldBB_msg_.bb.x = bbx;
            tldBB_msg_.bb.y = bby;
            tldBB_msg_.bb.width = bbw;
            tldBB_msg_.bb.height = bbh;
            tldBB_msg_.bb.confidence = 1.0;
            cv_ptr->toImageMsg(tldBB_msg_.img);
            tldMessageFilled = true;
      }

      //erase message data if previous iteration had greater array size
      MarkerArray_msg_.markers.erase(MarkerArray_msg_.markers.begin()+ii,MarkerArray_msg_.markers.end());	
}

void PeopleTrackingRaiAlgNode::mainNodeThread(void)
{	
      if (this->verboseMode) std::cout << std::endl << "************* NEW ITERATION **************" << std::endl;

      //LOCK data reception mutexes
      this->legDetections_mutex_.enter(); 
      this->bodyDetections_mutex_.enter(); 
      this->faceDetections_mutex_.enter(); 
      this->tldDetections_mutex_.enter(); 
      this->followMe_mutex_.enter(); 
      this->image_mutex_.enter();

      //PRINT DETECTIONS AVAILABLE AT THIS ITERATION
      if (this->verboseMode) tracker.printDetectionSets();
      
      //FILTER PREDICTION
      if (this->verboseMode) std::cout << std::endl << "*** Prior peopleSet:" << std::endl;
      this->odometry_mutex_.enter(); 
      tracker.propagateFilters(platformOdometry); //platform motion
      platformOdometry.resetDeltas();
      this->odometry_mutex_.exit(); 
      tracker.propagateFilters(); //people motion
      
      //OCCLUSIONS
      tracker.computeOcclusions();
      
      //UPDATE STATUS & ESTIMATES
      tracker.updateFilterEstimates();
      tracker.updateTargetStatus();
      if (this->verboseMode) tracker.printPeopleSet();

      //DATA ASSOCIATION
      //if (this->verboseMode) std::cout << std::endl << "*** Target/Detection association" << std::endl;
      tracker.updateAssociationTables();
            
      //MARK BOUNDING BOXES & LEARN CURRENT DETECTED APPEARANCES
      if (cv_ptr!=NULL)
      {
            //debug
            //frameCount++;
            //std::cout << "det/frames = " << (double)hogDetCount/(double)frameCount << std::endl;
            
            //Set image to the tracker
            tracker.setCurrentImage(cv_ptr->image);
            
            //compute appearances of body detections
            //tracker.computeTargetAppearance();
            
            //mark box bodies on image
            tracker.markBodies();            
            
            //mark box faces on image
            tracker.markFaces();
            
            //mark tld box
            tracker.markTld();
            
            //get marked image from the tracker
            tracker.getCurrentImage(cv_ptr->image);
      }                                
                        
      //CORRECTION
      if (this->verboseMode) std::cout << std::endl << "*** Posterior peopleSet:" << std::endl;
      tracker.correctFilters();
            
      //UPDATE FILTER ESTIMATES AND ADD THEM TO EACH TARGET TRACK
      tracker.updateFilterEstimates();
      tracker.addEstimatesToTracks();
      if (this->verboseMode) tracker.printPeopleSet();
                  
      //LAUNCH NEW FILTERS IF NEW DETECTIONS ARE NOT ASSOCIATED
      if (this->verboseMode) std::cout << std::endl << "*** Create Filters" << std::endl;
      tracker.createFilters();

      //REMOVE UNSUPPORTED TARGETS
      if (this->verboseMode) std::cout << std::endl << "*** Delete Filters" << std::endl;
      tracker.deleteFilters();
            
      //RESAMPLING PARTICLE SETS 
      if (this->verboseMode) std::cout << std::endl << "*** Resampling" << std::endl;
      tracker.resampleFilters();
      
      //Check if TLD tracker can be initialized, if so, initTLD
      if ( (exeMode == MULTI_TRACKING) && (tracker.getFollowMeTargetId()>0) )
      {
            exeMode = SHOOT_TLD;
            std::cout << "TRACKER EXE MODE UPDATED TO: " << exeMode << "(" << __LINE__ << ")" << std::endl;
            tracker.initTLD();
      }

      // [fill msg structures]
      if (this->verboseMode) std::cout << std::endl << "*** Filling Output Messages" << std::endl;
      this->fillMessages();

      //RESET DETECTION SETS
      tracker.resetDetectionSets(LEGS);
      tracker.resetDetectionSets(BODY);
      tracker.resetDetectionSets(FACE);
      tracker.resetDetectionSets(TLD);

      //UNLOCK data reception mutexes (except image mutex which will be unlocked below)
      this->legDetections_mutex_.exit(); 
      this->bodyDetections_mutex_.exit(); 
      this->faceDetections_mutex_.exit();
      this->tldDetections_mutex_.exit();
      this->followMe_mutex_.exit(); 

      // [fill srv structure and make request to the server]

      // [fill action structure and make request to the action server]

      // [publish messages]
      this->particleSet_publisher_.publish(this->MarkerArray_msg_);
      this->peopleSet_publisher_.publish(this->peopleTrackingArray_msg_);
      if (cv_ptr!=NULL) image_pub_.publish(cv_ptr->toImageMsg());
      if ( tldMessageFilled )
      {
            tldMessageFilled = false;
            exeMode = FOLLOW_ME;
            std::cout << "TRACKER EXE MODE UPDATED TO: " << exeMode << "(" << __LINE__ << ")" << std::endl;
            tldBB_publisher_.publish(tldBB_msg_);//this message is published once, just to start-up tld tracker
      }

      //unlock image mutex
      this->image_mutex_.exit();        
}

/*  [subscriber callbacks] */
void PeopleTrackingRaiAlgNode::followMe_callback(const std_msgs::Int32::ConstPtr& msg) 
{
      followMe_mutex_.enter();
      std::cout << "*************************************************" << std::endl;
      tracker.setFollowMeTargetId(msg->data);
      followMe_mutex_.exit();
}

void PeopleTrackingRaiAlgNode::tldDetections_callback(const tld_msgs::BoundingBox::ConstPtr& msg) 
{ 
      double dirX, dirY, dirMod;
      double cx,fx; //camera calibration params cx = P[0,2], fx = P[0,0].
      CbodyObservation newDetection;
      
      //hardcoded for REEM head cameras!! ToDo: get it from camera_info message !!
      cx = 286.37;
      fx = 338.49;
      
      //ROS_INFO("PeopleTrackingRaiAlgNode::tldDetections_callback: New Message Received"); 
      
      //blocks tld detection mutex
      this->tldDetections_mutex_.enter(); 

      //time stamp
      newDetection.timeStamp.set(msg->header.stamp.sec, msg->header.stamp.nsec);
      
      //bounding box
      newDetection.bbX = msg->x;
      newDetection.bbY = msg->y;
      newDetection.bbW = msg->width;
      newDetection.bbH = msg->height;
      
      //confidence set in the rgbEigen.X field (to be improved !)
      newDetection.rgbEigen.setX(msg->confidence);
      
      //direction (robot: X->forward, Y->left. Camera: x->horizontal, y->vertical, z->depth)
      dirX = 1.0;
      dirY = -((msg->x+msg->width/2.0)-cx)/fx;
      dirMod = sqrt(dirX*dirX+dirY*dirY);
      newDetection.direction.setXYZ(dirX/dirMod, dirY/dirMod, 0);
      
      //sets TLD detection to tracker
      tracker.setTLDdetection(newDetection);

      //unblocks tld detection mutex
      this->tldDetections_mutex_.exit(); 
}

void PeopleTrackingRaiAlgNode::faceDetections_callback(const pal_vision_msgs::FaceDetections::ConstPtr& msg) 
{ 
      unsigned int ii;
      CfaceObservation newDetection;
      std::list<CfaceObservation>::iterator iiB;

      //ROS_INFO("PeopleTrackingRaiAlgNode::faceDetections_callback: New Message Received"); 
      
      //blocks face detection mutex
      this->faceDetections_mutex_.enter(); 
      
      //sets current (received) detections
      for (ii=0; ii<msg->faces.size(); ii++)
      {
            //set time stamp
            newDetection.timeStamp.set(msg->header.stamp.sec, msg->header.stamp.nsec);
            
            //get message data
            newDetection.faceLoc.setX(msg->faces[ii].position3D.x);
            newDetection.faceLoc.setY(msg->faces[ii].position3D.y);
            newDetection.faceLoc.setZ(msg->faces[ii].position3D.z);
            newDetection.bbX = msg->faces[ii].imageBoundingBox.x;
            newDetection.bbY = msg->faces[ii].imageBoundingBox.y;
            newDetection.bbW = msg->faces[ii].imageBoundingBox.width;
            newDetection.bbH = msg->faces[ii].imageBoundingBox.height;
            
            //add detection to tracker list
            tracker.addDetection(newDetection);
      }
      
      //unblocks face detection mutex
      this->faceDetections_mutex_.exit();
}

void PeopleTrackingRaiAlgNode::bodyDetections_callback(const pal_vision_msgs::HogDetections::ConstPtr& msg) 
{ 
//   ROS_INFO("PeopleTrackingRaiAlgNode::bodyDetections_callback: New Message Received"); 
	
      unsigned int ii, jj;
      CbodyObservation newDetection;
      std::list<CbodyObservation>::iterator iiB;
      
      //blocks body detection mutex
      this->bodyDetections_mutex_.enter(); 

      //sets current (received) detections
      for (ii=0; ii<msg->persons.size(); ii++)
      {
// 		CbodyObservation newDetection;
		
		//debug
		hogDetCount ++;
		
		//time stamp
		newDetection.timeStamp.set(msg->header.stamp.sec, msg->header.stamp.nsec);
		
		//bounding box
		newDetection.bbX = msg->persons[ii].imageBoundingBox.x;
		newDetection.bbY = msg->persons[ii].imageBoundingBox.y;
		newDetection.bbW = msg->persons[ii].imageBoundingBox.width;
		newDetection.bbH = msg->persons[ii].imageBoundingBox.height;
		
		//direction
		newDetection.direction.setXYZ(msg->persons[ii].direction.x, msg->persons[ii].direction.y, msg->persons[ii].direction.z);
		
		//rgb Eigen
		newDetection.rgbEigen.setXYZ(msg->persons[ii].principalEigenVectorRGB.r, msg->persons[ii].principalEigenVectorRGB.g, msg->persons[ii].principalEigenVectorRGB.b);
		
		//rgb centers
		newDetection.rgbCenters.resize(msg->persons[ii].rgbClusterCenters.size());
		for(jj=0; jj<newDetection.rgbCenters.size(); jj++)
		{
			newDetection.rgbCenters.at(jj).setXYZ(msg->persons[ii].rgbClusterCenters[jj].r, msg->persons[ii].rgbClusterCenters[jj].g, msg->persons[ii].rgbClusterCenters[jj].b);
		}	
		
		//hog descriptor
		newDetection.hog.resize(msg->persons[ii].hog.size());
		for(jj=0; jj<newDetection.hog.size(); jj++)
		{
			newDetection.hog[jj] = msg->persons[ii].hog[jj];
			//hogFile << msg->persons[ii].hog[jj] << " ";
		}
		//hogFile << std::endl;
		
		//rgbDescriptors
		if (this->verboseMode) std::cout << "rgbDescriptor1.size(): " << msg->persons[ii].rgbDescriptor1.size() << std::endl;
				
		//add detection to tracker list
		tracker.addDetection(newDetection);
	}
	//if (this->verboseMode) tracker.printDetectionSets();
	this->bodyDetections_mutex_.exit();
	//hogFile << "#" << std::endl;
}

void PeopleTrackingRaiAlgNode::odometry_callback(const nav_msgs::Odometry::ConstPtr& msg) 
{ 
	//ROS_INFO("PeopleTrackingRaiAlgNode::odometry_callback: New Message Received"); 
	
	double vx,vy,vz,vTrans;
	double tLast, dT;

	//use appropiate mutex to shared variables if necessary 
	this->odometry_mutex_.enter(); 

	//if (this->verboseMode) std::cout << msg->data << std::endl; 
	tLast = platformOdometry.timeStamp.get();
	platformOdometry.timeStamp.setToNow();
	dT = platformOdometry.timeStamp.get() - tLast;
	//if (this->verboseMode) std::cout << "******************** dT = " << dT << std::endl;
	vx = msg->twist.twist.linear.x; 
	vy = msg->twist.twist.linear.y;
	vz = msg->twist.twist.linear.z;
	vTrans = sqrt(vx*vx+vy*vy+vz*vz);  //odometry observation considers only forward velocity
	platformOdometry.accumDeltaTrans(dT*vTrans);
	platformOdometry.accumDeltaH(dT*msg->twist.twist.angular.z);
	
	//unlock previously blocked shared variables 
	//this->alg_.unlock(); 
	this->odometry_mutex_.exit(); 
}

void PeopleTrackingRaiAlgNode::legDetections_callback(const pal_vision_msgs::LegDetections::ConstPtr& msg) 
{ 
  //ROS_INFO("PeopleTrackingRaiAlgNode::legDetections_callback: New Message Received"); 
        Cpoint3dObservation newDetection;
        
        //locks leg detection mutex
	this->legDetections_mutex_.enter(); 
  
	//tracker.resetDetectionSets(LEGS);//deletes previous detections
        
        //sets current (received) detections
	for (unsigned int ii=0; ii<msg->position3D.size(); ii++)
	{
// 		Cpoint3dObservation newDetection;
		newDetection.timeStamp.set(msg->header.stamp.sec, msg->header.stamp.nsec);
		newDetection.point.setXYZ(msg->position3D[ii].x, msg->position3D[ii].y, 0.0);
		newDetection.point.setXYcov(0.2,0.2,0);
		tracker.addDetection(newDetection);
	}
	
	//unlocks leg detection mutex
	this->legDetections_mutex_.exit();
}

void PeopleTrackingRaiAlgNode::image_callback(const sensor_msgs::ImageConstPtr& msg)
{
// 	ROS_INFO("PeopleTrackingRaiAlgNode::image_callback: New Message Received");
// 	this->alg_.lock();
	this->image_mutex_.enter();
	try
	{
		cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
	}
	catch (cv_bridge::Exception& e)
	{
		ROS_ERROR("cv_bridge exception: %s", e.what());
		return;
	}
// 	this->alg_.unlock();
	this->image_mutex_.exit();
}

/*  [service callbacks] */

/*  [action callbacks] */

/*  [action requests] */

void PeopleTrackingRaiAlgNode::node_config_update(Config &config, uint32_t level)
{
  this->alg_.lock();

  this->alg_.unlock();
}

void PeopleTrackingRaiAlgNode::addNodeDiagnostics(void)
{
}

/* main function */
int main(int argc,char *argv[])
{
  return algorithm_base::main<PeopleTrackingRaiAlgNode>(argc, argv, "people_tracking_rai_alg_node");
}