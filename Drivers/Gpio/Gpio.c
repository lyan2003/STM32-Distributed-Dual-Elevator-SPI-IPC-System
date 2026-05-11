#include <Std_Types.h>
#include "Gpio.h"
#include "Gpio_Private.h"

uint32 addressMap[5] = {GPIOA_BASE_ADDR, GPIOB_BASE_ADDR, GPIOC_BASE_ADDR, GPIOD_BASE_ADDR, GPIOE_BASE_ADDR};

void Gpio_Init(uint8 PortName, uint8 PinNumber, uint8 PinMode, uint8 DefaultState) {

    uint8 addressIndex = PortName - GPIO_A; /* 'A' - 'A' = 0 */

    GpioType* gpioDevice = (GpioType*) addressMap[addressIndex];

    gpioDevice->GPIO_MODER  &= ~(0x03 << (PinNumber * 2));
    gpioDevice->GPIO_MODER |= (PinMode << (PinNumber * 2));

    if (PinMode == GPIO_INPUT) {
        gpioDevice->GPIO_PUPDR &= ~(0x03 << (PinNumber * 2));
        gpioDevice->GPIO_PUPDR |= (DefaultState << (PinNumber * 2));
    } else {
        gpioDevice->GPIO_OTYPER &= ~(0x1 << PinNumber);
        gpioDevice->GPIO_OTYPER |= (DefaultState << (PinNumber));
    }
}

/* valid only if pin is in output mode */
uint8 Gpio_WritePin(uint8 PortName, uint8 PinNumber, uint8 Data) {
    uint8 status = NOK;
    uint8 addressIndex = PortName - GPIO_A;
    GpioType* gpioDevice = (GpioType*) addressMap[addressIndex];

    uint8 pinMode = (gpioDevice->GPIO_MODER & (0x03 << (PinNumber * 2))) >> (PinNumber * 2);
    if (pinMode != GPIO_INPUT) {
        gpioDevice->GPIO_ODR &= ~(0x1U << PinNumber);
        gpioDevice->GPIO_ODR  |= (Data << PinNumber);
        status = OK;
    }
    return status;
}

uint8 Gpio_ReadPin(uint8 PortName, uint8 PinNum) {
    uint8 data = 0;
    uint8 addressIndex = PortName - GPIO_A;
    GpioType* gpioDevice = (GpioType*) addressMap[addressIndex];

    data = (gpioDevice->GPIO_IDR & (0x1 << PinNum)) >> PinNum;

    return data;
}

void Gpio_ConfigAF(uint8 PortName, uint8 PinNumber, uint8 AF_Value) {
    uint8 addressIndex = PortName - GPIO_A;
    GpioType* gpioDevice = (GpioType*) addressMap[addressIndex];

    if (PinNumber < 8) {
        /* Configure AFRL (Pins 0 to 7) */
        gpioDevice->GPIO_AFRL &= ~(0x0F << (PinNumber * 4));        /* Clear the 4 bits */
        gpioDevice->GPIO_AFRL |=  (AF_Value << (PinNumber * 4));    /* Set the AF value */
    } else {
        /* Configure AFRH (Pins 8 to 15) */
        uint8 offset = PinNumber - 8;
        gpioDevice->GPIO_AFRH &= ~(0x0F << (offset * 4));           /* Clear the 4 bits */
        gpioDevice->GPIO_AFRH |=  (AF_Value << (offset * 4));       /* Set the AF value */
    }
}