hexmerge.py -o build/merged_boot.hex --no-start-addr build/mcuboot/zephyr/zephyr.hex build/app/zephyr/zephyr.signed.hex
hex2bin.py build/merged_boot.hex build/merged_boot.bin
../zephyr/scripts/build/uf2conv.py build/merged_boot.bin -f 0xe48bff56 -b 0x10000000 -o build/merged_boot.uf2 
