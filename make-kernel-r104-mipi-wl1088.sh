#!/bin/bash
make ARCH=arm64 r104_defconfig android-11.config && \
cp logo-270.bmp logo.bmp && \
cp drivers/video/backlight/pwm_bl.c_r104 drivers/video/backlight/pwm_bl.c && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3568-evb2-lp4x-rk817-r104-mipi-wl1088.img -j24 && \
cp drivers/video/backlight/pwm_bl.c-normal drivers/video/backlight/pwm_bl.c
