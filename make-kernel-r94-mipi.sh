make ARCH=arm64 r94_defconfig android-11.config && \
cp logo-90.bmp logo.bmp && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3566-rk809-lp4x-r94-mipi.img -j24
