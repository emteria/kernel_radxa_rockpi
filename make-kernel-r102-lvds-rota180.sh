make ARCH=arm64 r102_defconfig android-11.config && \
cp logo-180.bmp logo.bmp && \
#cp drivers/input/touchscreen/ilitek_limv5_2_0/ilitek_ts.h_215_down drivers/input/touchscreen/ilitek_limv5_2_0/ilitek_ts.h && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3568-evb2-lp4x-rk809-r102-lvds-rota180.img -j24
