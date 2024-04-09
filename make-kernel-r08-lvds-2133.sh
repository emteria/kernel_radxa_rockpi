echo "start patch"
cd kernel
cp drivers/media/i2c/ov5648.c_r70 drivers/media/i2c/ov5648.c && \
cp drivers/rtc/rtc-hym8563.c_r70 drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c_r70 kernel/reboot.c && \
echo "end patch"
make ARCH=arm64 r08_defconfig android-11.config && \
cp logo-180.bmp logo.bmp && \
cp drivers/input/touchscreen/ilitek_limv5_2_0/ilitek_ts.h_2133 drivers/input/touchscreen/ilitek_limv5_2_0/ilitek_ts.h && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3399-evb-ind-lpddr4-android-avb-r08-lvds-2133.img -j24 && \
echo "start checkout patch"
cp drivers/input/touchscreen/ilitek_limv5_2_0/ilitek_ts.h_156_173 drivers/input/touchscreen/ilitek_limv5_2_0/ilitek_ts.h && \
cp drivers/media/i2c/ov5648.c-nomal drivers/media/i2c/ov5648.c && \
cp drivers/rtc/rtc-hym8563.c-nomal drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c-nomal kernel/reboot.c && \
echo "end checkout patch"
cd -
