echo "start patch"
cp drivers/rtc/rtc-hym8563.c_r70 drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c_r70 kernel/reboot.c && \
cp drivers/input/io_control.c_r73 drivers/input/io_control.c && \
echo "end patch"
make ARCH=arm64 r73_defconfig android-11.config  && \
cp logo-0.bmp logo.bmp && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3399-evb-ind-lpddr4-android-avb-r73-edp.img -j24
echo "start checkout patch"
cp drivers/rtc/rtc-hym8563.c-nomal drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c-nomal kernel/reboot.c && \
git checkout drivers/input/io_control.c && \
echo "end checkout patch"
