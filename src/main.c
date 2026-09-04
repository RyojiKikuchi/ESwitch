/*
 * ESwitch
 * 
 * SW押下毎にGP0をLow=>High=>Low=>High=>....と切り替える
 * 
 * for PIC10F200
 * 
 * 2 5 Vdd
 * 5 1 GP0 Relay(Push pull)
 * 4 3 GP1 LED_NC (Nomary Close) (Push pull)
 * 3 4 GP2 LED_NO (Nomary Open) (Push pull)
 * 8 6 GP3 Switch(Pull Up)
 * 7 2 Vss
 */

/* ============================================================
 *  Include
 * ============================================================ */

#include <xc.h>
#include <stdint.h>
#include <stdlib.h>

/* ============================================================
 *  Configuration bits
 * ============================================================ */

// CONFIG
#pragma config WDTE = OFF       // Watchdog Timer (WDT disabled)
#pragma config CP = OFF         // Code Protect (Code protection off)
#pragma config MCLRE = OFF      // Master Clear Enable (GP3/MCLR pin fuction is digital I/O, MCLR internally tied to VDD)

/* ============================================================
 *  Clock Speed
 * ============================================================ */
/* Clock 4MHz */
#define _XTAL_FREQ  4000000UL               // Clock 4MHz

/* ============================================================
 *  Pin Define
 * ============================================================ */

#define RELAY_PIN       GPIObits.GP0        // output
#define LED_NC          GPIObits.GP1        // output
#define LED_NO          GPIObits.GP2        // output
#define SW_PIN          GPIObits.GP3        // input

/* ============================================================
 *  Construction
 * ============================================================ */

/* ボタンのチャタリング防止のため、ボタン押下判定するTMR0値 */
#define BUTTON_PRESS_DETECTION_TMR0 150U    // 1us * 128(PSA) * 150 = 19200us = 19.2ms
/* 連続押下無効期間 */
#define NO_PRESS_PERIOD_MS          500U    // 500ms(0.5sec)

/* ============================================================
 *  Variables
 * ============================================================ */

static __persistent uint8_t sw_state;

/* ============================================================
 *  システム初期化
 * ============================================================ */
static void system_init() {

    /*
     *  OPTION
     *    7:GPWU    = 0:Wake Up Enabled
     *    6:GPPU    = 0:Pull Up Enabled
     *    5:T0CS    = 0:TMR0ソース Focs/4
     *    4:T0SE    = 0:TMR0 Source Edge Low=>High
     *    3:PSA     = 0:TMR0
     *  2-0:PS      = 110 1:128 (128us)
     */
    OPTION = 0b00000110;

    /* 
     * TRIS
     *     3:GP3 = 1:input(SW)
     *     2:GP2 = 0:output(LED_NO)
     *     1:GP1 = 0:output(RELAY)
     *     0:GP0 = 0:output(LED_NC)
     */
    TRISGPIO = 0b00001000;

}

/* ============================================================
 *  ボタンPush/Releaseを待つ
 *  チャタリング判定期間ボタンの状態が維持されるのを待つ
 *   0: Push, 1:Release 
 * ============================================================ */
static void button_wait(uint8_t button_status) {
    TMR0 = 0;
    while (TMR0 < BUTTON_PRESS_DETECTION_TMR0) {
        if (SW_PIN != button_status) {
            TMR0 = 0;
        }
    }
}

/* ============================================================
 *  ボタンの状態をチェック
 *  チャタリング判定期間ボタンの状態が維持され続けたら1を返却
 *  ボタンの状態が変更されたらTMR0をリセットする
 *   0: Push, 1:Release 
 * ============================================================ */
static uint8_t button_check(uint8_t button_status) {
    TMR0 = 0;
    uint8_t current_button = SW_PIN;
    uint8_t prev_button = current_button;
    uint8_t ret = current_button == button_status ? 1 : 0;
    while (TMR0 < BUTTON_PRESS_DETECTION_TMR0) {
        current_button = SW_PIN;
        if (current_button != prev_button) {
            TMR0 = 0;
            prev_button = current_button;
            ret = current_button == button_status ? 1 : 0;
        }
    }
    return ret;
}

/* ============================================================
 *  LEDを点滅させる
 *  終了時には元と同じ状態を維持する
 * ============================================================ */
static void led_blink() {
    for (uint8_t i = 0; i < 10; i++) {
        __delay_ms(100);
        LED_NC = ~LED_NC;
        LED_NO = ~LED_NO;
    }
}

/* ============================================================
 *  gpioの出力ピンの状態をステータスに合わせて設定する
 * ============================================================ */
static void set_gpio() {
    if (sw_state) {
        // ON
        RELAY_PIN = 1;
        LED_NO = 1;
        __delay_ms(100);
        LED_NC = 0;
    } else {
        // OFF
        RELAY_PIN = 0;
        LED_NC = 1;
        __delay_ms(100);
        LED_NO = 0;
    }
}

/* ============================================================
 *  main
 * ============================================================ */
uint8_t main(void) {

    // システム初期化
    system_init();

    // スリープ解除判定
    if (!STATUSbits.GPWUF) {
        // パワーオンリセット

        // GPIO初期化
        GPIO = 0;

        // ステータスをOFF状態に設定
        sw_state = 0;

        // LEDとRELAYの状態を設定
        set_gpio();

        // 念のためボタンリリースwait
        button_wait(1);

        // startup
        led_blink();

        goto go_sleep;

    }

    // スイッチ押下判定
    if (!button_check(0)) {

        // スイッチOFFなら再度スリープ
        goto go_sleep;

    }

    // ステータス切替
    sw_state ^= 0x01;

    // LEDとRELAYの状態を設定
    set_gpio();

    // 連続押下禁止期間wait
    __delay_ms(NO_PRESS_PERIOD_MS);

go_sleep:

    // wait button release
    button_wait(1);

    // sleep前のGPIO読み出し
    (void) GPIO;

    // Sleep
    SLEEP();

    goto go_sleep;
    
}
