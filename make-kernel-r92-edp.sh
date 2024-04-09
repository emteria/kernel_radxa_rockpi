#!/bin/bash
make ARCH=arm64 r92_defconfig android-11.config && \
cp logo-0.bmp logo.bmp && \
cp drivers/video/backlight/pwm_bl.c-led drivers/video/backlight/pwm_bl.c && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3566-rk809-lp4x-r92-edp.img -j24 && \
cp drivers/video/backlight/pwm_bl.c-normal drivers/video/backlight/pwm_bl.c
