#ifndef P2_DEMO_CAMERA_H
#define P2_DEMO_CAMERA_H
typedef struct {float distance,angle,fov,offset,weight,detached,near_z,far_z;} CameraPreset;
typedef struct {
    CameraPreset preset[6];
    float close_distance,close_angle,close_fov,collision_radius,collision_speed,collision_height,no_collision_height;
    float setting,rotation,follow,accel,max_speed,damp;
} CameraParameters;
typedef struct {
    float target_theta,theta,distance,phi,fov,near_z,far_z;
    float look_x,look_y,look_z,follow_time,smooth,hold_r;
    unsigned zoom,high;
    int close,collision_ready;
} PlayCamera;
#endif
