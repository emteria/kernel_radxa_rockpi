echo "start patch"
cp drivers/media/i2c/ov5648.c_r70 drivers/media/i2c/ov5648.c && \
cp drivers/rtc/rtc-hym8563.c_r70 drivers/rtc/rtc-hym8563.c && \
cp kernel/reboot.c_r70 kernel/reboot.c && \
cp ../frameworks/base/services/core/java/com/android/server/policy/PhoneWindowManager.java_r70 ../frameworks/base/services/core/java/com/android/server/policy/PhoneWindowManager.java
echo "end patch"
