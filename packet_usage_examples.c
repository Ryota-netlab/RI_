/*
 * Packet-Level FIB Control Usage Examples
 * パケットレベルFIB制御の具体的使用例
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// #include "packet_level_fib_control.h"

/****************************************************************************************
 具体的な使用例
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
    例1: 時間帯によるパケット制御
    - 平日の9:00-17:00のみInterestを転送
    - それ以外は Interest Return
----------------------------------------------------------------------------------------*/
void
example_time_based_control (CefT_Packet_Control_Context* ctx) {
    CefT_Packet_Control_Rule* rule;
    
    rule = (CefT_Packet_Control_Rule*) malloc(sizeof(CefT_Packet_Control_Rule));
    memset(rule, 0, sizeof(CefT_Packet_Control_Rule));
    
    /* 平日営業時間のルール */
    rule->condition_types = CefC_Condition_Type_Time;
    rule->action = CefC_Packet_Action_Forward;
    rule->priority = 100;
    rule->enabled = 1;
    
    /* 時間条件設定 */
    rule->time_cond.type = CefC_Time_Condition_Schedule;
    rule->time_cond.weekdays = 0x3E;  // 月-金 (bit1-5)
    rule->time_cond.start_hour = 9;
    rule->time_cond.end_hour = 17;
    
    cef_packet_control_rule_add(ctx, rule);
    
    /* デフォルトアクション: Interest Return */
    ctx->default_action = CefC_Packet_Action_Return;
    
    printf("時間帯制御ルールを追加しました（平日9-17時のみ転送）\n");
}

/*--------------------------------------------------------------------------------------
    例2: コンテンツ名による選択的制御
    - /video/* パスはチャンク番号0-99のみ転送
    - /emergency/* パスは常時転送（最高優先度）
    - その他は通常転送
----------------------------------------------------------------------------------------*/
void
example_content_based_control (CefT_Packet_Control_Context* ctx) {
    CefT_Packet_Control_Rule* rule1;
    CefT_Packet_Control_Rule* rule2;
    
    /* 緊急コンテンツの最優先ルール */
    rule1 = (CefT_Packet_Control_Rule*) malloc(sizeof(CefT_Packet_Control_Rule));
    memset(rule1, 0, sizeof(CefT_Packet_Control_Rule));
    
    rule1->condition_types = CefC_Condition_Type_Name;
    rule1->action = CefC_Packet_Action_Forward;
    rule1->priority = 255;  // 最高優先度
    rule1->enabled = 1;
    
    /* 緊急コンテンツ名パターン */
    char* emergency_pattern = "/emergency/";
    rule1->name_cond.name_pattern = (unsigned char*) malloc(strlen(emergency_pattern));
    memcpy(rule1->name_cond.name_pattern, emergency_pattern, strlen(emergency_pattern));
    rule1->name_cond.pattern_len = strlen(emergency_pattern);
    rule1->name_cond.match_type = 0x02;  // 前方一致
    
    cef_packet_control_rule_add(ctx, rule1);
    
    /* ビデオコンテンツの制限ルール */
    rule2 = (CefT_Packet_Control_Rule*) malloc(sizeof(CefT_Packet_Control_Rule));
    memset(rule2, 0, sizeof(CefT_Packet_Control_Rule));
    
    rule2->condition_types = CefC_Condition_Type_Name | CefC_Condition_Type_ChunkNum;
    rule2->action = CefC_Packet_Action_Forward;
    rule2->priority = 50;
    rule2->enabled = 1;
    
    /* ビデオコンテンツ名パターン */
    char* video_pattern = "/video/";
    rule2->name_cond.name_pattern = (unsigned char*) malloc(strlen(video_pattern));
    memcpy(rule2->name_cond.name_pattern, video_pattern, strlen(video_pattern));
    rule2->name_cond.pattern_len = strlen(video_pattern);
    rule2->name_cond.match_type = 0x02;  // 前方一致
    
    /* チャンク番号制限 */
    rule2->chunk_cond.min_chunk = 0;
    rule2->chunk_cond.max_chunk = 99;  // 最初の100チャンクのみ
    
    cef_packet_control_rule_add(ctx, rule2);
    
    printf("コンテンツベース制御ルールを追加しました\n");
    printf("- /emergency/* : 常時転送\n");
    printf("- /video/* : チャンク0-99のみ転送\n");
}

/*--------------------------------------------------------------------------------------
    例3: 送信元によるアクセス制御
    - 特定のFace-IDからのリクエストを制限
    - 信頼できるソースからのリクエストは優先転送
----------------------------------------------------------------------------------------*/
void
example_source_based_control (CefT_Packet_Control_Context* ctx) {
    CefT_Packet_Control_Rule* rule1;
    CefT_Packet_Control_Rule* rule2;
    
    /* 信頼できるソースの優先ルール */
    rule1 = (CefT_Packet_Control_Rule*) malloc(sizeof(CefT_Packet_Control_Rule));
    memset(rule1, 0, sizeof(CefT_Packet_Control_Rule));
    
    rule1->condition_types = CefC_Condition_Type_Source;
    rule1->action = CefC_Packet_Action_Forward;
    rule1->priority = 200;
    rule1->enabled = 1;
    
    /* 信頼できるFace-ID */
    rule1->source_cond.faceid = 1;  // Face-ID 1は信頼できるソース
    
    cef_packet_control_rule_add(ctx, rule1);
    
    /* 制限されたソースのブロックルール */
    rule2 = (CefT_Packet_Control_Rule*) malloc(sizeof(CefT_Packet_Control_Rule));
    memset(rule2, 0, sizeof(CefT_Packet_Control_Rule));
    
    rule2->condition_types = CefC_Condition_Type_Source;
    rule2->action = CefC_Packet_Action_Drop;
    rule2->priority = 10;
    rule2->enabled = 1;
    
    /* 制限されたFace-ID */
    rule2->source_cond.faceid = 99;  // Face-ID 99は制限対象
    
    cef_packet_control_rule_add(ctx, rule2);
    
    printf("送信元ベース制御ルールを追加しました\n");
    printf("- Face-ID 1: 優先転送\n");
    printf("- Face-ID 99: パケットドロップ\n");
}

/*--------------------------------------------------------------------------------------
    例4: カスタム条件による高度な制御
    - パケットサイズによる制御
    - 特定のTLVの存在チェック
----------------------------------------------------------------------------------------*/
int
custom_packet_size_check (
    const unsigned char* msg,
    uint16_t payload_len,
    uint16_t header_len,
    void* user_data
) {
    uint16_t total_size = payload_len + header_len;
    uint16_t max_size = *((uint16_t*)user_data);
    
    /* パケットサイズが上限以下の場合のみ転送 */
    return (total_size <= max_size);
}

void
example_custom_control (CefT_Packet_Control_Context* ctx) {
    CefT_Packet_Control_Rule* rule;
    uint16_t* max_packet_size;
    
    rule = (CefT_Packet_Control_Rule*) malloc(sizeof(CefT_Packet_Control_Rule));
    memset(rule, 0, sizeof(CefT_Packet_Control_Rule));
    
    rule->condition_types = CefC_Condition_Type_Custom;
    rule->action = CefC_Packet_Action_Forward;
    rule->priority = 75;
    rule->enabled = 1;
    
    /* カスタム条件: パケットサイズ制限 */
    max_packet_size = (uint16_t*) malloc(sizeof(uint16_t));
    *max_packet_size = 1024;  // 1KBまで
    
    rule->custom_cond.func = custom_packet_size_check;
    rule->custom_cond.user_data = max_packet_size;
    
    cef_packet_control_rule_add(ctx, rule);
    
    /* サイズオーバーのパケットはドロップ */
    ctx->default_action = CefC_Packet_Action_Drop;
    
    printf("カスタム制御ルールを追加しました（1KB以下のパケットのみ転送）\n");
}

/*--------------------------------------------------------------------------------------
    例5: 複合条件による高度な制御
    - 特定時間帯 AND 特定コンテンツ AND 特定送信元
----------------------------------------------------------------------------------------*/
void
example_complex_control (CefT_Packet_Control_Context* ctx) {
    CefT_Packet_Control_Rule* rule;
    
    rule = (CefT_Packet_Control_Rule*) malloc(sizeof(CefT_Packet_Control_Rule));
    memset(rule, 0, sizeof(CefT_Packet_Control_Rule));
    
    /* 複数条件の組み合わせ */
    rule->condition_types = CefC_Condition_Type_Time | 
                           CefC_Condition_Type_Name | 
                           CefC_Condition_Type_Source;
    rule->action = CefC_Packet_Action_Forward;
    rule->priority = 150;
    rule->enabled = 1;
    
    /* 時間条件: 深夜時間帯（22:00-6:00） */
    rule->time_cond.type = CefC_Time_Condition_Schedule;
    rule->time_cond.weekdays = 0x7F;  // 全曜日
    rule->time_cond.start_hour = 22;
    rule->time_cond.end_hour = 6;
    
    /* 名前条件: バックアップコンテンツ */
    char* backup_pattern = "/backup/";
    rule->name_cond.name_pattern = (unsigned char*) malloc(strlen(backup_pattern));
    memcpy(rule->name_cond.name_pattern, backup_pattern, strlen(backup_pattern));
    rule->name_cond.pattern_len = strlen(backup_pattern);
    rule->name_cond.match_type = 0x02;  // 前方一致
    
    /* 送信元条件: 管理者のFace-ID */
    rule->source_cond.faceid = 10;
    
    cef_packet_control_rule_add(ctx, rule);
    
    printf("複合条件ルールを追加しました\n");
    printf("- 深夜時間帯 AND /backup/* AND Face-ID 10 → 転送\n");
}

/*--------------------------------------------------------------------------------------
    設定ファイル形式の例
----------------------------------------------------------------------------------------*/
void
example_config_file_format (void) {
    printf("\n=== 設定ファイル例 ===\n");
    printf("# パケット制御ルール設定ファイル\n");
    printf("# 形式: priority action condition_type [条件パラメータ]\n\n");
    
    printf("# 時間帯制御\n");
    printf("100 FORWARD TIME schedule weekdays=0x3E start_hour=9 end_hour=17\n\n");
    
    printf("# コンテンツ名制御\n");
    printf("255 FORWARD NAME pattern=/emergency/ match_type=prefix\n");
    printf("50 DROP NAME pattern=/video/ match_type=prefix CHUNK min=100 max=999\n\n");
    
    printf("# 送信元制御\n");
    printf("200 FORWARD SOURCE faceid=1\n");
    printf("10 DROP SOURCE faceid=99\n\n");
    
    printf("# 複合条件\n");
    printf("150 FORWARD TIME+NAME+SOURCE schedule weekdays=0x7F start_hour=22 end_hour=6 pattern=/backup/ faceid=10\n");
}

/*--------------------------------------------------------------------------------------
    メイン使用例
----------------------------------------------------------------------------------------*/
int
main_packet_control_example (void) {
    CefT_Packet_Control_Context* ctx;
    char stats_buffer[4096];
    
    /* コンテキスト初期化 */
    ctx = cef_packet_control_context_create();
    if (ctx == NULL) {
        printf("Failed to create packet control context\n");
        return (-1);
    }
    
    printf("=== パケットレベルFIB制御の使用例 ===\n\n");
    
    /* 各種制御例を追加 */
    example_time_based_control(ctx);
    example_content_based_control(ctx);
    example_source_based_control(ctx);
    example_custom_control(ctx);
    example_complex_control(ctx);
    
    /* 統計情報表示 */
    cef_packet_control_statistics_get(ctx, stats_buffer, sizeof(stats_buffer));
    printf("\n%s\n", stats_buffer);
    
    /* 設定ファイル形式例 */
    example_config_file_format();
    
    /* クリーンアップ */
    cef_packet_control_context_destroy(ctx);
    
    return (0);
} 