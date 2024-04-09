#!/bin/bash
make ARCH=arm64 r98_defconfig android-11.config && \
cp logo-0.bmp logo.bmp && cp logo-90.bmp logo_one.bmp && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3568-evb2-lp4x-rk809-r98-wd1518t-v1.0.img -j24
