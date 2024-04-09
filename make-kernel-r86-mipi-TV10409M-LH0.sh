echo "start patch"
cp drivers/media/i2c/ov5648.c_r70 drivers/media/i2c/ov5648.c && \
cp drivers/rtc/rtc-hym8563.c_r70 drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c_r70 kernel/reboot.c && \
echo "end patch"
make ARCH=arm r86_defconfig android-11.config && \
cp logo-270.bmp logo.bmp && \
make ARCH=arm BOOT_IMG=../rockdev/Image-rk3288_Android11/boot.img rk3288-r86-android-rk818-mipi-avb-TV10409M-LH0.img -j24
echo "start checkout patch"
cp drivers/media/i2c/ov5648.c-nomal drivers/media/i2c/ov5648.c && \
cp drivers/rtc/rtc-hym8563.c-nomal drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c-nomal kernel/reboot.c
echo "end checkout patch"