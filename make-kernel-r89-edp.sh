#!/bin/bash
make ARCH=arm64 r89_defconfig android-11.config && \
cp logo-0.bmp logo.bmp && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3568-evb2-lp4x-r89-edp.img -j24
