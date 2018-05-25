#include "Memory.h"

Memory::Memory() {
    //
}

unsigned long Memory::calculate_crc(uint32_t address) {
    unsigned long crc = ~0L;

    for(int index = 0; index < address + DATA_MAX_LENGTH; ++index){
        crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
        crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
        crc = ~crc;
    }

    return crc;
}

bool Memory::check_eeprom_validity(int ID_type){
    unsigned long current_crc = 0;
    unsigned long stored_crc = 0;

    switch(ID_type){
        case SERIAL_NUM:
            current_crc = calculate_crc(DEVICE_SERIAL_ADDR);
            EEPROM.get(SERIAL_CRC_ADDR, stored_crc);
            break;
        case MODEL_NUM:
            current_crc = calculate_crc(DEVICE_MODEL_ADDR);
            EEPROM.get(MODEL_CRC_ADDR, stored_crc);
            break;
        default:
            break;
    }
    if(current_crc == stored_crc)
        return true;
    return false;
}

void Memory::set_error_flag(int flag){
    error_flag = flag;
}

uint8_t Memory::error() {
    uint8_t return_val = error_flag;
    error_flag = 0;
    return return_val;
}

uint32_t Memory::id_type_to_address(int ID_type) {
    switch(ID_type){
        case SERIAL_NUM:
            return DEVICE_SERIAL_ADDR;
        case MODEL_NUM:
            return DEVICE_MODEL_ADDR;
    }
}

void Memory::read_from_eeprom(int ID_type){
    if (ID_type == SERIAL_NUM) serial = "";
    else model = "";
    if(!check_eeprom_validity(ID_type)){
        set_error_flag(ERR_FLAG_EEPROM_INVALID);
        if (ID_type == SERIAL_NUM)
            serial = "none";
        else model = "none";
        return;
    }

    uint32_t address = id_type_to_address(ID_type);
    char eeprom_data[DATA_MAX_LENGTH] = {'\0'};
    for(byte i = 0; i < DATA_MAX_LENGTH; ++i){
        EEPROM.get(address + i, eeprom_data[i]);
        if(eeprom_data[i] == '\0') {
            break;
        }
        if (ID_type == SERIAL_NUM)
            serial += char(eeprom_data[i]);
        else model += char(eeprom_data[i]);
    }
}

void Memory::write_to_eeprom(uint32_t ID_type){
    uint32_t address = id_type_to_address(ID_type);
    if(writeData.length() == 0 || writeData.length() > DATA_MAX_LENGTH){
        set_error_flag(ERR_FLAG_DATA_TO_LONG);
        return;
    }
    for(uint8_t i = 0; i < DATA_MAX_LENGTH; ++i){
        if(i >= writeData.length())
            EEPROM.put(address + i, '\0');
        else
            EEPROM.put(address + i, writeData.charAt(i));
    }
}

void Memory::update_serial_crc(){
    unsigned long current_crc = calculate_crc(DEVICE_SERIAL_ADDR);
    EEPROM.put(SERIAL_CRC_ADDR, current_crc);
}

void Memory::update_model_crc(){
    unsigned long current_crc = calculate_crc(DEVICE_MODEL_ADDR);
    EEPROM.put(MODEL_CRC_ADDR, current_crc);
}

void Memory::erase_serial_data() {
    // Clear EEPROM
    for(int i = DEVICE_SERIAL_ADDR; i < DEVICE_SERIAL_ADDR + DATA_MAX_LENGTH; i++)
    EEPROM.write(i,0);
}

void Memory::erase_model_data() {
    // Clear EEPROM
    for(int i = DEVICE_MODEL_ADDR; i < DEVICE_MODEL_ADDR + DATA_MAX_LENGTH; i++)
    EEPROM.write(i,0);
}

void Memory::write_serial() {
    write_to_eeprom(SERIAL_NUM);
    update_serial_crc();
}

void Memory::write_model() {
    write_to_eeprom(MODEL_NUM);
    update_model_crc();
}

void Memory::read_serial() {
    read_from_eeprom(SERIAL_NUM);
}

void Memory::read_model() {
    read_from_eeprom(MODEL_NUM);
}
