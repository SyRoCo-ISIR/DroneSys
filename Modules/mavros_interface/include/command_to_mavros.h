#ifndef COMMAND_TO_MAVROS_H
#define COMMAND_TO_MAVROS_H

#include <ros/ros.h>
#include <math_utils.h>
#include <bitset>
#include <mavros_msgs/CommandBool.h>
#include <mavros_msgs/SetMode.h>
#include <mavros_msgs/State.h>
#include <mavros_msgs/AttitudeTarget.h>
#include <mavros_msgs/PositionTarget.h>
#include <mavros_msgs/ActuatorControl.h>
#include <mavros_msgs/MountControl.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TwistStamped.h>
#include <sensor_msgs/Imu.h>
#include <drone_msgs/DroneState.h>
#include <drone_msgs/AttitudeReference.h>
#include <drone_msgs/DroneState.h>
using namespace std;

class command_to_mavros
{
private:
    ros::NodeHandle command_nh;

    ros::Subscriber position_target_sub;
    ros::Subscriber attitude_target_sub;
    ros::Subscriber actuator_target_sub;
    ros::Publisher local_pos_pub;
    ros::Publisher setpoint_raw_local_pub;
    ros::Publisher setpoint_raw_attitude_pub;
    ros::Publisher actuator_setpoint_pub;
    ros::Publisher mount_control_pub;

    void pos_target_cb(const mavros_msgs::PositionTarget::ConstPtr& msg)
    {
        pos_drone_fcu_target = Eigen::Vector3d(msg->position.x, msg->position.y, msg->position.z);

        vel_drone_fcu_target = Eigen::Vector3d(msg->velocity.x, msg->velocity.y, msg->velocity.z);

        accel_drone_fcu_target = Eigen::Vector3d(msg->acceleration_or_force.x, msg->acceleration_or_force.y, msg->acceleration_or_force.z);
    }

    void att_target_cb(const mavros_msgs::AttitudeTarget::ConstPtr& msg)
    {
        q_fcu_target = Eigen::Quaterniond(msg->orientation.w, msg->orientation.x, msg->orientation.y, msg->orientation.z);

        //Transform the Quaternion to euler Angles
        euler_fcu_target = quaternion_to_euler(q_fcu_target);

        rates_fcu_target = Eigen::Vector3d(msg->body_rate.x, msg->body_rate.y, msg->body_rate.z);

        Thrust_target = msg->thrust;
    }

    void actuator_target_cb(const mavros_msgs::ActuatorControl::ConstPtr& msg)
    {
        actuator_target = *msg;
    }

public:
    string uav_name;
    // Setpoint state received from fcu via mavros
    Eigen::Vector3d pos_drone_fcu_target;         // Position Setpoint [from fcu]
    Eigen::Vector3d vel_drone_fcu_target;         // Velocity Setpoint [from fcu]
    Eigen::Vector3d accel_drone_fcu_target;       // Acceleration Setpoint [from fcu]
    Eigen::Quaterniond q_fcu_target;              // Quaternion Setpoint [from fcu]
    Eigen::Vector3d euler_fcu_target;             // Euler angle Setpoint
    Eigen::Vector3d rates_fcu_target;             // Rate Setpoint [from fcu]
    float Thrust_target;                          // Throttle Setpoint [from fcu]
    mavros_msgs::ActuatorControl actuator_target; // Actuator control Setpoint [from fcu]


    command_to_mavros(string Uav_name = ""): command_nh("")
    {
        uav_name = Uav_name;
        
        pos_drone_fcu_target    = Eigen::Vector3d(0.0,0.0,0.0);
        vel_drone_fcu_target    = Eigen::Vector3d(0.0,0.0,0.0);
        accel_drone_fcu_target  = Eigen::Vector3d(0.0,0.0,0.0);
        q_fcu_target            = Eigen::Quaterniond(0.0,0.0,0.0,0.0);
        euler_fcu_target        = Eigen::Vector3d(0.0,0.0,0.0);
        rates_fcu_target        = Eigen::Vector3d(0.0,0.0,0.0);
        Thrust_target           = 0.0;

        // [SUB]
        // Pos / Vel / Acc [Local Fixed Frame ENU_ROS]
        // Mavros/plugins/setpoint_raw.cpp: Mavlink message (POSITION_TARGET_LOCAL_NED) -> uORB message (vehicle_local_position_setpoint.msg)
        position_target_sub = command_nh.subscribe<mavros_msgs::PositionTarget>(uav_name + "/mavros/setpoint_raw/target_local", 10, &command_to_mavros::pos_target_cb,this);

        // Attitude / Rate [Local Fixed Frame ENU_ROS]
        // Mavros/plugins/setpoint_raw.cpp: Mavlink message (ATTITUDE_TARGET (#83)) -> uORB (vehicle_attitude_setpoint.msg)
        attitude_target_sub = command_nh.subscribe<mavros_msgs::AttitudeTarget>(uav_name + "/mavros/setpoint_raw/target_attitude", 10, &command_to_mavros::att_target_cb,this);

        // Actuator contorl, throttle for each single rotation direction motor
        // Mavros/plugins/actuator_control.cpp: Mavlink message (ACTUATOR_CONTROL_TARGET) -> uORB message (actuator_controls.msg)
        actuator_target_sub = command_nh.subscribe<mavros_msgs::ActuatorControl>(uav_name + "/mavros/target_actuator_control", 10, &command_to_mavros::actuator_target_cb,this);

        // [PUB]
		// Pos / Attitude [Local Fixed Frame ENU_ROS]
        // Mavros/plugins/setpoint_position.cpp: Mavlink message (SET_ATTITUDE_TARGET (#82)) + (SET_POSITION_TARGET_LOCAL_NED (#84))
		local_pos_pub = command_nh.advertise<geometry_msgs::PoseStamped>(uav_name + "/mavros/setpoint_position/local", 10);
            
        // Pos / Vel / Acc / Yaw / Yaw_rate [Local Fixed Frame ENU_ROS]
        // Mavros/plugins/setpoint_raw.cpp: Mavlink message (SET_POSITION_TARGET_LOCAL_NED (#84)) -> uORB message (position_setpoint_triplet.msg)
        setpoint_raw_local_pub = command_nh.advertise<mavros_msgs::PositionTarget>(uav_name + "/mavros/setpoint_raw/local", 10);

        // Attitude / Rate [Local Fixed Frame ENU_ROS]
        // Mavros/plugins/setpoint_raw.cpp: Mavlink message (SET_ATTITUDE_TARGET (#82)) -> uORB message (vehicle_attitude_setpoint.msg) + (vehicle_rates_setpoint.msg)
        setpoint_raw_attitude_pub = command_nh.advertise<mavros_msgs::AttitudeTarget>(uav_name + "/mavros/setpoint_raw/attitude", 10);

        // Actuator contorl, throttle for each single rotation direction motor
        // Mavros/plugins/actuator_control.cpp : Mavlink message (SET_ACTUATOR_CONTROL_TARGET) -> uORB message (actuator_controls.msg)
        actuator_setpoint_pub = command_nh.advertise<mavros_msgs::ActuatorControl>(uav_name + "/mavros/actuator_control", 10);

        // Mount control
        // Mavros_extra/plugins/mount_control.cpp
        mount_control_pub = command_nh.advertise<mavros_msgs::MountControl>(uav_name + "/mavros/mount_control/command", 1);

        // [SERVICE]
        // Arming
        // mavros/plugins/command.cpp
        arming_client = command_nh.serviceClient<mavros_msgs::CommandBool>(uav_name + "/mavros/cmd/arming");

        // Mode Switcher
        // mavros/plugins/command.cpp
        set_mode_client = command_nh.serviceClient<mavros_msgs::SetMode>(uav_name + "/mavros/set_mode");
    }

    // [Service]
    ros::ServiceClient arming_client;
    mavros_msgs::CommandBool arm_cmd;
    ros::ServiceClient set_mode_client;
    mavros_msgs::SetMode mode_cmd;

    void idle();
    void takeoff();
    void loiter();
    void land();

    void send_pos_setpoint(const Eigen::Vector3d& pos_sp, float yaw_sp);
    void send_vel_setpoint(const Eigen::Vector3d& vel_sp, float yaw_sp);
    void send_vel_xy_pos_z_setpoint(const Eigen::Vector3d& state_sp, float yaw_sp);
    void send_vel_xy_pos_z_setpoint_yawrate(const Eigen::Vector3d& state_sp, float yaw_rate_sp);
    void send_vel_setpoint_body(const Eigen::Vector3d& vel_sp, float yaw_sp);
    void send_vel_setpoint_yaw_rate(const Eigen::Vector3d& vel_sp, float yaw_rate_sp);
    void send_pos_vel_xyz_setpoint(const Eigen::Vector3d& pos_sp, const Eigen::Vector3d& vel_sp, float yaw_sp);
    void send_acc_xyz_setpoint(const Eigen::Vector3d& accel_sp, float yaw_sp);


    void send_attitude_setpoint(const drone_msgs::AttitudeReference& _AttitudeReference);
    void send_attitude_rate_setpoint(const Eigen::Vector3d& attitude_rate_sp, float thrust_sp);
    void send_attitude_setpoint_yawrate(const drone_msgs::AttitudeReference& _AttitudeReference, float yaw_rate_sp);
    void send_actuator_setpoint(const Eigen::Vector4d& actuator_sp);


    void send_mount_control_command(const Eigen::Vector3d& gimbal_att_sp);
    
};

void command_to_mavros::send_mount_control_command(const Eigen::Vector3d& gimbal_att_sp)
{
  mavros_msgs::MountControl mount_setpoint;
  //
  mount_setpoint.mode = 2;
  mount_setpoint.pitch = gimbal_att_sp[0]; // Gimbal Pitch
  mount_setpoint.roll = gimbal_att_sp[1]; // Gimbal  Yaw
  mount_setpoint.yaw = gimbal_att_sp[2]; // Gimbal  Yaw

  mount_control_pub.publish(mount_setpoint);

}

void command_to_mavros::takeoff()
{
}

void command_to_mavros::land()
{
}

void command_to_mavros::loiter()
{
}

void command_to_mavros::idle()
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.coordinate_frame = 1;

    pos_setpoint.type_mask = 0b010111000111;
    pos_setpoint.velocity.x = 0.0;
    pos_setpoint.velocity.y = 0.0;
    pos_setpoint.velocity.z = 0.0;
    pos_setpoint.yaw_rate = 0.0;

    setpoint_raw_local_pub.publish(pos_setpoint);
}

// px + py + pz + body_yaw [Local Frame ENU_ROS]
void command_to_mavros::send_pos_setpoint(const Eigen::Vector3d& pos_sp, float yaw_sp)
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.type_mask = 0b100111111000;  // 100 111 111 000  xyz + yaw

    //uint8 FRAME_LOCAL_NED = 1
    //uint8 FRAME_BODY_NED = 8
    pos_setpoint.coordinate_frame = 1;

    pos_setpoint.position.x = pos_sp[0];
    pos_setpoint.position.y = pos_sp[1];
    pos_setpoint.position.z = pos_sp[2];

    pos_setpoint.yaw = yaw_sp;

    setpoint_raw_local_pub.publish(pos_setpoint);
}

// vx + vy + vz + body_yaw [Local Frame ENU_ROS]
void command_to_mavros::send_vel_setpoint(const Eigen::Vector3d& vel_sp, float yaw_sp)
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.type_mask = 0b100111000111;

    //uint8 FRAME_LOCAL_NED = 1
    //uint8 FRAME_BODY_NED = 8
    pos_setpoint.coordinate_frame = 1;

    pos_setpoint.velocity.x = vel_sp[0];
    pos_setpoint.velocity.y = vel_sp[1];
    pos_setpoint.velocity.z = vel_sp[2];

    pos_setpoint.yaw = yaw_sp;

    setpoint_raw_local_pub.publish(pos_setpoint);
}

// vx + vy + vz + body_yaw_rate [Local Frame ENU_ROS]
void command_to_mavros::send_vel_setpoint_yaw_rate(const Eigen::Vector3d& vel_sp, float yaw_rate_sp)
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.type_mask = 0b010111000111;

    //uint8 FRAME_LOCAL_NED = 1
    //uint8 FRAME_BODY_NED = 8
    pos_setpoint.coordinate_frame = 1;

    pos_setpoint.velocity.x = vel_sp[0];
    pos_setpoint.velocity.y = vel_sp[1];
    pos_setpoint.velocity.z = vel_sp[2];

    pos_setpoint.yaw_rate = yaw_rate_sp;

    setpoint_raw_local_pub.publish(pos_setpoint);
}

// vx + vy + vz + body_yaw  [Body Frame]
void command_to_mavros::send_vel_setpoint_body(const Eigen::Vector3d& vel_sp, float yaw_sp)
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.type_mask = 0b100111000111;

    //uint8 FRAME_LOCAL_NED = 1
    //uint8 FRAME_BODY_NED = 8
    pos_setpoint.coordinate_frame = 8;

    pos_setpoint.position.x = vel_sp[0];
    pos_setpoint.position.y = vel_sp[1];
    pos_setpoint.position.z = vel_sp[2];

    pos_setpoint.yaw = yaw_sp;

    setpoint_raw_local_pub.publish(pos_setpoint);
}

// vx + vy + pz + body_yaw [Local Frame ENU_ROS]
void command_to_mavros::send_vel_xy_pos_z_setpoint(const Eigen::Vector3d& state_sp, float yaw_sp)
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.type_mask = 0b100111000011;   // 100 111 000 011  vx vy vz z + yaw

    //uint8 FRAME_LOCAL_NED = 1
    //uint8 FRAME_BODY_NED = 8
    pos_setpoint.coordinate_frame = 1;

    pos_setpoint.velocity.x = state_sp[0];
    pos_setpoint.velocity.y = state_sp[1];
    pos_setpoint.velocity.z = 0.0;
    pos_setpoint.position.z = state_sp[2];

    pos_setpoint.yaw = yaw_sp;

    setpoint_raw_local_pub.publish(pos_setpoint);
}

// vx + vy + pz + body_yaw_rate [Local Frame ENU_ROS]
void command_to_mavros::send_vel_xy_pos_z_setpoint_yawrate(const Eigen::Vector3d& state_sp, float yaw_rate_sp)
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.type_mask = 0b010111000011;   // 100 111 000 011  vx vy vz z + yawrate

    //uint8 FRAME_LOCAL_NED = 1
    //uint8 FRAME_BODY_NED = 8
    pos_setpoint.coordinate_frame = 1;

    pos_setpoint.velocity.x = state_sp[0];
    pos_setpoint.velocity.y = state_sp[1];
    pos_setpoint.velocity.z = 0.0;
    pos_setpoint.position.z = state_sp[2];

    pos_setpoint.yaw_rate = yaw_rate_sp;

    setpoint_raw_local_pub.publish(pos_setpoint);
}

// px + py + pz + vx + vy + vz + body_yaw [Local Frame ENU_ROS]
void command_to_mavros::send_pos_vel_xyz_setpoint(const Eigen::Vector3d& pos_sp, const Eigen::Vector3d& vel_sp, float yaw_sp)
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.type_mask = 0b100111000000;   // 100 111 000 000  vx vy　vz x y z+ yaw

    //uint8 FRAME_LOCAL_NED = 1
    //uint8 FRAME_BODY_NED = 8
    pos_setpoint.coordinate_frame = 1;

    pos_setpoint.position.x = pos_sp[0];
    pos_setpoint.position.y = pos_sp[1];
    pos_setpoint.position.z = pos_sp[2];
    pos_setpoint.velocity.x = vel_sp[0];
    pos_setpoint.velocity.y = vel_sp[1];
    pos_setpoint.velocity.z = vel_sp[2];

    pos_setpoint.yaw = yaw_sp;

    setpoint_raw_local_pub.publish(pos_setpoint);
}

// ax + ay + az + body_yaw [Local Frame ENU_ROS]
void command_to_mavros::send_acc_xyz_setpoint(const Eigen::Vector3d& accel_sp, float yaw_sp)
{
    mavros_msgs::PositionTarget pos_setpoint;
    //Bitmask toindicate which dimensions should be ignored (1 means ignoring, 0 means selection; Bit 10 must be set to 0)
    //Bit 1:x, bit 2:y, bit 3:z, bit 4:vx, bit 5:vy, bit 6:vz, bit 7:ax, bit 8:ay, bit 9:az, bit 10:is_force_sp, bit 11:yaw, bit 12:yaw_rate
    //Bit 10 should be set as 0, which means it's not force sp
    pos_setpoint.type_mask = 0b100000111111;

    //uint8 FRAME_LOCAL_NED = 1
    //uint8 FRAME_BODY_NED = 8
    pos_setpoint.coordinate_frame = 1;

    pos_setpoint.acceleration_or_force.x = accel_sp[0];
    pos_setpoint.acceleration_or_force.y = accel_sp[1];
    pos_setpoint.acceleration_or_force.z = accel_sp[2];

    pos_setpoint.yaw = yaw_sp;

    setpoint_raw_local_pub.publish(pos_setpoint);

}

// quaternion attitude + throttle
void command_to_mavros::send_attitude_setpoint(const drone_msgs::AttitudeReference& _AttitudeReference)
{
    mavros_msgs::AttitudeTarget att_setpoint;

    //Mappings: If any of these bits are set, the corresponding input should be ignored:
    //bit 1: body roll rate, bit 2: body pitch rate, bit 3: body yaw rate. bit 4-bit 5: reserved, bit 6: 3D body thrust sp instead of throttle, bit 7: throttle, bit 8: attitude

    att_setpoint.type_mask = 0b00111111;

    att_setpoint.orientation.x = _AttitudeReference.desired_att_q.x;
    att_setpoint.orientation.y = _AttitudeReference.desired_att_q.y;
    att_setpoint.orientation.z = _AttitudeReference.desired_att_q.z;
    att_setpoint.orientation.w = _AttitudeReference.desired_att_q.w;

    att_setpoint.thrust = _AttitudeReference.desired_throttle;

    setpoint_raw_attitude_pub.publish(att_setpoint);
}


// quaternion attitude + throttle + body_yaw_rate
void command_to_mavros::send_attitude_setpoint_yawrate(const drone_msgs::AttitudeReference& _AttitudeReference, float yaw_rate_sp)
{
    mavros_msgs::AttitudeTarget att_setpoint;

    //Mappings: If any of these bits are set, the corresponding input should be ignored:
    //bit 1: body roll rate, bit 2: body pitch rate, bit 3: body yaw rate. bit 4-bit 5: reserved, bit 6: 3D body thrust sp instead of throttle, bit 7: throttle, bit 8: attitude

    att_setpoint.type_mask = 0b00111011;

    att_setpoint.orientation.x = _AttitudeReference.desired_att_q.x;
    att_setpoint.orientation.y = _AttitudeReference.desired_att_q.y;
    att_setpoint.orientation.z = _AttitudeReference.desired_att_q.z;
    att_setpoint.orientation.w = _AttitudeReference.desired_att_q.w;

    att_setpoint.thrust = _AttitudeReference.desired_throttle;

    att_setpoint.body_rate.x = 0.0;
    att_setpoint.body_rate.y = 0.0;
    att_setpoint.body_rate.z = yaw_rate_sp;

    setpoint_raw_attitude_pub.publish(att_setpoint);
}

// body_rate + throttle
void command_to_mavros::send_attitude_rate_setpoint(const Eigen::Vector3d& attitude_rate_sp, float thrust_sp)
{
    mavros_msgs::AttitudeTarget att_setpoint;

    //Mappings: If any of these bits are set, the corresponding input should be ignored:
    //bit 1: body roll rate, bit 2: body pitch rate, bit 3: body yaw rate. bit 4-bit 5: reserved, bit 6: 3D body thrust sp instead of throttle, bit 7: throttle, bit 8: attitude

    att_setpoint.type_mask = 0b10111000;

    att_setpoint.body_rate.x = attitude_rate_sp[0];
    att_setpoint.body_rate.y = attitude_rate_sp[1];
    att_setpoint.body_rate.z = attitude_rate_sp[2];

    att_setpoint.thrust = thrust_sp;

    setpoint_raw_attitude_pub.publish(att_setpoint);
}

// actuator control setpoint [PWM]
void command_to_mavros::send_actuator_setpoint(const Eigen::Vector4d& actuator_sp)
{
    mavros_msgs::ActuatorControl actuator_setpoint;

    actuator_setpoint.group_mix = 0;
    actuator_setpoint.controls[0] = actuator_sp(0);
    actuator_setpoint.controls[1] = actuator_sp(1);
    actuator_setpoint.controls[2] = actuator_sp(2);
    actuator_setpoint.controls[3] = actuator_sp(3);
    actuator_setpoint.controls[4] = 0.0;
    actuator_setpoint.controls[5] = 0.0;
    actuator_setpoint.controls[6] = 0.0;
    actuator_setpoint.controls[7] = 0.0;

    actuator_setpoint_pub.publish(actuator_setpoint);
}


#endif


