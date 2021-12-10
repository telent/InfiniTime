#include "displayapp/DisplayAppRecovery.h"
#include <FreeRTOS.h>
#include <task.h>
#include <libraries/log/nrf_log.h>
#include "touchhandler/TouchHandler.h"
#include "components/ble/BleController.h"
#include "lua_state.h"

using namespace Pinetime::Applications;

DisplayApp::DisplayApp(Drivers::St7789& lcd,
                       Components::LittleVgl& lvgl,
                       Drivers::Cst816S& touchPanel,
                       Controllers::Battery& batteryController,
                       Controllers::Ble& bleController,
                       Controllers::DateTime& dateTimeController,
                       Drivers::WatchdogView& watchdog,
                       Pinetime::Controllers::NotificationManager& notificationManager,
                       Pinetime::Controllers::HeartRateController& heartRateController,
                       Controllers::Settings& settingsController,
                       Pinetime::Controllers::MotorController& motorController,
                       Pinetime::Controllers::MotionController& motionController,
                       Pinetime::Controllers::TimerController& timerController,
                       Pinetime::Controllers::AlarmController& alarmController,
                       Pinetime::Controllers::TouchHandler& touchHandler)
  : lcd {lcd}, bleController {bleController} {

}

void DisplayApp::Start() {
  msgQueue = xQueueCreate(queueSize, itemSize);
  if (pdPASS != xTaskCreate(DisplayApp::Process, "displayapp", 512, this, 0, &taskHandle))
    APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
}

static Pinetime::Drivers::St7789 * private_lua_lcd;

void DisplayApp::Process(void* instance) {
  auto* app = static_cast<DisplayApp*>(instance);
  NRF_LOG_INFO("displayapp task started!");

  // Send a dummy notification to unlock the lvgl display driver for the first iteration
  xTaskNotifyGive(xTaskGetCurrentTaskHandle());

  app->InitHw();

  open_lua();
  private_lua_lcd = &(app->lcd);

  app->clear_screen();
  lua_say_hello();

  while (true) {
    app->Refresh();
  }
}


extern uint16_t lcd_buffer[];

extern "C" void draw_lcd_buffer(int x, int y, int w, int h, int length);

void draw_lcd_buffer(int x, int y, int w, int h, int length){
    ulTaskNotifyTake(pdTRUE, 500);
    private_lua_lcd->DrawBuffer(x, y, w, h,
				reinterpret_cast<const uint8_t*>(lcd_buffer),
				length);
}

void DisplayApp::InitHw() {
  brightnessController.Init();
  brightnessController.Set(Controllers::BrightnessController::Levels::High);
}

void DisplayApp::Refresh() {
  Display::Messages msg;
  if (xQueueReceive(msgQueue, &msg, 200)) {
    switch (msg) {
      case Display::Messages::UpdateBleConnection:
        if (bleController.IsConnected()) {
          DisplayLogo(colorBlue);
        } else {
          DisplayLogo(colorWhite);
        }
        break;
      case Display::Messages::BleFirmwareUpdateStarted:
        DisplayLogo(colorGreen);
        break;
      default:
        break;
    }
  }

  if (bleController.IsFirmwareUpdating()) {
    uint8_t percent =
      (static_cast<float>(bleController.FirmwareUpdateCurrentBytes()) / static_cast<float>(bleController.FirmwareUpdateTotalBytes())) *
      100.0f;
    switch (bleController.State()) {
      case Controllers::Ble::FirmwareUpdateStates::Running:
        DisplayOtaProgress(percent, colorWhite);
        break;
      case Controllers::Ble::FirmwareUpdateStates::Validated:
        DisplayOtaProgress(100, colorGreenSwapped);
        break;
      case Controllers::Ble::FirmwareUpdateStates::Error:
        DisplayOtaProgress(100, colorRedSwapped);
        break;
      default:
        break;
    }
  }
}

static uint16_t picture[] = {
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,

  0x0000, 0x0000, 0x0000, 0x0ff0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0ff0, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0ff0, 0x00ff, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0ff0, 0x00ff, 0x00ff, 0x0000, 0x0000,
};

static uint8_t buffer[240 * 2];

void DisplayApp::clear_screen() {
  memset(buffer, 0, 240 * 2);
  for(int y = 0;y < 240; y++) {
    ulTaskNotifyTake(pdTRUE, 500);
    lcd.DrawBuffer(0, y, 240, 1, buffer, 240 * 2);
  }
}


void DisplayApp::DisplayLogo(uint16_t color) {

  const uint8_t * data = reinterpret_cast<const uint8_t*>(picture);
  for(int x = 10; x < 200;x += 12) {
    ulTaskNotifyTake(pdTRUE, 500); // we do this before each drawBuffer call?

    lcd.DrawBuffer(x, x, 8, 8, data, 2* sizeof picture);
  }
}

void DisplayApp::DisplayOtaProgress(uint8_t percent, uint16_t color) {
  const uint8_t barHeight = 20;
  std::fill(displayBuffer, displayBuffer + (displayWidth * bytesPerPixel), color);
  for (int i = 0; i < barHeight; i++) {
    ulTaskNotifyTake(pdTRUE, 500);
    uint16_t barWidth = std::min(static_cast<float>(percent) * 2.4f, static_cast<float>(displayWidth));
    lcd.DrawBuffer(0, displayWidth - barHeight + i, barWidth, 1, reinterpret_cast<const uint8_t*>(displayBuffer), barWidth * bytesPerPixel);
  }
}

void DisplayApp::PushMessage(Display::Messages msg) {
  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(msgQueue, &msg, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    /* Actual macro used here is port specific. */
    // TODO : should I do something here?
  }
}

void DisplayApp::Register(Pinetime::System::SystemTask* systemTask) {

}
