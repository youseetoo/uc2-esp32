#pragma once
#include "esp_err.h"

//https://github.com/Gordon01/esp32-tca9535/


typedef enum {
	TCA9535_INPUT_REG0 =	0x00,		/*!< Input status register */
	TCA9535_INPUT_REG1 =	0x01,		/*!< Input status register */
	TCA9535_OUTPUT_REG0	=	0x02,		/*!< Output register to change state of output BIT set to 1, output set HIGH */
	TCA9535_OUTPUT_REG1	=	0x03,		/*!< Output register to change state of output BIT set to 1, output set HIGH */
	TCA9535_POLARITY_REG0 =	0x04,		/*!< Polarity inversion register. BIT '1' inverts input polarity of register 0x00 */
	TCA9535_POLARITY_REG1 =	0x05,		/*!< Polarity inversion register. BIT '1' inverts input polarity of register 0x00 */
	TCA9535_CONFIG_REG0	=	0x06,		/*!< Configuration register. BIT = '1' sets port to input BIT = '0' sets port to output */
	TCA9535_CONFIG_REG1	=	0x07		/*!< Configuration register. BIT = '1' sets port to input BIT = '0' sets port to output */
} tca9535_reg_t;

    struct TCA9535_sBit{
    uint8_t Bit0:1;
    uint8_t Bit1:1;
    uint8_t Bit2:1;
    uint8_t Bit3:1;
    uint8_t Bit4:1;
    uint8_t Bit5:1;
    uint8_t Bit6:1;
    uint8_t Bit7:1;
    };




union TCA9535_uInputPort{
	uint8_t asInt;						/*!< Port data as unsigned integer */
	struct TCA9535_sBit bit;			/*!< Port data as separate bits */
};

struct TCA9535_sRegister{
	union TCA9535_uInputPort P0;
	union TCA9535_uInputPort P1;
};

typedef union {
	uint16_t asInt;						/*!< Register data as unsigned integer */
	struct TCA9535_sRegister Port;		/*!< Register data as separate ports */
} TCA9535_Register;

esp_err_t TCA9535Init(uint8_t I2C_MASTER_SCL_IO, uint8_t I2C_MASTER_SDA_IO,uint8_t TCA9535_ADDRESS);
unsigned char TCA9535ReadSingleRegister(tca9535_reg_t address);
esp_err_t TCA9535WriteSingleRegister(tca9535_reg_t address, unsigned short regVal);


esp_err_t TCA9535WriteOutput(TCA9535_Register *reg);
esp_err_t TCA9535WritePolarity(TCA9535_Register *reg);
esp_err_t TCA9535WriteConfig(TCA9535_Register *reg);

esp_err_t TCA9535ReadInput(TCA9535_Register *reg);
esp_err_t TCA9535ReadOutput(TCA9535_Register *reg);
esp_err_t TCA9535ReadPolarity(TCA9535_Register *reg);
esp_err_t TCA9535ReadConfig(TCA9535_Register *reg);
