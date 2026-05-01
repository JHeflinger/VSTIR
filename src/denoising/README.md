# Haar Denoiser
the denoiser binary runs the denoiser on all images in the `data/` folder and outputs it to the `denoised_data/` folder.
To run do:
```sh
denoiser bias
```
where bias is any float number. Below is some sample images with different bias values

| original | 0.0 | 0.02 | 0.07|
| --- | --- | --- | --- |
| ![original](./data/noise1.png) | ![no bias](./denoised_data/bias0_noise1.png) | ![bias=0.02](./denoised_data/bias0.02_noise1.png) | ![bias=0.07](./denoised_data/bias0.07_noise1.png)|
| ![original](./data/noise2.png) | ![no bias](./denoised_data/bias0_noise2.png) | ![bias=0.02](./denoised_data/bias0.02_noise2.png) | ![bias=0.07](./denoised_data/bias0.07_noise2.png)|
| ![original](./data/noise3.png) | ![no bias](./denoised_data/bias0_noise3.png) | ![bias=0.02](./denoised_data/bias0.02_noise3.png) | ![bias=0.07](./denoised_data/bias0.07_noise3.png)|
| ![original](./data/cute_cat.jpg) | ![no bias](./denoised_data/bias0_cute_cat.png) | ![bias=0.02](./denoised_data/bias0.02_cute_cat.png) | ![bias=0.07](./denoised_data/bias0.07_cute_cat.png)|
