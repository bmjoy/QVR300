#ifndef ARCORE_API_H
#define ARCORE_API_H
#include <string>
#include <vector>
#include <functional>

#define ARCORE_API_EXPORT __attribute__ ((visibility("default")))
//dep
ARCORE_API_EXPORT int ARCORE_init(const std::string &config_file_path, const std::string &vocab_file_path);
ARCORE_API_EXPORT int ARCORE_release();
ARCORE_API_EXPORT void ARCORE_feed_images(const char *cameraImageData,double timestamp);
ARCORE_API_EXPORT void ARCORE_feed_images2(const char *cameraImageDataLeft,const char* cameraImageDataRight,double timestamp);
ARCORE_API_EXPORT void ARCORE_feed_imu(const std::vector<float> &imu,double timestamp);
ARCORE_API_EXPORT int ARCORE_get_opengl_pose(float *q, float *p, double predicttime);//tcw
ARCORE_API_EXPORT void ARCORE_set_pose_callback(std::function<void(float * ,double)> callback);

#ifdef __cplusplus
extern "C" {
#endif
ARCORE_API_EXPORT int ARCORE_get_pose(float *pose,double predicttime);//twc
ARCORE_API_EXPORT int ARCORE_get_plane(int x, int y, float &A, float &B, float &C, float& pointX, float& pointY, float& pointZ);
ARCORE_API_EXPORT int ARCORE_get_anchor(float& pointX, float& pointY, float& pointZ);
#ifdef __cplusplus
}
#endif
#endif //ARCORE_API_H
