#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <cstdlib>
// #include "opencv2/freetype.hpp"
#include "crnnNet.h"
#include "dbNet.h"
#include "tools.h"
#include "boxSorter.h"

class ocrDetect
{
public:
    ocrDetect();  // Constructor for network initialization
    bool dataVerify(const std::vector<TextLine>& recResult);   // Perform data verification
    int getDetectResult(cv::Mat& image);

private:
    dbNet detectNet;
    crnnNet recNet;
    std::vector<TextBox> boxResult;
    std::vector<TextLine> recResult;

private:
    std::string detNetParam = "D:/tool/Qt/Project/modbusCamera-main/weights/det.param";
    std::string detNetBin = "D:/tool/Qt/Project/modbusCamera-main/weights/det.bin";
    std::string recNetParam = "D:/tool/Qt/Project/modbusCamera-main/weights/ch_PP_OCRv4_rec_infer.ncnn.param";
    std::string recNetBin = "D:/tool/Qt/Project/modbusCamera-main/weights/ch_PP_OCRv4_rec_infer.ncnn.bin";
    std::string initKeys = "D:/tool/Qt/Project/modbusCamera-main/weights/dict_chi_sim.txt";
};
