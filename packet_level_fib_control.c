/*
 * Packet-Level FIB Active/Inactive Control Implementation
 * パケットレベルでのFIBアクティブ・インアクティブ制御実装
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// #include "packet_level_fib_control.h"
// #include <cefore/cef_frame.h>  // Ceforeのフレーム処理用

/****************************************************************************************
 コンテキスト管理
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
    パケット制御コンテキストの初期化
----------------------------------------------------------------------------------------*/
CefT_Packet_Control_Context*
cef_packet_control_context_create (void) {
    CefT_Packet_Control_Context* ctx;
    
    ctx = (CefT_Packet_Control_Context*) malloc(sizeof(CefT_Packet_Control_Context));
    if (ctx == NULL) {
        return (NULL);
    }
    
    memset(ctx, 0, sizeof(CefT_Packet_Control_Context));
    ctx->default_action = CefC_Packet_Action_Forward;  // デフォルトは転送
    
    return (ctx);
}

/*--------------------------------------------------------------------------------------
    パケット制御コンテキストの破棄
----------------------------------------------------------------------------------------*/
void
cef_packet_control_context_destroy (
    CefT_Packet_Control_Context* ctx
) {
    CefT_Packet_Control_Rule* rule;
    CefT_Packet_Control_Rule* next_rule;
    
    if (ctx == NULL) {
        return;
    }
    
    /* 全ルールを削除 */
    rule = ctx->rules;
    while (rule) {
        next_rule = rule->next;
        
        /* 各条件のメモリを解放 */
        if (rule->name_cond.name_pattern) {
            free(rule->name_cond.name_pattern);
        }
        if (rule->source_cond.node_id) {
            free(rule->source_cond.node_id);
        }
        
        free(rule);
        rule = next_rule;
    }
    
    free(ctx);
}

/****************************************************************************************
 ルール管理
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
    制御ルールの追加
----------------------------------------------------------------------------------------*/
int
cef_packet_control_rule_add (
    CefT_Packet_Control_Context* ctx,
    CefT_Packet_Control_Rule* rule
) {
    CefT_Packet_Control_Rule* current;
    CefT_Packet_Control_Rule* prev = NULL;
    
    if (ctx == NULL || rule == NULL) {
        return (-1);
    }
    
    /* 優先度順に挿入（高い優先度が先頭） */
    current = ctx->rules;
    while (current && current->priority >= rule->priority) {
        prev = current;
        current = current->next;
    }
    
    rule->next = current;
    if (prev) {
        prev->next = rule;
    } else {
        ctx->rules = rule;
    }
    
    ctx->rule_count++;
    
#ifdef CefC_Debug
    cef_dbg_write(CefC_Dbg_Fine, 
        "[PacketControl] Added rule with priority %d (total: %d rules)\n", 
        rule->priority, ctx->rule_count);
#endif
    
    return (0);
}

/****************************************************************************************
 パケット制御判定のメイン処理
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
    パケットに対する制御判定（メイン関数）
----------------------------------------------------------------------------------------*/
uint8_t
cef_packet_control_evaluate (
    CefT_Packet_Control_Context* ctx,
    const unsigned char* msg,               /* パケットデータ */
    uint16_t payload_len,                   /* ペイロード長 */
    uint16_t header_len,                    /* ヘッダー長 */
    uint16_t source_faceid,                 /* 送信元Face-ID */
    const unsigned char* name,              /* コンテンツ名 */
    uint16_t name_len,                      /* コンテンツ名長 */
    uint32_t chunk_num,                     /* チャンク番号 */
    uint8_t chunk_num_f                     /* チャンク番号有効フラグ */
) {
    CefT_Packet_Control_Rule* rule;
    int match_result;
    struct timespec ts;
    uint64_t current_time;
    
    if (ctx == NULL) {
        return (CefC_Packet_Action_Forward);  // デフォルト動作
    }
    
    ctx->total_packets++;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    current_time = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    
    /* 優先度順にルールをチェック */
    rule = ctx->rules;
    while (rule) {
        if (!rule->enabled) {
            rule = rule->next;
            continue;
        }
        
        match_result = 1;  // マッチするまでTrue
        
        /* 名前条件チェック */
        if ((rule->condition_types & CefC_Condition_Type_Name) &&
            !cef_packet_name_pattern_match(name, name_len, &rule->name_cond)) {
            match_result = 0;
        }
        
        /* 時間条件チェック */
        if (match_result && 
            (rule->condition_types & CefC_Condition_Type_Time) &&
            !cef_packet_time_condition_check(&rule->time_cond)) {
            match_result = 0;
        }
        
        /* 送信元条件チェック */
        if (match_result && 
            (rule->condition_types & CefC_Condition_Type_Source) &&
            !cef_packet_source_condition_check(source_faceid, NULL, 0, &rule->source_cond)) {
            match_result = 0;
        }
        
        /* チャンク番号条件チェック */
        if (match_result && 
            (rule->condition_types & CefC_Condition_Type_ChunkNum) &&
            !cef_packet_chunk_condition_check(chunk_num, chunk_num_f, &rule->chunk_cond)) {
            match_result = 0;
        }
        
        /* カスタム条件チェック */
        if (match_result && 
            (rule->condition_types & CefC_Condition_Type_Custom) &&
            rule->custom_cond.func &&
            !rule->custom_cond.func(msg, payload_len, header_len, rule->custom_cond.user_data)) {
            match_result = 0;
        }
        
        /* ルールにマッチした場合 */
        if (match_result) {
            rule->match_count++;
            rule->last_match_time = current_time;
            ctx->processed_packets++;
            
#ifdef CefC_Debug
            cef_dbg_write(CefC_Dbg_Finest, 
                "[PacketControl] Rule matched, action: %d\n", rule->action);
#endif
            
            return (rule->action);
        }
        
        rule = rule->next;
    }
    
    /* どのルールにもマッチしなかった場合 */
    return (ctx->default_action);
}

/****************************************************************************************
 条件チェック関数
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
    名前パターンマッチング
----------------------------------------------------------------------------------------*/
int
cef_packet_name_pattern_match (
    const unsigned char* name,
    uint16_t name_len,
    const CefT_Name_Condition* cond
) {
    if (cond->name_pattern == NULL || cond->pattern_len == 0) {
        return (1);  // 条件なしは常にマッチ
    }
    
    switch (cond->match_type) {
        case 0x01:  /* 完全一致 */
            return (name_len == cond->pattern_len && 
                    memcmp(name, cond->name_pattern, name_len) == 0);
            
        case 0x02:  /* 前方一致 */
            return (name_len >= cond->pattern_len && 
                    memcmp(name, cond->name_pattern, cond->pattern_len) == 0);
            
        case 0x03:  /* 部分一致 */
            if (name_len < cond->pattern_len) {
                return (0);
            }
            /* 簡単な部分一致実装 */
            for (int i = 0; i <= name_len - cond->pattern_len; i++) {
                if (memcmp(&name[i], cond->name_pattern, cond->pattern_len) == 0) {
                    return (1);
                }
            }
            return (0);
            
        default:
            return (0);
    }
}

/*--------------------------------------------------------------------------------------
    時間条件チェック
----------------------------------------------------------------------------------------*/
int
cef_packet_time_condition_check (
    const CefT_Time_Condition* cond
) {
    struct timespec ts;
    time_t current_time;
    struct tm* tm_info;
    uint64_t current_usec;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    current_time = ts.tv_sec;
    current_usec = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    
    switch (cond->type) {
        case CefC_Time_Condition_Always:
            return (1);
            
        case CefC_Time_Condition_Period:
            return (current_usec >= cond->start_time && 
                    current_usec <= cond->end_time);
            
        case CefC_Time_Condition_Schedule:
            tm_info = localtime(&current_time);
            
            /* 曜日チェック */
            if (cond->weekdays != 0) {
                uint8_t weekday_bit = 1 << tm_info->tm_wday;
                if (!(cond->weekdays & weekday_bit)) {
                    return (0);
                }
            }
            
            /* 時間帯チェック */
            if (cond->start_hour != cond->end_hour) {
                if (cond->start_hour < cond->end_hour) {
                    return (tm_info->tm_hour >= cond->start_hour && 
                            tm_info->tm_hour < cond->end_hour);
                } else {
                    /* 日をまたぐ場合 */
                    return (tm_info->tm_hour >= cond->start_hour || 
                            tm_info->tm_hour < cond->end_hour);
                }
            }
            return (1);
            
        case CefC_Time_Condition_Interval:
            /* 間隔チェック（簡単な実装） */
            return ((current_time % cond->interval_sec) == 0);
            
        default:
            return (0);
    }
}

/*--------------------------------------------------------------------------------------
    送信元条件チェック
----------------------------------------------------------------------------------------*/
int
cef_packet_source_condition_check (
    uint16_t faceid,
    const unsigned char* node_id,
    uint16_t node_id_len,
    const CefT_Source_Condition* cond
) {
    /* Face-IDチェック */
    if (cond->faceid != 0 && cond->faceid != faceid) {
        return (0);
    }
    
    /* ノードIDチェック */
    if (cond->node_id && cond->node_id_len > 0) {
        if (node_id == NULL || node_id_len != cond->node_id_len) {
            return (0);
        }
        if (memcmp(node_id, cond->node_id, node_id_len) != 0) {
            return (0);
        }
    }
    
    return (1);
}

/*--------------------------------------------------------------------------------------
    チャンク番号条件チェック
----------------------------------------------------------------------------------------*/
int
cef_packet_chunk_condition_check (
    uint32_t chunk_num,
    uint8_t chunk_num_f,
    const CefT_ChunkNum_Condition* cond
) {
    /* チャンク番号が存在しない場合 */
    if (!chunk_num_f) {
        return (cond->min_chunk == 0 && cond->max_chunk == 0);
    }
    
    /* 範囲チェック */
    if (cond->max_chunk > 0) {
        return (chunk_num >= cond->min_chunk && 
                chunk_num <= cond->max_chunk);
    } else {
        return (chunk_num >= cond->min_chunk);
    }
}

/****************************************************************************************
 統計情報とユーティリティ
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
    制御ルールの統計情報取得
----------------------------------------------------------------------------------------*/
void
cef_packet_control_statistics_get (
    CefT_Packet_Control_Context* ctx,
    char* info_buff,
    int buff_size
) {
    CefT_Packet_Control_Rule* rule;
    int offset = 0;
    int rule_id = 0;
    
    if (ctx == NULL || info_buff == NULL) {
        return;
    }
    
    offset += snprintf(&info_buff[offset], buff_size - offset,
        "PacketControl Statistics:\n");
    offset += snprintf(&info_buff[offset], buff_size - offset,
        "  Total Packets: %lu\n", ctx->total_packets);
    offset += snprintf(&info_buff[offset], buff_size - offset,
        "  Processed Packets: %lu\n", ctx->processed_packets);
    offset += snprintf(&info_buff[offset], buff_size - offset,
        "  Total Rules: %u\n", ctx->rule_count);
    offset += snprintf(&info_buff[offset], buff_size - offset,
        "  Default Action: %s\n",
        (ctx->default_action == CefC_Packet_Action_Forward) ? "FORWARD" :
        (ctx->default_action == CefC_Packet_Action_Drop) ? "DROP" : "OTHER");
    
    offset += snprintf(&info_buff[offset], buff_size - offset,
        "\nRule Details:\n");
    
    rule = ctx->rules;
    while (rule && offset < buff_size - 100) {
        offset += snprintf(&info_buff[offset], buff_size - offset,
            "  Rule %d: Priority=%d, Enabled=%s, Matches=%lu\n",
            rule_id++, rule->priority, 
            rule->enabled ? "YES" : "NO", 
            rule->match_count);
        rule = rule->next;
    }
}

/****************************************************************************************
 cefnetdへの統合用インターフェース
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
    cefnetdでの利用例
----------------------------------------------------------------------------------------*/
static int
cefnetd_incoming_interest_process_with_packet_control (
    CefT_Netd_Handle* hdl,                  /* cefnetd handle */
    int faceid,                             /* Face-ID where messages arrived at */
    int peer_faceid,                        /* Face-ID to reply to origin */
    unsigned char* msg,                     /* received message to handle */
    uint16_t payload_len,                   /* Payload Length */
    uint16_t header_len,                    /* Header Length */
    char* user_id
) {
    CefT_CcnMsg_MsgBdy pm = {0};
    CefT_CcnMsg_OptHdr poh = {0};
    CefT_Fib_Entry* fe = NULL;
    CefT_Pit_Entry* pe = NULL;
    uint16_t faceids[CefC_Fib_UpFace_Max];
    uint16_t name_len;
    int face_num = 0;
    int res;
    uint8_t packet_action;
    uint32_t chunk_num = 0;
    uint8_t chunk_num_f = 0;
    
    /* メッセージを解析 */
    res = cef_frame_message_parse(
        msg, payload_len, header_len, &poh, &pm, CefC_PT_INTEREST);
    if (res < 0) {
        return (-1);
    }
    
    /* Interest名の長さを決定 */
    if (pm.chunk_num_f) {
        name_len = pm.name_len - (CefC_S_Type + CefC_S_Length + CefC_S_ChunkNum);
        chunk_num = pm.chunk_num;
        chunk_num_f = 1;
    } else {
        name_len = pm.name_len;
    }
    
    /* **重要**: パケットレベルでの制御判定 */
    packet_action = cef_packet_control_evaluate(
        hdl->packet_ctrl_ctx,               /* パケット制御コンテキスト */
        msg, payload_len, header_len,       /* パケットデータ */
        peer_faceid,                        /* 送信元Face-ID */
        pm.name, name_len,                  /* コンテンツ名 */
        chunk_num, chunk_num_f              /* チャンク番号 */
    );
    
    /* パケット制御の結果に応じて処理 */
    switch (packet_action) {
        case CefC_Packet_Action_Drop:
            /* パケットをドロップ */
#ifdef CefC_Debug
            cef_dbg_write(CefC_Dbg_Fine, 
                "[PacketControl] Interest dropped by packet control\n");
#endif
            return (0);
            
        case CefC_Packet_Action_Return:
            /* Interest Returnを送信 */
            cefnetd_interest_return_send(hdl, peer_faceid, &pm, 
                CefC_CtRc_NO_ROUTE, msg, payload_len, header_len);
            return (0);
            
        case CefC_Packet_Action_Queue:
            /* キューイング処理（実装は省略） */
            cefnetd_interest_queue_add(hdl, msg, payload_len, header_len, peer_faceid);
            return (0);
            
        case CefC_Packet_Action_Forward:
        default:
            /* 通常の転送処理を継続 */
            break;
    }
    
    /* 従来のFIB検索処理 */
    fe = cef_fib_entry_search(hdl->fib, pm.name, name_len);
    
    if (fe) {
        pe = cef_pit_entry_lookup_and_down_face_update(
            hdl->pit, &pm, &poh, peer_faceid, msg, hdl->InterestRetrans, &res);
        
        if (pe) {
            face_num = cef_fib_forward_faceid_select(fe, peer_faceid, faceids);
            
            if (face_num > 0) {
                cefnetd_interest_forward(
                    hdl, faceids, face_num, peer_faceid, msg,
                    payload_len, header_len, &pm, &poh, pe, fe);
            } else {
                cefnetd_interest_return_send(hdl, peer_faceid, &pm, 
                    CefC_CtRc_NO_ROUTE, msg, payload_len, header_len);
            }
        }
    } else {
        cefnetd_interest_return_send(hdl, peer_faceid, &pm, 
            CefC_CtRc_NO_ROUTE, msg, payload_len, header_len);
    }
    
    return (1);
} 