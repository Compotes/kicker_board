#include "ch.h"
#include "hal.h"

#include <icu_lld.h>
#include <inttypes.h>
#include "chprintf.h"

#define KICKER_LOADED_VALUE 1500
#define KICKER_LOADING_SPEED 160
#define VOLTAGE_SENSOR_ANALOG_DRIVER &ADCD1

#define KICKER_PINS_GPIO GPIOA

#define LED_PIN 4
#define PHYSICAL_KICK_PIN 6
#define MASTER_READY_PIN 9
#define MASTER_KICK_PIN 10

#define KICKER_CONTROLL_PWM_DRIVER &PWMD3
#define KICKER_PWM_CONTROLL_PINS_GPIO GPIOB
#define PWM_CONTROLL_PIN 1

static const ADCConversionGroup loading_voltage_sensor = {
    FALSE,
    1,
    NULL,
    NULL,
    ADC_CFGR1_RES_12BIT,
    ADC_TR(0, 0),
    ADC_SMPR_SMP_1P5,
    ADC_CHSELR_CHSEL5
};

int16_t voltage_sensor_analog_read(void) {
    adcsample_t  sample;
    adcConvert(VOLTAGE_SENSOR_ANALOG_DRIVER, &loading_voltage_sensor, &sample, 1);
    return (int16_t)sample;
} 

void i_am_ready_to_kick(void) {
    palSetPad(KICKER_PINS_GPIO, MASTER_READY_PIN);
}

void i_am_not_ready_to_kick(void) {
    palClearPad(KICKER_PINS_GPIO, MASTER_READY_PIN);
}

THD_WORKING_AREA(waKickerThread, 64);
THD_FUNCTION(KickerThread, arg) {
    (void)arg;

    palClearPad(GPIOA, 4);
    while (1) {
        if(voltage_sensor_analog_read() >= KICKER_LOADED_VALUE) {
            i_am_ready_to_kick();
            pwmEnableChannel(&PWMD3, 3, 0);
        } else {
            i_am_not_ready_to_kick();
            pwmEnableChannel(&PWMD3, 3, KICKER_LOADING_SPEED);
        }
        
       chThdSleepMilliseconds(10);
    }
    chRegSetThreadName("kicker");
}

static PWMConfig kicker_loading_controll_pwmcfg = {
    2000000,
    200, 
    NULL, /* No callback */
    {
        {PWM_OUTPUT_ACTIVE_HIGH, NULL},
        {PWM_OUTPUT_ACTIVE_HIGH, NULL},
        {PWM_OUTPUT_ACTIVE_HIGH, NULL},
        {PWM_OUTPUT_ACTIVE_HIGH, NULL}
    }, 
    0,
    0
};

void led(uint8_t state) {
    if(state) {
    	palSetPad(KICKER_PINS_GPIO, LED_PIN);
    } else {
    	palClearPad(KICKER_PINS_GPIO, LED_PIN);
    }
}

void init(void) {
    halInit();
    chSysInit();

    /*
     *	ADC driver  setup
     */

    adcSTM32SetCCR(ADC_CCR_VBATEN | ADC_CCR_TSEN | ADC_CCR_VREFEN);
    adcStart(VOLTAGE_SENSOR_ANALOG_DRIVER, NULL);
    
    /*
     * PWM driver setup
     */

    pwmInit();
    pwmStart(KICKER_CONTROLL_PWM_DRIVER, &kicker_loading_controll_pwmcfg);

    /*
     * Pins setup
     */

    // PWM pin
    palSetPadMode(KICKER_PWM_CONTROLL_PINS_GPIO, PWM_CONTROLL_PIN, PAL_MODE_ALTERNATE(1));
    
    // Master communication pins
    palSetPadMode(KICKER_PINS_GPIO, MASTER_READY_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(KICKER_PINS_GPIO, MASTER_KICK_PIN, PAL_MODE_INPUT_PULLDOWN);

    // Physical kick
    palSetPadMode(KICKER_PINS_GPIO, PHYSICAL_KICK_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    
    // LED pin
    palSetPadMode(KICKER_PINS_GPIO, LED_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    
    /*
     * default features configuration
     */

    led(0);
    i_am_not_ready_to_kick();   
    palClearPad(KICKER_PINS_GPIO, PHYSICAL_KICK_PIN);

    /*
     * Thread setup
     */

    chThdCreateStatic(waKickerThread, sizeof(waKickerThread), NORMALPRIO, KickerThread, NULL);
}

void kick(void) {
    palSetPad(KICKER_PINS_GPIO, LED_PIN);
    palSetPad(KICKER_PINS_GPIO, PHYSICAL_KICK_PIN);
    chThdSleepMilliseconds(500);
    palClearPad(KICKER_PINS_GPIO, LED_PIN);
    palClearPad(KICKER_PINS_GPIO, PHYSICAL_KICK_PIN);    
}

int main(void) {
    
    init();

    while (true) {
        if(palReadPad(KICKER_PINS_GPIO, MASTER_KICK_PIN)) {
            kick();
        }

        chThdSleepMilliseconds(10);
    }
}
