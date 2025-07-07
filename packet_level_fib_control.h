/*
 * Packet-Level FIB Active/Inactive Control Extension
 * パケットレベルでのFIBアクティブ・インアクティブ制御機能
 */

#ifndef __PACKET_LEVEL_FIB_CONTROL_H__
#define __PACKET_LEVEL_FIB_CONTROL_H__

#include <stdint.h>
#include <time.h>

/****************************************************************************************
 パケットレベル制御の定義
 ****************************************************************************************/

/* パケット処理結果 */
#define CefC_Packet_Action_Forward      0x01    /* 転送実行 */
#define CefC_Packet_Action_Drop         0x02    /* パケットドロップ */
#define CefC_Packet_Action_Return       0x04    /* Interest Return送信 */
#define CefC_Packet_Action_Queue        0x08    /* キューイング（遅延処理） */

/* パケット制御条件タイプ */
#define CefC_Condition_Type_Name        0x01    /* コンテンツ名による条件 */
#define CefC_Condition_Type_Time        0x02    /* 時間による条件 */
#define CefC_Condition_Type_Source      0x04    /* 送信元による条件 */
#define CefC_Condition_Type_Content     0x08    /* コンテンツタイプによる条件 */
#define CefC_Condition_Type_ChunkNum    0x10    /* チャンク番号による条件 */
#define CefC_Condition_Type_Custom      0x20    /* カスタム条件 */

/* 時間条件タイプ */
#define CefC_Time_Condition_Always      0x00    /* 常時 */
#define CefC_Time_Condition_Period      0x01    /* 期間指定 */
#define CefC_Time_Condition_Schedule    0x02    /* スケジュール指定 */
#define CefC_Time_Condition_Interval    0x04    /* 間隔指定 */

/****************************************************************************************
 パケット制御条件の構造体
 ****************************************************************************************/

/* 名前条件 */
typedef struct {
    unsigned char*  name_pattern;       /* 名前パターン */
    uint16_t        pattern_len;        /* パターン長 */
    uint8_t         match_type;         /* マッチタイプ（完全一致/前方一致/部分一致） */
} CefT_Name_Condition;

/* 時間条件 */
typedef struct {
    uint8_t         type;               /* 時間条件タイプ */
    uint64_t        start_time;         /* 開始時刻 */
    uint64_t        end_time;           /* 終了時刻 */
    uint32_t        interval_sec;       /* 間隔（秒） */
    uint8_t         weekdays;           /* 曜日マスク（bit0=日曜） */
    uint8_t         start_hour;         /* 開始時刻（時） */
    uint8_t         end_hour;           /* 終了時刻（時） */
} CefT_Time_Condition;

/* 送信元条件 */
typedef struct {
    uint16_t        faceid;             /* 送信元Face-ID */
    unsigned char*  node_id;            /* 送信元ノードID */
    uint16_t        node_id_len;        /* ノードID長 */
} CefT_Source_Condition;

/* チャンク番号条件 */
typedef struct {
    uint32_t        min_chunk;          /* 最小チャンク番号 */
    uint32_t        max_chunk;          /* 最大チャンク番号 */
    uint8_t         chunk_type;         /* チャンクタイプ */
} CefT_ChunkNum_Condition;

/* カスタム条件（コールバック関数） */
typedef int (*CefT_Custom_Condition_Func)(
    const unsigned char* msg,           /* パケットデータ */
    uint16_t payload_len,               /* ペイロード長 */
    uint16_t header_len,                /* ヘッダー長 */
    void* user_data                     /* ユーザーデータ */
);

typedef struct {
    CefT_Custom_Condition_Func func;   /* 判定関数 */
    void* user_data;                    /* ユーザーデータ */
} CefT_Custom_Condition;

/* 統合制御条件 */
typedef struct CefT_Packet_Control_Rule {
    uint8_t                     condition_types;    /* 条件タイプビットマスク */
    uint8_t                     action;             /* 実行アクション */
    uint8_t                     priority;           /* 優先度（高い数値が優先） */
    uint8_t                     enabled;            /* 有効/無効フラグ */
    
    /* 各種条件 */
    CefT_Name_Condition         name_cond;
    CefT_Time_Condition         time_cond;
    CefT_Source_Condition       source_cond;
    CefT_ChunkNum_Condition     chunk_cond;
    CefT_Custom_Condition       custom_cond;
    
    /* 統計情報 */
    uint64_t                    match_count;        /* マッチ回数 */
    uint64_t                    last_match_time;    /* 最終マッチ時刻 */
    
    /* リンク */
    struct CefT_Packet_Control_Rule* next;
} CefT_Packet_Control_Rule;

/* パケット制御コンテキスト */
typedef struct {
    CefT_Packet_Control_Rule*   rules;             /* ルールリスト */
    uint32_t                    rule_count;        /* ルール数 */
    uint8_t                     default_action;    /* デフォルトアクション */
    uint64_t                    total_packets;     /* 総パケット数 */
    uint64_t                    processed_packets; /* 処理済みパケット数 */
} CefT_Packet_Control_Context;

/****************************************************************************************
 関数宣言
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
    パケット制御コンテキストの初期化
----------------------------------------------------------------------------------------*/
CefT_Packet_Control_Context*
cef_packet_control_context_create (void);

/*--------------------------------------------------------------------------------------
    パケット制御コンテキストの破棄
----------------------------------------------------------------------------------------*/
void
cef_packet_control_context_destroy (
    CefT_Packet_Control_Context* ctx
);

/*--------------------------------------------------------------------------------------
    制御ルールの追加
----------------------------------------------------------------------------------------*/
int
cef_packet_control_rule_add (
    CefT_Packet_Control_Context* ctx,
    CefT_Packet_Control_Rule* rule
);

/*--------------------------------------------------------------------------------------
    制御ルールの削除
----------------------------------------------------------------------------------------*/
int
cef_packet_control_rule_remove (
    CefT_Packet_Control_Context* ctx,
    uint32_t rule_id
);

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
);

/*--------------------------------------------------------------------------------------
    名前パターンマッチング
----------------------------------------------------------------------------------------*/
int
cef_packet_name_pattern_match (
    const unsigned char* name,
    uint16_t name_len,
    const CefT_Name_Condition* cond
);

/*--------------------------------------------------------------------------------------
    時間条件チェック
----------------------------------------------------------------------------------------*/
int
cef_packet_time_condition_check (
    const CefT_Time_Condition* cond
);

/*--------------------------------------------------------------------------------------
    送信元条件チェック
----------------------------------------------------------------------------------------*/
int
cef_packet_source_condition_check (
    uint16_t faceid,
    const unsigned char* node_id,
    uint16_t node_id_len,
    const CefT_Source_Condition* cond
);

/*--------------------------------------------------------------------------------------
    チャンク番号条件チェック
----------------------------------------------------------------------------------------*/
int
cef_packet_chunk_condition_check (
    uint32_t chunk_num,
    uint8_t chunk_num_f,
    const CefT_ChunkNum_Condition* cond
);

/*--------------------------------------------------------------------------------------
    制御ルールの統計情報取得
----------------------------------------------------------------------------------------*/
void
cef_packet_control_statistics_get (
    CefT_Packet_Control_Context* ctx,
    char* info_buff,
    int buff_size
);

/*--------------------------------------------------------------------------------------
    設定ファイルからルールを読み込み
----------------------------------------------------------------------------------------*/
int
cef_packet_control_config_load (
    CefT_Packet_Control_Context* ctx,
    const char* config_file_path
);

/*--------------------------------------------------------------------------------------
    ルールの動的追加（管理インターフェース用）
----------------------------------------------------------------------------------------*/
int
cef_packet_control_rule_add_by_string (
    CefT_Packet_Control_Context* ctx,
    const char* rule_string
);

#endif // __PACKET_LEVEL_FIB_CONTROL_H__ 