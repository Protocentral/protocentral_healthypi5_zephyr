west build -p auto -b healthypi5_rp2040 app  -DEXTRA_CONF_FILE='overlay-bt.conf;overlay-display-st7796.conf;overlay-logger-sd.conf' -DCONFIG_HEALTHYPI_OP_MODE_DISPLAY=n -DEXTRA_DTC_OVERLAY_FILE='healthypi5_rp2040_display_st7796.overlay;healthypi5_rp2040_sd.overlay'

#west build -p always -b healthypi5_rp2040 app -DEXTRA_CONF_FILE='overlay-logger-sd.conf;overlay-bt.conf' -DEXTRA_DTC_OVERLAY_FILE=healthypi5_rp2040_sd.overlay 

#DEXTRA_CONF_FILE=overlay-bt.conf