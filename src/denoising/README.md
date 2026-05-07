# Haar Denoiser
the denoiser binary runs the denoiser on all images in the `data/` folder and outputs it to the `denoised_data/` folder.
To run do:
```sh
denoiser bias
```
where bias is any float number. Below is some sample images with different bias values

## Global version
this version operates on the entire image

### General Samples
| original | 0.0 | 0.02 | 0.05|
| --- | --- | --- | --- |
| ![original](./data/noise1.png) | ![no bias](./denoised_data/bias0_noise1.png) | ![bias=0.02](./denoised_data/bias0.02_noise1.png) | ![bias=0.05](./denoised_data/bias0.05_noise1.png)|
| ![original](./data/noise2.png) | ![no bias](./denoised_data/bias0_noise2.png) | ![bias=0.02](./denoised_data/bias0.02_noise2.png) | ![bias=0.05](./denoised_data/bias0.05_noise2.png)|
| ![original](./data/noise3.png) | ![no bias](./denoised_data/bias0_noise3.png) | ![bias=0.02](./denoised_data/bias0.02_noise3.png) | ![bias=0.05](./denoised_data/bias0.05_noise3.png)|
| ![original](./data/cute_cat.jpg) | ![no bias](./denoised_data/bias0_cute_cat.jpg) | ![bias=0.02](./denoised_data/bias0.02_cute_cat.jpg) | ![bias=0.05](./denoised_data/bias0.05_cute_cat.jpg)|
| ![original](./data/cool1.jpg) | ![no bias](./denoised_data/bias0_cool1.jpg) | ![bias=0.02](./denoised_data/bias0.02_cool1.jpg) | ![bias=0.05](./denoised_data/bias0.05_cool1.jpg)|
| ![original](./data/cool2.jpg) | ![no bias](./denoised_data/bias0_cool2.jpg) | ![bias=0.02](./denoised_data/bias0.02_cool2.jpg) | ![bias=0.05](./denoised_data/bias0.05_cool2.jpg)|

### Cornell Samples
(main failure on second image)
| original | 0.0 | 0.02 |
| --- | --- | ---|
| ![original](./data/cornell_nee_norestir.png) | ![no bias](./denoised_data/bias0_cornell_nee_norestir.png) | ![bias=0.02](./denoised_data/bias0.02_cornell_nee_norestir.png) 
| ![original](./data/cornell_nonee_restir.png) | ![no bias](./denoised_data/bias0_cornell_nonee_restir.png) | ![bias=0.02](./denoised_data/bias0.02_cornell_nonee_restir.png) 
| ![original](./data/cornell_nee_restir1.png) | ![no bias](./denoised_data/bias0_cornell_nee_restir1.png) | ![bias=0.02](./denoised_data/bias0.02_cornell_nee_restir1.png) 
| ![original](./data/cornell_nee_restir2.png) | ![no bias](./denoised_data/bias0_cornell_nee_restir2.png) | ![bias=0.02](./denoised_data/bias0.02_cornell_nee_restir2.png) 

| original | 0.05 | 0.07|
| --- | --- | --- |
| ![original](./data/cornell_nee_norestir.png) | ![bias=0.05](./denoised_data/bias0.05_cornell_nee_norestir.png) | ![bias=0.07](./denoised_data/bias0.07_cornell_nee_norestir.png)|
| ![original](./data/cornell_nonee_restir.png) | ![bias=0.05](./denoised_data/bias0.05_cornell_nonee_restir.png) | ![bias=0.07](./denoised_data/bias0.07_cornell_nonee_restir.png)|
| ![original](./data/cornell_nee_restir1.png) | ![bias=0.05](./denoised_data/bias0.05_cornell_nee_restir1.png) | ![bias=0.07](./denoised_data/bias0.07_cornell_nee_restir1.png)|
| ![original](./data/cornell_nee_restir2.png) | ![bias=0.05](./denoised_data/bias0.05_cornell_nee_restir2.png) | ![bias=0.07](./denoised_data/bias0.07_cornell_nee_restir2.png)|

## Local version:
operates on subdivisions on image then combines. Below is subdivision twice.

### General Samples
| original | 0.0 | 0.02 | 0.05|
| --- | --- | --- | --- |
| ![original](./data/noise1.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_noise1.png) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_noise1.png) | ![bias_divisions2=0.05](./denoised_subdivide_data/bias0.05_divisions2_noise1.png)|
| ![original](./data/noise2.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_noise2.png) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_noise2.png) | ![bias_divisions2=0.05](./denoised_subdivide_data/bias0.05_divisions2_noise2.png)|
| ![original](./data/noise3.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_noise3.png) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_noise3.png) | ![bias_divisions2=0.05](./denoised_subdivide_data/bias0.05_divisions2_noise3.png)|
| ![original](./data/cute_cat.jpg) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_cute_cat.jpg) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_cute_cat.jpg) | ![bias_divisions2=0.05](./denoised_subdivide_data/bias0.05_divisions2_cute_cat.jpg)|
| ![original](./data/cool1.jpg) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_cool1.jpg) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_cool1.jpg) | ![bias_divisions2=0.05](./denoised_subdivide_data/bias0.05_divisions2_cool1.jpg)|
| ![original](./data/cool2.jpg) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_cool2.jpg) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_cool2.jpg) | ![bias_divisions2=0.05](./denoised_subdivide_data/bias0.05_divisions2_cool2.jpg)|

### Cornell Samples
(main failure on second image)
| original | 0.0 | 0.02 |
| --- | --- | ---|
| ![original](./data/cornell_nee_norestir.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_cornell_nee_norestir.png) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_cornell_nee_norestir.png) 
| ![original](./data/cornell_nonee_restir.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_cornell_nonee_restir.png) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_cornell_nonee_restir.png) 
| ![original](./data/cornell_nee_restir1.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_cornell_nee_restir1.png) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_cornell_nee_restir1.png) 
| ![original](./data/cornell_nee_restir2.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0_divisions2_cornell_nee_restir2.png) | ![bias_divisions2=0.02](./denoised_subdivide_data/bias0.02_divisions2_cornell_nee_restir2.png) 

| original | 0.05 | 0.07 |
| --- | --- | ---|
| ![original](./data/cornell_nee_norestir.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0.05_divisions2_cornell_nee_norestir.png) | ![bias_divisions2=0.07](./denoised_subdivide_data/bias0.07_divisions2_cornell_nee_norestir.png) 
| ![original](./data/cornell_nonee_restir.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0.05_divisions2_cornell_nonee_restir.png) | ![bias_divisions2=0.07](./denoised_subdivide_data/bias0.07_divisions2_cornell_nonee_restir.png) 
| ![original](./data/cornell_nee_restir1.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0.05_divisions2_cornell_nee_restir1.png) | ![bias_divisions2=0.07](./denoised_subdivide_data/bias0.07_divisions2_cornell_nee_restir1.png) 
| ![original](./data/cornell_nee_restir2.png) | ![no bias_divisions2](./denoised_subdivide_data/bias0.05_divisions2_cornell_nee_restir2.png) | ![bias_divisions2=0.07](./denoised_subdivide_data/bias0.07_divisions2_cornell_nee_restir2.png) 
