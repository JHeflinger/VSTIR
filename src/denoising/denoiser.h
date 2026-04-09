#ifndef DENOISER_H
#define DENOISER_H
#include <vector>
#include <cstring>
#include <array>
struct col3f 
{
    std::array<float, 3> comps;
    inline float operator()(int idx) {return comps[idx];}
    inline void set(float r, float g, float b) {comps[0] = r; comps[1] = g; comps[2] = b;}
};
// uses haar with multilevels.
void wavelet_transform_1d(float*, int);
void inv_wavelet_transform_1d(float*, int);

void wavelet_transform_2d(std::vector<float>&, int w, int h, int gw);
void inv_wavelet_transform_2d(std::vector<float>&, int w, int h, int gw);

float median(std::vector<float>& data);
float estimate_sd(std::vector<float>& img, int width, int height, int stride);

void soft_thresh_img(std::vector<float>& data, int w, int h, int gw, int gh, float bias);
float soft_thresh(float v, float t);
void denoise(std::vector<col3f>& img, int stride, float bias);
void extract_components(std::vector<col3f>& img, 
        std::vector<float>& r, 
        std::vector<float>& g, 
        std::vector<float>& b
        );

void combine_components(std::vector<col3f>& img, 
        std::vector<float>& r, 
        std::vector<float>& g, 
        std::vector<float>& b
        );
#endif
