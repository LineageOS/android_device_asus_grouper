#!/system/bin/sh

# AL3010 Ambient Light Sensor calibration data
cat /per/lightsensor/AL3010_Config.ini > /sys/devices/platform/tegra-i2c.2/i2c-2/2-001c/calibration

# AMI304 Compass calibration data
cat /per/sensors/AMI304_Config.ini > /sys/devices/platform/tegra-i2c.2/i2c-2/2-000e/iio:device1/compass_cali_data
