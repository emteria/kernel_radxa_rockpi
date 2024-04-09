echo "start patch"
cp drivers/media/i2c/ov5648.c_r70 drivers/media/i2c/ov5648.c && \
cp drivers/rtc/rtc-hym8563.c_r70 drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c_r70 kernel/reboot.c && \
echo "end patch"
make ARCH=arm64 r100_defconfig android-11.config && \
cp logo-0.bmp logo.bmp && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3399-evb-ind-lpddr4-android-avb-r100-mipi.img -j24 && \
echo "start checkout patch"
cp drivers/media/i2c/ov5648.c-nomal drivers/media/i2c/ov5648.c && \
cp drivers/rtc/rtc-hym8563.c-nomal drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c-nomal kernel/reboot.c
echo "end checkout patch"
