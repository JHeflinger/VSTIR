#include "denoiser.h"
#include "./stb/stb_image.h"
#include "./stb/stb_image_write.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
std::pair<int, std::vector<col3f>> load_img(std::string&& file)
{
    int w;
    int h;
    int c;
   auto* img = stbi_load(file.c_str(), &w, &h, &c, STBI_rgb); 
   if (img != nullptr)
   {
   std::vector<col3f> img_f(w * h);
   for (int i = 0; i < w * h; i++)
   {
       img_f[i].comps[0] = (float)img[3 * i + 0] / 0xff;
       img_f[i].comps[1] = (float)img[3 * i + 1] / 0xff;
       img_f[i].comps[2] = (float)img[3 * i + 2] / 0xff;
   }
   return {w,img_f};
   }
   return {-1, {}};
}
void write_img(const std::vector<col3f>& data, int stride,  std::string&& file)
{
    struct u8col
    {
        std::array<uint8_t, 3> comps;
    };
    std::vector<u8col> u8data(data.size());
    for (int i = 0; i < data.size(); i++)
    {
        for (int j = 0; j < 3; j++)
        {
            u8data[i].comps[j] = std::clamp((uint8_t)(data[i].comps[j] * 0xff), (uint8_t)0, (uint8_t)0xff);
        }
    }
    stbi_write_png(file.c_str(), stride, data.size()/stride, 3, u8data.data(), stride * 3);
}
int main(int argc, char** argv)
{
    std::stringstream ss;
    float bias = std::stof(argv[1]) * (argc >= 2);
    ss << bias;
    std::vector<std::string> paths;
    if (argc < 3)
    {
        for (const auto& entry :
                std::filesystem::directory_iterator("./data/"))
        {
            if (entry.is_directory() || !entry.path().has_filename())[[unlikely]]
            {
                std::cerr<< "cannot support directories yet, skipping..." << '\n';
                continue;
            }
            paths.push_back("./data/" + std::string(entry.path().stem()) + std::string(entry.path().extension()));
        }
    }
    else 
    {
        for (int i = 2; i < argc; i++)
        {
            paths.push_back(std::string(argv[i]));
        }
    }
        for (const auto& path: paths)
        {
            auto [stride, img] = load_img(std::string(path));
            if (stride < 0)
            {
                continue;
            }
            denoise(img, stride, bias, false);
            int last = path.find_last_of("/");
            if (last == std::string::npos)
            {
                last = 0;
            }
            else 
            {
                last += 1;
            }
            write_img(img, stride,
                    "./denoised_data/bias"+ss.str()+"_" + std::string(&path[last]));
            

        }
}
