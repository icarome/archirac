#include "esphome.h"
#include "IRremoteESP8266.h"
#include "IRsend.h"
#include <IRrecv.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
#define AcFanAuto   stdAc::fanspeed_t::kAuto
#define AcCool      stdAc::opmode_t::kCool
#define AcHeat      stdAc::opmode_t::kHeat
#define AcAuto      stdAc::opmode_t::kAuto
#define AcFan       stdAc::opmode_t::kFan
#define AcDry       stdAc::opmode_t::kDry
#define AcFanLow    stdAc::fanspeed_t::kLow
#define AcFanMedium stdAc::fanspeed_t::kMedium
#define AcFanHigh   stdAc::fanspeed_t::kHigh


class IRAC : public Component, public Climate {
  private:
    esphome::sensor::Sensor *sensor_{nullptr};
    String *strKMarca_{nullptr};
    uint16_t kRecvPinholder = -1;
    const uint16_t kCaptureBufferSize = 1024;
    const uint8_t kTimeout = 50;
    const uint16_t kMinUnknownSize = 12;
    decode_type_t kMarca;
    bool receiveIr = false; 
    IRrecv *irrecv;
    decode_results results;
    uint16_t kIrLedholder = -1;
    IRac *ac; 
  public:
    void setup() override {
      if (this->sensor_) {
        this->sensor_->add_on_state_callback([this](float state) {
          this->current_temperature = state;
          this->publish_state();
        });
        this->current_temperature = this->sensor_->state;
      } else {
        this->current_temperature = 18;
      }
      auto restore = this->restore_state_();
      if (restore.has_value()) {
        restore->apply(this);
      } else {
        this->mode = CLIMATE_MODE_OFF;
        this->target_temperature = 18;
      }
      if(this->kRecvPinholder != -1){
        irrecv = new IRrecv(this->kRecvPinholder, kCaptureBufferSize, kTimeout, true);
        irrecv->enableIRIn();
        receiveIr = true;
      }
      ac = new IRac(this->kIrLedholder);
      ac->next.protocol = kMarca;
      ac->next.celsius = true;
      ac->next.degrees = 18;
      ac->next.fanspeed = AcFanAuto;
      ac->next.mode = AcCool;
      this->target_temperature = 18;
      this->current_temperature = 18;
    }
  void set_sensor(esphome::sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_pins(uint16_t kIrLed, uint16_t kRecvPin=-1) { 
    this->kRecvPinholder = kRecvPin;
    this->kIrLedholder = kIrLed;  
  }
  void set_protocol(String *protocol) {
    //kMarca = strToDecodeType(protocol.c_str());
    this->strKMarca_ = protocol;
  }
  void loop() override {
    if(receiveIr){
      if (irrecv->decode(&results)) {
        stdAc::state_t state;
        if (IRAcUtils::decodeToState(&results, &state, nullptr)) {
          String power = IRac::boolToString(state.power);
          esphome::climate::ClimateMode mode = CLIMATE_MODE_OFF;
          switch (state.mode) {
            case stdAc::opmode_t::kOff:
              mode = CLIMATE_MODE_OFF;
              break;
            case stdAc::opmode_t::kAuto:
              mode = CLIMATE_MODE_AUTO;
              break;
            case stdAc::opmode_t::kCool:
              mode = CLIMATE_MODE_COOL;
              break;
            case stdAc::opmode_t::kHeat:
              mode = CLIMATE_MODE_HEAT;
              break;
            case stdAc::opmode_t::kDry:
              mode = CLIMATE_MODE_DRY;
              break;
            case stdAc::opmode_t::kFan:
              mode = CLIMATE_MODE_FAN_ONLY;
              break;
            default:
              mode = CLIMATE_MODE_OFF;
          }
          float target_temp = float(state.degrees);
          if (state.mode == stdAc::opmode_t::kOff || !state.power) {
            String power = IRac::opmodeToString(stdAc::opmode_t::kOff);
            mode = CLIMATE_MODE_OFF;
            target_temp = this->target_temperature;
            // if (!this->sensor_) {
            //   target_temp = this->current_temperature;
            // } 
          }
          esphome::climate::ClimateFanMode fanspeed = CLIMATE_FAN_AUTO;
          switch(state.fanspeed) {
            case stdAc::fanspeed_t::kMax:
              fanspeed = CLIMATE_FAN_HIGH;
              break;
            case stdAc::fanspeed_t::kHigh:
              fanspeed = CLIMATE_FAN_HIGH;
              break;
            case stdAc::fanspeed_t::kMedium:
              fanspeed = CLIMATE_FAN_MEDIUM;
              break;
            case stdAc::fanspeed_t::kLow:
              fanspeed = CLIMATE_FAN_LOW;
              break;
            case stdAc::fanspeed_t::kMin:
              fanspeed = CLIMATE_FAN_LOW;
              break;
            default:
              fanspeed = CLIMATE_FAN_AUTO;
              break;
          }
          // String modelo = String(state.model);
           String marca = typeToString(state.protocol);
          // String debug_fan = IRac::fanspeedToString(state.fanspeed);
          // String debug_modo = IRac::opmodeToString(state.mode);
          // String description = "Marca: "+marca+", Modelo: "+modelo+", Modo: "+debug_modo+", Fan: "+debug_fan;
          //ESP_LOGD("IRReceiver", "Marca: %s, Modelo: %d, Modo: %s, FanSpeed: %s", typeToString(state.protocol),state.model, IRac::opmodeToString(state.mode), IRac::fanspeedToString(state.fanspeed));
          //ESP_LOGD("IRReceiver", marca.c_str());
          if(state.protocol != kMarca){
            this->target_temperature = target_temp;
            if (!this->sensor_) {
              this->current_temperature = target_temp;
            }
            this->mode = mode;
            this->fan_mode = fanspeed;
            this->publish_state();
          }
        }
      }
    }
  }
  climate::ClimateTraits traits() {
    auto traits = climate::ClimateTraits();
    traits.set_supports_current_temperature(true);
    traits.set_supports_two_point_target_temperature(false);
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(30);
    traits.set_visual_temperature_step(1.f);
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_OFF);
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_COOL);
    traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_AUTO);
    traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_LOW);
    traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_MEDIUM);
    traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_HIGH);

    return traits;
  }
  void control(const ClimateCall &call) override {
    ac->next.protocol = strToDecodeType(strKMarca_->c_str());
    if (call.get_mode().has_value()) {
      ClimateMode mode = *call.get_mode();
      switch(mode) {
        case CLIMATE_MODE_HEAT:
          ac->next.power = true;
          ac->next.mode = AcHeat;
          break;
        case CLIMATE_MODE_COOL:
          ac->next.power = true;
          ac->next.mode = AcCool;
          break;
        case CLIMATE_MODE_AUTO:
          ac->next.power = true;
          ac->next.mode = AcAuto;
          break;
        case CLIMATE_MODE_FAN_ONLY: 
          ac->next.power = true;
          ac->next.mode = AcFan;
          break;
        case CLIMATE_MODE_DRY:
          ac->next.power = true;
          ac->next.mode = AcDry;
          break;
        case CLIMATE_MODE_OFF:
          ac->next.power = false;
          break;
        default:
          break;
      }
    this->mode = mode;
    }
    if (call.get_target_temperature().has_value()) {
      float temp = *call.get_target_temperature();
      if(this->mode == CLIMATE_MODE_OFF){
          ac->next.mode = AcCool;
          this->mode = CLIMATE_MODE_COOL;
      }
      ac->next.degrees = temp;
      ac->next.power = true;
      this->target_temperature = temp;
      if (!this->sensor_) {
        this->current_temperature = temp;
      }
    }
    /*if (call.get_away().has_value()) {
      bool awayMode = *call.get_away();
      // Send target temp to climate
      ac->next->setTemp(awayMode);
      this->away = awayMode;
      this->publish_state();
    }*/
    if (call.get_fan_mode().has_value()) {
      ClimateFanMode fanMode = *call.get_fan_mode();
      switch(fanMode) {
        case CLIMATE_FAN_AUTO:
          ac->next.fanspeed = AcFanAuto;
          break;
        case CLIMATE_FAN_LOW:
          ac->next.fanspeed = AcFanLow;
          break;
        case CLIMATE_FAN_MEDIUM:
          ac->next.fanspeed = AcFanMedium;
          break;
        case CLIMATE_FAN_HIGH:
          ac->next.fanspeed = AcFanHigh;
          break;
        default:
          break;
      }
      this->fan_mode = fanMode;
      ac->next.power = true;
    }
    this->publish_state();
    ac->sendAc();
  }
};
