make ARCH=arm64 r108_defconfig android-11.config && \
cp logo-270.bmp logo.bmp && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3568-evb2-lp4x-rk809-r108-mipi.img -j24
