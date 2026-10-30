#ifndef __OCR_DBNET_H__
#define __OCR_DBNET_H__

#include "baseStruct.h"
#include "net.h"
#include <memory>
#include "mat.h"
#include <opencv2/opencv.hpp>
#include <vector>

class dbNet {
public:
    dbNet();

    ~dbNet() {};


    void initParam(std::string param) {
        net.load_param(param.data());
    };

    void initBin(std::string bin) {
        net.load_model(bin.data());
    };


    bool forward(cv::Mat& src, std::vector<TextBox>& results_);

private:
    ncnn::Net net;
    const float meanValues[3] = { 0.485f * 255.f, 0.456f * 255.f, 0.406f * 255.f };
    const float normValues[3] = { 1.0f / 0.229f / 255.0f, 1.0f / 0.224f / 255.0f,
                                 1.0f / 0.225f / 255.0f };
    float boxThresh = 0.3f;
    float boxScoreThresh = 0.5f;
    float unClipRatio = 2.0f;

};

#endif //__OCR_DBNET_H__