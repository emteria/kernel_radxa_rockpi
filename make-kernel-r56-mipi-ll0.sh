echo "start patch"
cp drivers/rtc/rtc-hym8563.c_r70 drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c_r70 kernel/reboot.c && \
cp drivers/media/i2c/gc2155.c_r56 drivers/media/i2c/gc2155.c && \
cp drivers/video/backlight/pwm_bl.c_r56 drivers/video/backlight/pwm_bl.c && \
echo "end patch"
make ARCH=arm r56_defconfig android-11.config && \
cp logo-270.bmp logo.bmp && \
make ARCH=arm BOOT_IMG=../rockdev/Image-rk3288_Android11_r56/boot.img rk3288-evb-android-act8846-mipi-r56-ll0.img -j32
echo "start checkout patch"
cp drivers/rtc/rtc-hym8563.c-nomal drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c-nomal kernel/reboot.c && \
git checkout drivers/media/i2c/gc2155.c && \
cp drivers/video/backlight/pwm_bl.c-normal drivers/video/backlight/pwm_bl.c && \
echo "end checkout patch"
