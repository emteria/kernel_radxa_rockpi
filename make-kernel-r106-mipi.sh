make ARCH=arm64 r106_defconfig android-11.config && \
cp logo-90.bmp logo.bmp && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3568-evb2-lp4x-rk809-r106-mipi.img -j24
