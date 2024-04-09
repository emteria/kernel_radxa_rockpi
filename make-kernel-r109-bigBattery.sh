#!/bin/bash
make ARCH=arm64 r109_bigBattery_defconfig android-11.config && \
cp logo-270.bmp logo.bmp && \
cp drivers/power/supply/rk817_charger.c_bigBattery drivers/power/supply/rk817_charger.c && \
cp drivers/rtc/rtc-rk808.c_bigBattery drivers/rtc/rtc-rk808.c && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3568-evb2-lp4x-rk817-r109-mipi-bigBattery.img -j24 && \
git checkout drivers/power/supply/rk817_charger.c && \
git checkout drivers/rtc/rtc-rk808.c
