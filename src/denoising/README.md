# Haar Denoiser
the denoiser binary runs the denoiser on all images in the `data/` folder and outputs it to the `denoised_data/` folder.
To run do:
```sh
denoiser bias
```
where bias is any float number. Below is some sample images with different bias values

| original | 0.0 | 0.2 | 0.7|
| --- | --- | --- | --- |
| ![original](./data/noise1.png) | ![no bias](./denoised_data/noise1_bias0.png) | ![bias=0.2](./denoised_data/noise1_bias0.2.png) | ![bias=0.7](./denoised_data/noise1_bias0.7.png)|
| ![original](./data/noise2.png) | ![no bias](./denoised_data/noise2_bias0.png) | ![bias=0.2](./denoised_data/noise2_bias0.2.png) | ![bias=0.7](./denoised_data/noise2_bias0.7.png)|
| ![original](./data/noise3.png) | ![no bias](./denoised_data/noise3_bias0.png) | ![bias=0.2](./denoised_data/noise3_bias0.2.png) | ![bias=0.7](./denoised_data/noise3_bias0.7.png)|

## limitation
only works when width and height are powers of 2.
