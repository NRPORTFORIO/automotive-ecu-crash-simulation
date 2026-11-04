#include <stdio.h>
#include <stdbool.h>

// --- 定数定義 ---
#define NOISE_FILTER_COUNT 3   // 誤作動を防ぐための連続検知回数（デバウンス）
#define CRASH_THRESHOLD_G 4.5f // 衝突と判定する加速度(G)のしきい値
#define SENSOR_ERROR_VAL -99.0f// センサー故障時の異常値（バリデーション用）

// --- 車載ECUのステータス構造体 ---
typedef struct {
    float current_g;          // 現在計測中の加速度(G)
    bool is_sensor_fault;     // センサー故障フラグ
    bool airbag_deployed;     // エアバッグ展開フラグ
    bool fuel_pump_stopped;   // 燃料ポンプ停止フラグ（火災防止）
} VehicleEcuStatus;

// --- 関数のプロトタイプ宣言 ---
void initialize_ecu(VehicleEcuStatus *ecu);
void process_sensor_input(VehicleEcuStatus *ecu, float raw_g_input, int *detect_counter);
void print_ecu_log(int timestamp_ms, const VehicleEcuStatus *ecu);

int main(void) {
    VehicleEcuStatus my_ecu;
    initialize_ecu(&my_ecu);
    
    int noise_counter = 0;
    
    // 衝突実験時（100ミリ秒間）の疑似センサーデータ（タイムスタンプ, 入力G）
    // 段差ノイズ、センサー故障、そして本衝突のケースをシミュレート
    float test_data[10] = {
        1.2f,  // 10ms: 通常走行
        4.8f,  // 20ms: 路面のガタつき（1回目のノイズ：まだ作動させない）
        1.1f,  // 30ms: 通常に復帰（カウンターリセット）
        SENSOR_ERROR_VAL, // 40ms: センサー異常値混入（バリデーション確認）
        5.2f,  // 50ms: 本衝突開始（1回目検知）
        5.5f,  // 60ms: 本衝突継続（2回目検知）
        5.8f,  // 70ms: 本衝突継続（3回目検知 -> ここで安全装置トリガー）
        6.0f,  // 80ms: 衝突中
        3.0f,  // 90ms: 減速中
        0.5f   // 100ms: 停止
    };

    printf("--- 衝突安全評価用：ECUログシミュレーション開始 ---\n");
    
    for(int i = 0; i < 10; i++) {
        int timestamp_ms = (i + 1) * 10;
        
        // センサー入力処理と安全制御ロジックの実行
        process_sensor_input(&my_ecu, test_data[i], &noise_counter);
        
        // 評価用ログの出力
        print_ecu_log(timestamp_ms, &my_ecu);
    }
    
    return 0;
}

// ECUの初期化
void initialize_ecu(VehicleEcuStatus *ecu) {
    if (ecu == NULL) return;
    ecu->current_g = 0.0f;
    ecu->is_sensor_fault = false;
    ecu->airbag_deployed = false;
    ecu->fuel_pump_stopped = false;
}

// センサー入力処理（バリデーション ＆ デバウンス ＆ フェイルセーフ）
void process_sensor_input(VehicleEcuStatus *ecu, float raw_g_input, int *detect_counter) {
    if (ecu == NULL || detect_counter == NULL) return;

    // 1. 入力値のバリデーション（異常値チェック）
    if (raw_g_input <= SENSOR_ERROR_VAL) {
        ecu->is_sensor_fault = true;
        // センサー故障時は二次災害を防ぐため、安全策として燃料ポンプを停止（フェイルセーフ）
        ecu->fuel_pump_stopped = true;
        return;
    }
    
    ecu->current_g = raw_g_input;

    // すでに展開済みの場合は制御をスキップ（ラッチ回路のシミュレート）
    if (ecu->airbag_deployed) return;

    // 2. デバウンス処理（誤作動フィルター）とフェイルセーフ起動
    if (raw_g_input >= CRASH_THRESHOLD_G) {
        (*detect_counter)++;
        
        // 規定回数連続でしきい値を超えた場合、本物の「衝突」と客観的に判定
        if (*detect_counter >= NOISE_FILTER_COUNT) {
            ecu->airbag_deployed = true;
            ecu->fuel_pump_stopped = true; // 火災防止のため燃料カット
        }
    } else {
        *detect_counter = 0; // 連続が途切れたらノイズとみなしてクリア
    }
}

// 性能評価用のフォーマット出力
void print_ecu_log(int timestamp_ms, const VehicleEcuStatus *ecu) {
    if (ecu == NULL) return;
    printf("[%3dms] G-Force:%4.1f | Fault:%s | Airbag:%s | FuelPump:%s\n",
           timestamp_ms,
           ecu->current_g,
           ecu->is_sensor_fault ? "YES" : "NO ",
           ecu->airbag_deployed ? "DEPLOYED" : "READY   ",
           ecu->fuel_pump_stopped ? "STOPPED" : "DRIVE  ");
}
