#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>
#include "denoiser.h"
void denoise_space_transform(std::vector<col3f>& img, int stride, float bias, bool global)
{
    int max_w = std::max(stride, (int)img.size() / stride);
    max_w = std::pow(2, std::ceil(log2(max_w)));
    int max_h = max_w;

    int levels = std::floor(std::log2(max_w));
    levels = std::max(0, levels);

    std::vector<float> imgr(max_w * max_h, 0.f);

    std::vector<float> imgg(max_w * max_h, 0.f);

    std::vector<float> imgb(max_w * max_h, 0.f);

    extract_to_yspace(img, stride, 
            max_h, max_w,
            imgr, imgg, imgb);

    int currw = max_w;
    int currh = max_h;
    int gh = currh;
    int gw = currw;
    for (int l = 0; l < levels; l++)
    {
        wavelet_transform_2d(imgr, currw, currh, max_w);
        wavelet_transform_2d(imgg, currw, currh, max_w);
        wavelet_transform_2d(imgb, currw, currh, max_w);
        if (!global)
        {
            gw = currw;
            gh = currh;
            soft_thresh_img(imgr, gw, gh, max_w, max_h, bias);
            soft_thresh_img(imgg, gw, gh, max_w, max_h, bias);
            soft_thresh_img(imgb, gw, gh, max_w, max_h, bias);
        }
        currw /=2;
        currh /=2;
    }
    if (global)
    {
        soft_thresh_img(imgr, max_w, max_h, max_w, max_h, bias);
        soft_thresh_img(imgg, max_w, max_h, max_w, max_h, bias);
        soft_thresh_img(imgb, max_w, max_h, max_w, max_h, bias);
    }
    for (int l = 0; l < levels; l++)
    {
        currw *= 2;
        currh *= 2;
        inv_wavelet_transform_2d(imgr, currw, currh, max_w);
        inv_wavelet_transform_2d(imgg, currw, currh, max_w);
        inv_wavelet_transform_2d(imgb, currw, currh, max_w);
    }
    extract_from_yspace(img, stride, max_w, imgr, imgg, imgb);
}
void denoise(std::vector<col3f>& img, int stride, float bias, bool global)
{
    int max_w = std::max(stride, (int)img.size() / stride);
    max_w = std::pow(2, std::ceil(log2(max_w)));
    int max_h = max_w;


    int levels = std::floor(std::log2(max_w));
    levels = std::max(0, levels - 6);

    std::vector<float> imgr(max_w * max_h, 0.f);

    std::vector<float> imgg(max_w * max_h, 0.f);

    std::vector<float> imgb(max_w * max_h, 0.f);

    extract_components(img, stride, 
            max_h, max_w,
            imgr, imgg, imgb);

    int currw = max_w;
    int currh = max_h;
    int gh = currh;
    int gw = currw;
    for (int l = 0; l < levels; l++)
    {
        wavelet_transform_2d(imgr, currw, currh, max_w);
        wavelet_transform_2d(imgg, currw, currh, max_w);
        wavelet_transform_2d(imgb, currw, currh, max_w);
        if (!global)
        {
            gw = currw;
            gh = currh;
            soft_thresh_img(imgr, gw, gh, max_w, max_h, bias);
            soft_thresh_img(imgg, gw, gh, max_w, max_h, bias);
            soft_thresh_img(imgb, gw, gh, max_w, max_h, bias);
        }
        currw /=2;
        currh /=2;
    }
    if (global)
    {
        soft_thresh_img(imgr, max_w, max_h, max_w, max_h, bias);
        soft_thresh_img(imgg, max_w, max_h, max_w, max_h, bias);
        soft_thresh_img(imgb, max_w, max_h, max_w, max_h, bias);
    }
    for (int l = 0; l < levels; l++)
    {
        currw *= 2;
        currh *= 2;
        inv_wavelet_transform_2d(imgr, currw, currh, max_w);
        inv_wavelet_transform_2d(imgg, currw, currh, max_w);
        inv_wavelet_transform_2d(imgb, currw, currh, max_w);
    }
    combine_components(img, stride, max_w, imgr, imgg, imgb);
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
    float global_t = estimate_sd(data, gw, gh, gw);
    float t = estimate_sd(data, width, height, gw);
    t = sqrt(std::max(t * t - global_t * global_t, 0.f));
    t = global_t * global_t / (t + 1e-4);
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
        mid = (mid + std::abs(temp[data.size() / 2 - 1]))/2;
    }
    return mid;
}

void extract_components(std::vector<col3f>& img, 
        int stride_main,
        int new_height,
        int new_width,
        std::vector<float>& r, 
        std::vector<float>& g, 
        std::vector<float>& b
        )
{
    for (int j = 0; j < new_height; j++)
    {
        for (int i = 0; i < new_width; i++)
        {
            int y = std::min((int)img.size() / stride_main - 1, j);
            int x = std::min(stride_main - 1, i);
            if ((y < img.size() / stride_main) && x < stride_main)
            {
                r[j * new_width + i] = (img[y * stride_main + x])(0);
                g[j * new_width + i] = (img[y * stride_main + x])(1);
                b[j * new_width + i] = (img[y * stride_main + x])(2);
            }
        }
    }
}

void combine_components(std::vector<col3f>& img, 
        int stride_main,
        int stride_comp,
        std::vector<float>& r, 
        std::vector<float>& g, 
        std::vector<float>& b
        )
{
    for (int j = 0; j < img.size() / stride_main; j++)
    {
        for (int i = 0; i < stride_main; i++)
        {
            int new_idx = j * stride_comp + i;
            img[j * stride_main + i].set(r[new_idx], g[new_idx], b[new_idx]);
        }
    }
}


void extract_to_yspace(
        std::vector<col3f>& img, 
        int stride_main,
        int new_height,
        int new_width,
        std::vector<float>& y_space, 
        std::vector<float>& cb, 
        std::vector<float>& cr
        )
{
    for (int j = 0; j < new_height; j++)
    {
        for (int i = 0; i < new_width; i++)
        {
            int y = std::min((int)img.size() / stride_main - 1, j);
            int x = std::min(stride_main - 1, i);
            if ((y < img.size() / stride_main) && x < stride_main)
            {
                auto [r, g, b] = 
                    (float[]){
                        (img[y * stride_main + x])(0),
                        (img[y * stride_main + x])(1),
                        (img[y * stride_main + x])(2)
                    };
                float yp = 0.299f*r + 0.587f*g + 0.114f*b;
                y_space[j * new_width + i] = yp;
                cb[j * new_width + i] = (b - yp) * 0.564f;
                cr[j * new_width + i] = (r - yp) * 0.713f;
            }
        }
    }
}

void extract_from_yspace(
        std::vector<col3f>& img, 
        int stride_main,
        int stride_comp,
        std::vector<float>& yp, 
        std::vector<float>& cb, 
        std::vector<float>& cr
        )
{
    for (int j = 0; j < img.size() / stride_main; j++)
    {
        for (int i = 0; i < stride_main; i++)
        {
            int new_idx = j * stride_comp + i;
            float r = std::clamp(yp[new_idx] + 1.403f * cr[new_idx], 0.f, 1.f);
            float g = std::clamp(yp[new_idx] - 0.334f * cb[new_idx] - 0.714f * cr[new_idx], 0.f, 1.f);
            float b = std::clamp(yp[new_idx] + 1.773f * cb[new_idx], 0.f, 1.f);
            img[j * stride_main + i].set(r, g, b);
        }
    }
}
