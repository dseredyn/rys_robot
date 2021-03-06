#include <ros/ros.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Twist.h>
#include <tf/transform_broadcaster.h>
#include <sensor_msgs/Imu.h>

#include "elektron.hpp"

ros::Time cmd_time;

Protonek *p;

void twistCallback(const geometry_msgs::TwistConstPtr& msg) {

        // turbo button
        p->joystick.buttonTurbo = (msg->angular.x > 0.5);

        // stop button
        p->joystick.buttonStop = (msg->angular.y > 0.5);

        // feature 1 button
        p->joystick.buttonGetUp = (msg->linear.y > 0.5);

        // feature 2 button
        p->joystick.buttonTrick = (msg->linear.z > 0.5);

        p->joystick.speedLinear = msg->linear.x;
        p->joystick.speedAngular = msg->angular.z;
}

int main(int argc, char** argv) {
        ros::init(argc, argv, "elektron_base_node");
        ros::NodeHandle n;
        ros::NodeHandle nh("~");

        ros::Publisher odom_pub = n.advertise<nav_msgs::Odometry> ("odom", 1);
        ros::Publisher imu_pub = n.advertise<sensor_msgs::Imu> ("imu", 1);
        ros::Publisher currentL_pub = n.advertise<geometry_msgs::PointStamped> ("currentL", 1);
        ros::Publisher currentR_pub = n.advertise<geometry_msgs::PointStamped> ("currentR", 1);
        ros::Publisher current_pub = n.advertise<geometry_msgs::PointStamped> ("current", 1);
        ros::Publisher speedL_pub = n.advertise<geometry_msgs::PointStamped> ("speedL", 1);
        ros::Publisher speedR_pub = n.advertise<geometry_msgs::PointStamped> ("speedR", 1);
        ros::Publisher speed_pub = n.advertise<geometry_msgs::PointStamped> ("speed", 1);

        ros::Subscriber twist_sub = n.subscribe("cmd_vel", 1, &twistCallback);

        ros::Rate loop_rate(100);

        std::string dev, dev2;
        
        nh.param<std::string>("device", dev, "/dev/ttyUSB0");
        nh.param<std::string>("device2", dev2, "/dev/ttyUSB1");

        Protonek::Parameters parL, parR;
        // lewy mostek
        nh.param<int>("parL_currentKp", parL.currentKp,0);
        nh.param<int>("parL_currentKi", parL.currentKi,2);
        nh.param<int>("parL_currentKd", parL.currentKd,0);
        nh.param<int>("parL_maxCurrent", parL.maxCurrent,50);
        nh.param<int>("parL_speedKp", parL.speedKp,8);
        nh.param<int>("parL_speedKi", parL.speedKi,0);
        nh.param<int>("parL_speedKd", parL.speedKd,0);

        // prawy mostek
        nh.param<int>("parR_currentKp", parR.currentKp,0);
        nh.param<int>("parR_currentKi", parR.currentKi,2);
        nh.param<int>("parR_currentKd", parR.currentKd,0);
        nh.param<int>("parR_maxCurrent", parR.maxCurrent,50);
        nh.param<int>("parR_speedKp", parR.speedKp,8);
        nh.param<int>("parR_speedKi", parR.speedKi,0);
        nh.param<int>("parR_speedKd", parR.speedKd,0);

        int useSpeedRegulator;
        nh.param<int>("use_speed_regulator",useSpeedRegulator, 1);

        double balanceAngle;
        nh.param<double>("balance_angle",balanceAngle, 90);

        double Kp, Ki, Kd;
        nh.param<double>("angleKp", Kp, 0.01);
        nh.param<double>("angleKi", Ki,0.0);
        nh.param<double>("angleKd", Kd,0.0);

        double avKp, avKi, avKd;
        nh.param<double>("avKp", avKp, 0.0000005);
        nh.param<double>("avKi", avKi,0.0);
        nh.param<double>("avKd", avKd,0.0);

        double lvKp, lvKi, lvKd;
        nh.param<double>("lvKp", lvKp, 0.5);
        nh.param<double>("lvKi", lvKi,0.0);
        nh.param<double>("lvKd", lvKd,0.0);
                
        nav_msgs::Odometry odom;
        odom.header.frame_id = "odom";
        odom.child_frame_id = "base_link";

        sensor_msgs::Imu imu;
        imu.header.frame_id = "imu";
        
        geometry_msgs::PointStamped currentL, currentR, speedL, speedR, current, speed;
        currentL.header.frame_id = "currentL";
        currentR.header.frame_id = "currentR";
        current.header.frame_id = "current";
        speedL.header.frame_id = "speedL";
        speedR.header.frame_id = "speedR";
        speed.header.frame_id = "speed";

        // initialize hardware
        p = new Protonek(dev, dev2, parL, parR);

        if (useSpeedRegulator)
                p->enableSpeedRegulator();
        else
                p->disableSpeedRegulator();

        p->setupPIDangle(Kp,Ki,Kd);
        p->setupPIDangularVelocity(avKp,avKi,avKd);
        p->setupPIDlinearVelocity(lvKp,lvKi,lvKd);
        
        p->balanceAngle = balanceAngle;
        
        double angular_pos = 0;
        if (p->isConnected()) {

                while (ros::ok()) {
                        double x, y, dist, th, xvel, thvel;
                        double accX, accY, accZ, omegaZ, pitch, pitch2, destAngle;

                        ros::Time current_time = ros::Time::now();

                        p->update();
                        p->updateOdometry();
                        p->getOdometry(x, y, th, dist);
                        p->getVelocity(xvel, thvel);
                        p->getImu(accX, accY, accZ, omegaZ, pitch, pitch2, destAngle);

                        //since all odometry is 6DOF we'll need a quaternion created from yaw
                        geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(th);

                        //next, we'll publish the odometry message over ROS
                        odom.header.stamp = current_time;

                        //set the position
                        odom.pose.pose.position.x = x;
                        odom.pose.pose.position.y = y;
                        odom.pose.pose.position.z = dist;
                        //odom.pose.pose.orientation = odom_quat;
                        odom.pose.pose.orientation.z = th;

                        odom.pose.covariance[0] = 0.00001;
                        odom.pose.covariance[7] = 0.00001;
                        odom.pose.covariance[14] = 10.0;
                        odom.pose.covariance[21] = 1.0;
                        odom.pose.covariance[28] = 1.0;
                        odom.pose.covariance[35] = thvel + 0.001;

                        //set the velocity
                        odom.child_frame_id = "base_link";
                        odom.twist.twist.linear.x = xvel;
                        odom.twist.twist.linear.y = p->GetHorizontalAcceleration();
                        p->getMeanLinearVelocity(odom.twist.twist.linear.z);
                        odom.twist.twist.angular.z = thvel;

                        //publish the message
                        odom_pub.publish(odom);

                        // imu
                        imu.header.stamp = current_time;

                        imu.orientation.x = pitch;
                        imu.orientation.y = pitch2;
                        imu.orientation.z = destAngle;
                        imu.orientation_covariance[0] = 0.01;

                        imu.angular_velocity.y = omegaZ;          // odczyt z gyro
                        imu.angular_velocity.z = p->GetRA();
                        imu.angular_velocity_covariance[0] = 0.01;
                        imu.angular_velocity_covariance[3] = 0.01;
                        imu.angular_velocity_covariance[6] = 0.01;
                        
                        imu.linear_acceleration.x = accX;       // odczyt z akcelerometru
                        imu.linear_acceleration.y = accY;
                        imu.linear_acceleration.z = accZ;
                        imu.linear_acceleration_covariance[0] = 0.01;
                        imu.linear_acceleration_covariance[3] = 0.01;
                        imu.linear_acceleration_covariance[6] = 0.01;
                        
                        imu_pub.publish(imu);

                        // prad i predkosc
                        currentL.point.x = p->bridgeL.currentMeasured;
                        currentL.point.y = p->bridgeL.currentGiven;
                        currentL.header.stamp = current_time;
                        currentL_pub.publish(currentL);

                        speedL.point.x = p->bridgeL.speedMeasured;
                        speedL.point.y = p->bridgeL.speedGiven;
                        speedL.header.stamp = current_time;
                        speedL_pub.publish(speedL);
                        
                        currentR.point.x = p->bridgeR.currentMeasured;
                        currentR.point.y = p->bridgeR.currentGiven;
                        currentR.header.stamp = current_time;
                        currentR_pub.publish(currentR);

                        speedR.point.x = p->bridgeR.speedMeasured;
                        speedR.point.y = p->bridgeR.speedGiven;
                        speedR.header.stamp = current_time;
                        speedR_pub.publish(speedR);

                        current.point.x = (p->bridgeR.currentMeasured+p->bridgeL.currentMeasured)*0.5;
                        current.point.y = (p->bridgeR.currentGiven+p->bridgeL.currentGiven)*0.5;
                        current.header.stamp = current_time;
                        current_pub.publish(current);

                        speed.point.x = (p->bridgeR.speedMeasured+p->bridgeL.speedMeasured)*0.5;
                        speed.point.y = (p->bridgeR.speedGiven+p->bridgeL.speedGiven)*0.5;
                        speed.header.stamp = current_time;
                        speed_pub.publish(speed);
                        
                        ros::spinOnce();
                        loop_rate.sleep();
                }
        } else {
                ROS_ERROR("Connection to device %s failed", dev.c_str());
        }

        return 0;
}
