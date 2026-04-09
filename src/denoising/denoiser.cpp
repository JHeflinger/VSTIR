#include <algorithm>
#include <cmath>
#include <iostream>
#include "denoiser.h"
void denoise(std::vector<col3f>& img, int stride, float bias)
{
    int levels = std::floor(std::log2(fmin(stride, img.size()/stride)));
    std::vector<float> imgr(img.size());

    std::vector<float> imgg(img.size());

    std::vector<float> imgb(img.size());

    extract_components(img, imgr, imgg, imgb);

    int currw = stride;
    int currh = img.size() / stride;
    int gh = currh;
    for (int l = 0; l < levels; l++)
    {
        wavelet_transform_2d(imgr, currw, currh, stride);
        wavelet_transform_2d(imgg, currw, currh, stride);
        wavelet_transform_2d(imgb, currw, currh, stride);
        currw /=2;
        currh /=2;
    }
    soft_thresh_img(imgr, stride, gh, stride, img.size()/stride, bias);
    soft_thresh_img(imgg, stride, gh, stride, img.size()/stride, bias);
    soft_thresh_img(imgb, stride, gh, stride, img.size()/stride, bias);
    for (int l = 0; l < levels; l++)
    {
        currw *= 2;
        currh *= 2;
        inv_wavelet_transform_2d(imgr, currw, currh, stride);
        inv_wavelet_transform_2d(imgg, currw, currh, stride);
        inv_wavelet_transform_2d(imgb, currw, currh, stride);
    }
    combine_components(img, imgr, imgg, imgb);
}


void wavelet_transform_2d(std::vector<float>& img, int width, int height, int stride)
{
    // row transformations:
    for (int i = 0; i < height; i++)
    {
        wavelet_transform_1d(&img[i * stride], width);
    }

    // col transformation
    float* img_transpose = new float[width * height];
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            img_transpose[i * height + j] = img[j * stride + i];
        }
    }
    for (int i = 0; i < width; i++)
    {
        wavelet_transform_1d(&img_transpose[i * height], height);
    }

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            img[i * stride + j] = img_transpose[j * height + i];
        }
    }
    delete[] img_transpose;
}

void inv_wavelet_transform_2d(std::vector<float>& img, int width, int height, int stride)
{
    float* img_transpose = new float[width * height];
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            img_transpose[i * height + j] = img[j * stride + i];
        }
    }
    // col transformation
    for (int i = 0; i < width; i++)
    {
        inv_wavelet_transform_1d(&img_transpose[i * height], height);
    }
    for (int j = 0; j < width; j++)
    {
        for (int i = 0; i < height; i++)
        {
            img[i * stride + j] = img_transpose[j * height + i];
        }
    }
    // row transformations:
    for (int i = 0; i < height; i++)
    {
        inv_wavelet_transform_1d(&img[i * stride], width);
    }

    delete[] img_transpose;
}
void wavelet_transform_1d(float* data, int n)
{
    float sq2 = std::sqrt(2);
    float* temp = new float[n];

    std::memset(temp, 0, n * sizeof(float));

    for (int i = 0; i < n/2; i++)
    {
        temp[i] = (data[2*i] + data[2*i+1] ) / sq2;
        temp[i+n/2] = (data[2*i] - data[2*i+1] ) / sq2;
    }
    std::memcpy(data, temp, n * sizeof(float));
    delete[] temp;
}

void inv_wavelet_transform_1d(float* data, int n)
{
    float sq2 = sqrt ( 2.0 );
    float* temp = new float[n];
    std::memset(temp, 0, n * sizeof(float));

    for (int i = 0; i < n/2; i++)
    {
        temp[2*i] = (data[i] + data[i+n/2]) / sq2;
        temp[2*i+1] = (data[i] - data[i+n/2]) / sq2;
    }
    std::memcpy(data, temp,n * sizeof(float));
    delete[] temp;
}

float estimate_sd(std::vector<float>& img, int width, int height, int stride)
{
    std::vector<float> d01;
    d01.reserve(width/2 * height/2 + 2);
    for (int j = height/2; j < height; j++)
    {
        for (int i = width/2; i < width; i++)
        {
            d01.push_back(img[j * stride + i]);
        }
    }
    return median(d01) / 0.6745;
}
void soft_thresh_img(std::vector<float>& data, int width, int height, int gw, int gh, float bias)
{
    float t = estimate_sd(data, gw, gh, gw);
    t *=sqrt(2 * log(width * height));
    t += bias;
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            if (i < width/2 && j < height/2) continue;
            data[j * gw + i] = soft_thresh(data[j * gw + i], t);
        }
    }
}
float soft_thresh(float v, float t)
{
    float k =  ((v > 0) - (v < 0)) * std::max(((float)fabs(v) - t), 0.0f);
    return k;
}

float median(std::vector<float>& data)
{
    std::vector<float> temp(data.begin(), data.end());
    std::sort(temp.begin(), temp.end(), [](const auto& v1, const auto& v2){return std::abs(v1) < std::abs(v2);});
    float mid = std::abs(temp[temp.size() / 2]);
    if (data.size() %2 == 0)
    {
        mid = (mid + std::abs(temp[data.size() / 2 + 1]))/2;
    }
    return mid;
}

void extract_components(std::vector<col3f>& img, 
        std::vector<float>& r, 
        std::vector<float>& g, 
        std::vector<float>& b
        )
{
    for (int i = 0; i < img.size(); i++)
    {
        r[i] = (img[i])(0);
        g[i] = (img[i])(1);
        b[i] = (img[i])(2);
    }
}

void combine_components(std::vector<col3f>& img, 
        std::vector<float>& r, 
        std::vector<float>& g, 
        std::vector<float>& b
        )
{
    for (int i = 0; i < img.size(); i++)
    {
        img[i].set(r[i], g[i], b[i]);
    }
}
