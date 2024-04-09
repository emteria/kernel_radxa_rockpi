make ARCH=arm64 r111_defconfig android-11.config && \
cp logo-180.bmp logo.bmp && \
cp drivers/video/backlight/pwm_bl.c-r111 drivers/video/backlight/pwm_bl.c && \
cp drivers/hid/hid-multitouch.c-r111 drivers/hid/hid-multitouch.c && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3568-evb2-lp4x-rk809-r111-lvds.img -j24 && \
cp drivers/hid/hid-multitouch.c-normal drivers/hid/hid-multitouch.c && \
cp drivers/video/backlight/pwm_bl.c-normal drivers/video/backlight/pwm_bl.c
