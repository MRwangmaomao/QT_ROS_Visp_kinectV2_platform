﻿/**
 * @file /src/qnode.cpp
 *
 * @brief Ros communication central!
 *
 * @date February 2018
 **/

/*****************************************************************************
** Includes
*****************************************************************************/
#include <ros/ros.h>
#include <ros/network.h>
#include <string>
#include <std_msgs/String.h>
#include <sstream>
#include <qnode.h>
#include <QString>
#include <QDebug>

/*****************************************************************************
** Namespaces
*****************************************************************************/
namespace test_gui
{

/*****************************************************************************
** Implementation
*****************************************************************************/
QNode::QNode(int argc, char** argv )
    : init_argc(argc), init_argv(argv)
{
    orignal_I.init(540,960);
    vp_blob_init_done = false;
#if defined(VISP_HAVE_X11)
//    vpDisplayX d(orignal_I, vpDisplay::SCALE_AUTO);
#elif defined(VISP_HAVE_GDI)
    vpDisplayGDI d(orignal_I, vpDisplay::SCALE_AUTO);
#endif
}

QNode::~QNode()
{
    if(ros::isStarted())
    {
        ros::shutdown(); // explicitly needed since we use ros::start();
        ros::waitForShutdown();
    }
    wait();
}



bool QNode::init()
{
    ros::init(init_argc,init_argv,"test_gui");
    if (!ros::master::check())
    {
        return false;
    }
    ros::start(); // explicitly needed since our nodehandle is going out of scope.
    ros::NodeHandle n;
    image_transport::ImageTransport it(n);
    image_transport::Subscriber image_sub;
    image_sub = it.subscribe("/kinect2/qhd/image_color",1,&QNode::myCallback_img,this);
    image_transport::Subscriber depth_sub;
    depth_sub = it.subscribe("/kinect2/sd/image_depth",1,&QNode::myCallback_depth,this);
    start();
    ROS_INFO("I'm Starting!");

    return true;
}

bool QNode::init(const std::string &master_url, const std::string &host_url)
{
    std::map<std::string,std::string> remappings;
    remappings["__master"] = master_url;
    remappings["__hostname"] = host_url;
    ros::init(remappings,"test_gui");
    if (!ros::master::check())
    {
        return false;
    }
    ros::start(); // explicitly needed since our nodehandle is going out of scope.
    ros::NodeHandle n;
    image_transport::ImageTransport it(n);
    image_transport::Subscriber image_sub;
    image_sub = it.subscribe("/kinect2/qhd/image_color",1,&QNode::myCallback_img,this);
    image_transport::Subscriber depth_sub;
    depth_sub = it.subscribe("/kinect2/sd/image_depth",1,&QNode::myCallback_depth,this);
    start();
    return true;
}

void QNode::RecvTopicCallback(const std_msgs::StringConstPtr &msg)
{
     log_listen(Info, std::string("I heard: ")+msg->data.c_str());
}

void QNode::myCallback_img(const sensor_msgs::ImageConstPtr &msg)
{
    cv_bridge::CvImageConstPtr cv_ptrRGB;
    try
    {
        cv_ptrRGB = cv_bridge::toCvCopy(msg);
        img = cv_ptrRGB->image;
        image = QImage(img.data,img.cols,img.rows,img.step[0],QImage::Format_RGB888);//change  to QImage format

        vpImageConvert::convert(img, orignal_I);

        if (!vp_blob_init_done) {
            vpDisplay::displayText(orignal_I, vpImagePoint(10, 10), "Click in the blob to initialize the tracker", vpColor::green);
            if (vpDisplay::getClick(orignal_I, germ, false)) {
            blob.initTracking(orignal_I, germ);
            vp_blob_init_done = true;
            }
        }
        else {
            blob.track(orignal_I);
        }

//        vpDisplay::setTitle(orignal_I, "My image");
//        vpDisplay::display(orignal_I);
//        //vpDisplay::displayPoint(I, I.getHeight() / 2, I.getWidth() / 2, vpColor::red, 2);
//        vpDisplay::flush(orignal_I);

        vpImageConvert::convert(orignal_I,vpimg);
        vpimage = QImage(vpimg.data,vpimg.cols,vpimg.rows,QImage::Format_Indexed8);//change  to QImage format
        Q_EMIT loggingCamera();
        cv::waitKey(100);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
}

void QNode::myCallback_depth(const sensor_msgs::ImageConstPtr &msg)
{
    cv_bridge::CvImageConstPtr cv_ptrDEPTH;
    try
    {
        cv_ptrDEPTH = cv_bridge::toCvCopy(msg);
        dph = cv_ptrDEPTH->image;
//        depthnearestFiltering(dph);
       unsigned short max_depth;

       uchar p_depth_argb[424*512*4];
       int idx = 0;
        //find depth max
       for (int x = 0; x < dph.rows; x++) {
           for (int y = 0; y < dph.cols; y++) {
                if(dph.at<unsigned short>(x,y) > max_depth)
                    max_depth = dph.at<unsigned short>(x,y);
           }
       }

       for (int x = 0; x < dph.rows; x++) {
           for (int y = 0; y < dph.cols; y++) {
       //一定要使用1.0f相乘，转换成float类型，否则该工程的结果会有错误,因为这个要么是0，要么是1，0的概率要大很多
                   float fscale = 1.0f*(dph.at<unsigned short>(x,y))/max_depth;
                   if(dph.at<unsigned short>(x,y) != 0) {
                       p_depth_argb[idx++] = 255*(1-fscale);    //蓝色分量
                       p_depth_argb[idx++] = 0; //绿色分量
                       p_depth_argb[idx++] = 255*fscale
                               ;   //红色分量，越远越红
                       p_depth_argb[idx++] = 255*(1-fscale); //距离越近，越不透明
                   }
                   else {
                       p_depth_argb[idx++] = 0;
                       p_depth_argb[idx++] = 0;
                       p_depth_argb[idx++] = 0;
                       p_depth_argb[idx++] = 255;
                   }
            }
       }
        depth = QImage(p_depth_argb, dph.cols , dph.rows,  QImage::Format_ARGB32);//change  to QImage format
        Q_EMIT loggingDepthCamera();
        cv::waitKey(100);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
}

void QNode::depthnearestFiltering(cv::Mat & depthSrc)
{
    CvPoint topLeft;
    CvPoint downRight;
    cv::Mat tempImage;
    depthSrc.copyTo(tempImage);

    CvRect image_ROI(80, 60, 350,250);

//    qDebug() << depthSrc.cols << " " << depthSrc.rows << " " << image_ROI.width << " " << image_ROI.height << " " ;
    for(int i = image_ROI.y; i < image_ROI.y + image_ROI.height; i++)
    {
        unsigned short* pix_data = (unsigned short*)(depthSrc.data + depthSrc.cols *i);
        for (int j = image_ROI.x; j < image_ROI.x + image_ROI.width; j++) {
            for (int k = 1; pix_data[j] == 0; k++) {
                topLeft = cvPoint(j - k, i - k);
                downRight = cvPoint(j + k, i + k);
                // 左上点和右下点
                // (j-k,i-k) ( j ,i-k) (j+k,i-k)
                // (j-k, i ) ( j , i ) (j+k, i )
                // (j-k,i+k) ( j ,i+k) (j+k,i+k)
                /********************************************************/
                for (int m = topLeft.x; (m <= downRight.x)&&(pix_data[j] == 0); m++) {
                    if(m < 0) continue;
                    if(m >= image_ROI.width) break;
                    if(topLeft.y >= 0){
                        unsigned short temp = tempImage.at<unsigned short>(m,downRight.x);
                        if(temp > 0)
                        {
                            pix_data[j] = temp;
                            break;
                        }
                    }
                    if (downRight.y < image_ROI.height)
                    {
                        // 获取中心点(j,i)右下角(downRight.y,m)位置数据
                        unsigned short temp = tempImage.at<unsigned short>(downRight.y, m);
                        if (temp > 0)
                        {
                            pix_data[j] = temp;
                            break;
                        }
                    }
                }

                for (int m = topLeft.y; (m<downRight.y) && (pix_data[j] == 0); m++) {
                    if (m<0) continue;
                    if (m >= image_ROI.height) break;
                    if (topLeft.x>0)
                    {
                        unsigned short temp = tempImage.at<unsigned short>(m, topLeft.x);
                        if (temp > 0)
                        {
                            pix_data[j] = temp;
                            break;
                        }
                    }
                    if (downRight.x< image_ROI.width)
                    {
                        unsigned short temp = tempImage.at<unsigned short>(m, downRight.x);
                        if (temp > 0)
                        {
                            pix_data[j] = temp;
                            break;
                        }
                    }
                }
                /********************************************************/
            }
        }
    }
    tempImage.release();
}

void QNode::run()
{
    ros::Rate loop_rate(1);
    ros::NodeHandle n;
    image_transport::ImageTransport it(n);
    image_transport::Subscriber image_sub;
    image_sub = it.subscribe("/kinect2/qhd/image_color",1,&QNode::myCallback_img,this);
    image_transport::Subscriber depth_sub;
    depth_sub = it.subscribe("/kinect2/sd/image_depth",1,&QNode::myCallback_depth,this);
    ros::spin();
    loop_rate.sleep();
    std::cout << "Ros shutdown, proceeding to close the gui." << std::endl;
    Q_EMIT rosShutdown(); // used to signal the gui for a shutdown (useful to roslaunch)

}

void QNode::log(const LogLevel &level, const std::string &msg)
{
    logging_model.insertRows(logging_model.rowCount(),1);
    std::stringstream logging_model_msg;
    switch (level)
    {
        case(Debug):
        {
            ROS_DEBUG_STREAM(msg);
            logging_model_msg << "[DEBUG] [" << ros::Time::now() << "]: " << msg;
            break;
        }
        case(Info):
        {
            ROS_INFO_STREAM(msg);
            logging_model_msg << "[INFO] [" << ros::Time::now() << "]: " << msg;
            break;
        }
        case(Warn):
        {
            ROS_WARN_STREAM(msg);
            logging_model_msg << "[INFO] [" << ros::Time::now() << "]: " << msg;
            break;
        }
        case(Error):
        {
            ROS_ERROR_STREAM(msg);
            logging_model_msg << "[ERROR] [" << ros::Time::now() << "]: " << msg;
            break;
        }
        case(Fatal):
        {
            ROS_FATAL_STREAM(msg);
            logging_model_msg << "[FATAL] [" << ros::Time::now() << "]: " << msg;
            break;
        }
    }
    QVariant new_row(QString(logging_model_msg.str().c_str()));
    logging_model.setData(logging_model.index(logging_model.rowCount()-1),new_row);
    Q_EMIT loggingUpdated(); // used to readjust the scrollbar
}

void QNode::log_listen(const LogLevel &level, const std::string &msg)
{
    logging_listen.insertRows(logging_listen.rowCount(),1);
    std::stringstream logging_model_msg;
    switch (level)
    {
        case(Debug):
        {
            ROS_DEBUG_STREAM(msg);
            logging_model_msg << "[DEBUG] [" << ros::Time::now() << "]: " << msg;
            break;
        }
        case(Info):
        {
            ROS_INFO_STREAM(msg);
            logging_model_msg << "[INFO] [" << ros::Time::now() << "]: " << msg;
            break;
        }
        case(Warn):
        {
            ROS_WARN_STREAM(msg);
            logging_model_msg << "[INFO] [" << ros::Time::now() << "]: " << msg;
            break;
        }
        case(Error):
        {
            ROS_ERROR_STREAM(msg);
            logging_model_msg << "[ERROR] [" << ros::Time::now() << "]: " << msg;
            break;
        }
        case(Fatal):
        {
            ROS_FATAL_STREAM(msg);
            logging_model_msg << "[FATAL] [" << ros::Time::now() << "]: " << msg;
            break;
        }
    }
    QVariant new_row(QString(logging_model_msg.str().c_str()));
    logging_listen.setData(logging_listen.index(logging_listen.rowCount()-1),new_row);
    Q_EMIT loggingListen(); // used to readjust the scrollbar
}

}  // namespace test_gui
