#include <driver/i2s.h>
#include <math.h>

// ---------------------------------------------------------
// 【完全独立動作版（おすすめ）】
// クロックの共有設定がM5Stick側のシステム（GPIOモードの復帰や内部マトリックスの仕様）により
// 全体を停止させてしまっていたため、「完全に独立した2系統」として動かす一番確実で安全なコードに戻します。
//
// M5StickS3上の空いている「6本のピン」を使って、グループAとグループBを完全に分けて配線してください。
// ---------------------------------------------------------

// ==== グループA (I2S_0) : アンプ1(左) & アンプ2(右) 用 ====
#define I2S_0_BCK   5
#define I2S_0_WS    6
#define I2S_0_DATA  7

// ==== グループB (I2S_1) : アンプ3(左) & アンプ4(右) 用 ====
// ※ピン8や3はESP32-S3の内部状態（Strapping等）と干渉しやすく出力しにくいことがあります。
// M5StickS3のHat側やGrove側にある「4, 1, 2」などに変更してみます。
#define I2S_1_BCK   4    // Hat等のG4
#define I2S_1_WS    1    // Grove等にあるG1
#define I2S_1_DATA  2    // Grove等にあるG2

#define SAMPLE_RATE 44100
#define FREQ_0 50   // グループA 左
#define FREQ_1 50   // グループA 右
#define FREQ_2 50   // グループB 左
#define FREQ_3 50   // グループB 右

// 各チャンネルの位相
static float phase_0 = 0.0;
static float phase_1 = 0.0;
static float phase_2 = 0.0;
static float phase_3 = 0.0;

void setup() {
  Serial.begin(115200);

  // ---------- I2S_0 (グループA) の初期化 : マスター ----------
  i2s_config_t config_0 = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_pin_config_t pin_0 = {
    .bck_io_num = I2S_0_BCK,
    .ws_io_num = I2S_0_WS,
    .data_out_num = I2S_0_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_0, &config_0, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_0);


  // ---------- I2S_1 (グループB) の初期化 : マスター ----------
  i2s_config_t config_1 = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_pin_config_t pin_1 = {
    .bck_io_num = I2S_1_BCK,
    .ws_io_num = I2S_1_WS,
    .data_out_num = I2S_1_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_1, &config_1, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pin_1);

  Serial.println("4 Channels Initialized (Fully Independent Mode).");
}

void loop() {
  const int samples = 128; // バッファ詰まり防止のため小さめに設定
  int16_t buffer0[samples * 2]; // I2S_0 用 (Ch0 & Ch1)
  int16_t buffer1[samples * 2]; // I2S_1 用 (Ch2 & Ch3)

  float inc_0 = (2.0f * PI * FREQ_0) / SAMPLE_RATE;
  float inc_1 = (2.0f * PI * FREQ_1) / SAMPLE_RATE;
  float inc_2 = (2.0f * PI * FREQ_2) / SAMPLE_RATE;
  float inc_3 = (2.0f * PI * FREQ_3) / SAMPLE_RATE;

  for (int iter = 0; iter < (SAMPLE_RATE / samples); iter++) { 
    for (int i = 0; i < samples; i++) {
      // I2S_0 用 (Ch0: 左, Ch1: 右)
      // 動いているか分かりやすいようにグループBと振幅を変えています
      buffer0[2 * i]     = (int16_t)(sin(phase_0) * 8000);   // L (Ch0)
      buffer0[2 * i + 1] = (int16_t)(sin(phase_1) * 8000);   // R (Ch1)
      
      phase_0 += inc_0; if (phase_0 >= 2.0f * PI) phase_0 -= 2.0f * PI;
      phase_1 += inc_1; if (phase_1 >= 2.0f * PI) phase_1 -= 2.0f * PI;

      // I2S_1 用 (Ch2: 左, Ch3: 右)
      buffer1[2 * i]     = (int16_t)(sin(phase_2) * 8000);   // L (Ch2)
      buffer1[2 * i + 1] = (int16_t)(sin(phase_3) * 8000);   // R (Ch3)
      
      phase_2 += inc_2; if (phase_2 >= 2.0f * PI) phase_2 -= 2.0f * PI;
      phase_3 += inc_3; if (phase_3 >= 2.0f * PI) phase_3 -= 2.0f * PI;
    }
    
    size_t bytes_written_0, bytes_written_1;
    
    // 一片がブロックして全体が固まるのを防ぐためタイムアウトを短く設定
    i2s_write(I2S_NUM_0, buffer0, sizeof(buffer0), &bytes_written_0, pdMS_TO_TICKS(10));
    i2s_write(I2S_NUM_1, buffer1, sizeof(buffer1), &bytes_written_1, pdMS_TO_TICKS(10));
  }
}