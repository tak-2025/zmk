# Copyright (c) 2020-2023 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_library_amend()

# kscan_gpio_matrix.c takes its debounce config through the torabo timing seam
# (<zmk/torabo_timing.h>), which lives with the rest of the zmk app headers.
# Those are PRIVATE to the `app` target, and this driver is compiled into a
# zephyr_library, so add the directory for this library only — no global include
# path is introduced.
# (this dir is <zmk>/app/module/drivers/kscan, so ../../../include is <zmk>/app/include)
zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

zephyr_library_sources_ifdef(CONFIG_ZMK_KSCAN_GPIO_DRIVER kscan_gpio.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_KSCAN_GPIO_MATRIX kscan_gpio_matrix.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_KSCAN_GPIO_CHARLIEPLEX kscan_gpio_charlieplex.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_KSCAN_GPIO_DIRECT kscan_gpio_direct.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_KSCAN_GPIO_DEMUX kscan_gpio_demux.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_KSCAN_MOCK_DRIVER kscan_mock.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_KSCAN_COMPOSITE_DRIVER kscan_composite.c)
