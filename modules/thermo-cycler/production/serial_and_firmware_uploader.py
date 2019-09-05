# Script that will:
# - Upload eeprom writer code onto TC
# - Write Serial number from scanner onto TC
# - Verify written Serial number
# - Upload production TC firmware


# 1. Upload firmware:
# - (Optional) Convert bin to uf2 -> import uf2conv.py and use
# - Open and close serial at 1200 baud
# - Search /Volumes for TCBOOT
# - Once found, copy uf2 file to TCBOOT
