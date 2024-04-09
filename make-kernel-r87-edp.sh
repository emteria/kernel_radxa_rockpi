make ARCH=arm64 r87_defconfig android-11.config && \
cp logo-0.bmp logo.bmp && \
make ARCH=arm64 BOOT_IMG=../rockdev/Image-$TARGET_PRODUCT/boot.img rk3566-rk817-lp4x-r87-edp.img -j24
